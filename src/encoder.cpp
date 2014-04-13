#include "pidmap.h"
#include "encoder.h"
#include "demuxer.h"
#include "util.h"

#include <string>
using std::string;

#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <dirent.h>
#include <string.h>

Encoder::Encoder(const PidMap &pids_in, string frame_size, string bitrate,
		string profile, string level, string bframes) throw(string)
{
	PidMap::const_iterator	it;
	PidMap::iterator		it2;
	PidMap::const_iterator	pmt, video, audio;
	string	encoder_device;
	int		encoder;
	int		tbr;
	int		bf;
	DIR				*dir;
	struct	dirent	*dire;

	start_thread_running	= false;
	start_thread_joined		= true;
	stopped					= false;

	for(it = pids_in.begin(); it != pids_in.end(); it++)
	{
		if((it->first != "pat") && (it->first != "pmt") &&
				(it->first != "audio") && (it->first != "video"))
			continue;

		for(it2 = pids.begin(); it2 != pids.end(); it2++)
			if(it2->second == it->second)
				break;

		if((it2 != pids.end()) && (it->first == "video"))
			pids.erase(it2);

		if((it2 = pids.find(it->first)) != pids.end())
			pids.erase(it2);

		if(it->first == "pmt")
			pmt = it;

		if(it->first == "video")
			video = it;

		if(it->first == "audio")
			audio = it;

		pids[it->first] = it->second;
	}

	if((pmt == pids.end()) || (video == pids.end()) || (audio == pids.end()))
		throw(string("Encoder: missing pmt, video or audio pid"));

	model = model_vuplus_solo2;

	if((dir = opendir("/dev")))
	{
		while((dire = readdir(dir)))
		{
			if(!strcmp(dire->d_name, "bcm_enc1"))
			{
				model = model_vuplus_duo2;
				break;
			}
		}

		closedir(dir);
	}

	Util::vlog("Encoder: probed model: %s", model_id().c_str());

	switch(model)
	{
		case(model_vuplus_solo2):
		{
			id = 0;

			tbr = Util::string_to_int(bitrate);

			if((tbr >= 100) && (tbr <= 10000))
				tbr *= 1000;
			else
				tbr = 1000000;

			//setprop("bitrate", Util::int_to_string(tbr));

			if((fd = open("/dev/bcm_enc0", O_RDWR, 0)) < 0)
				throw(string("Encoder: cannot open encoder"));

			break;
		}

		case(model_vuplus_duo2):
		{
			for(encoder = 0; encoder < 3; encoder++)
			{
				encoder_device = string("/dev/bcm_enc") + Util::int_to_string(encoder);
				errno = 0;
				Util::vlog("Encoder: open encoder %s", encoder_device.c_str());

				if((fd = open(encoder_device.c_str(), O_RDWR, 0)) < 0)
				{
					if(errno == ENOENT)
					{
						Util::vlog("Encoder: not found %s", encoder_device.c_str());
						break;
					}

					Util::vlog("Encoder: encoder %d is busy", encoder);
				}
				else
					break;
			}

			if(fd < 0)
				throw(string("Encoder: cannot open encoder"));

			id = encoder;

			if((frame_size == "480p") || (frame_size == "576p") ||
					(frame_size == "720p"))
				setprop("display_format", frame_size);
			else
				setprop("display_format", "480p");

			tbr = Util::string_to_int(bitrate);

			if((tbr >= 100) && (tbr <= 10000))
				tbr *= 1000;
			else
				tbr = 1000000;

			setprop("bitrate", Util::int_to_string(tbr));

			if((profile == "baseline") || (profile == "main") || (profile == "high"))
				setprop("profile", profile);
			else
				setprop("profile", "baseline");

			if((level == "3.1") || (level == "3.2") || (level == "4.0"))
				setprop("level", level);
			else
				setprop("level", "3.1");

			bf = Util::string_to_int(bframes);

			if((bf >= 0) && (bf <= 2))
				setprop("gop_frameb", Util::int_to_string(bf));
			else
				setprop("gop_frameb", "0");

			break;
		}

		default:
		{
			throw(string("Encoder: unknown model"));
		}
	}

	if(ioctl(fd, IOCTL_VUPLUS_SET_PMTPID, pmt->second) ||
			ioctl(fd, IOCTL_VUPLUS_SET_VPID, video->second) ||
			ioctl(fd, IOCTL_VUPLUS_SET_APID, audio->second))
	{
			throw(string("Encoder: cannot init encoder"));
	}
}

Encoder::~Encoder() throw()
{
	stop();
	close(fd);
}

void* Encoder::start_thread_function(void * arg)
{
	Encoder *_this = (Encoder *)arg;

	//Util::vlog("Encoder: start thread: start ioctl");

	_this->start_thread_joined	= false;
	_this->start_thread_running	= true;
	_this->stopped				= false;

	if(ioctl(_this->fd, IOCTL_VUPLUS_START_TRANSCODING, 0))
		Util::vlog("Encoder: IOCTL start transcoding");

	_this->start_thread_running = false;

	return((void *)0);
}

bool Encoder::start_init() throw()
{
	//Util::vlog("Encoder: START TRANSCODING start");

	if(start_thread_running || !start_thread_joined)
	{
		Util::vlog("Encoder: start thread already running");
		return(true);
	}

	if(pthread_create(&start_thread, 0, &start_thread_function, (void *)this))
	{
		Util::vlog("Encoder: pthread create failed");
		return(false);
	}

	//Util::vlog("Encoder: START TRANSCODING done");
	return(true);
}

bool Encoder::start_finish() throw()
{
	if(!start_thread_running && !start_thread_joined)
	{
		//Util::vlog("Encoder: START detects start thread stopped");
		pthread_join(start_thread, 0);
		//Util::vlog("Encoder: START joined start thread");
		start_thread_joined = true;

		return(true);
	}

	return(false);
}

bool Encoder::stop() throw()
{
	struct pollfd pfd;
	static char buffer[4096];
	ssize_t rv;

	//Util::vlog("Encoder: STOP TRANSCODING begin");

	if(stopped)
	{
		Util::vlog("Encoder: already stopped");
		return(true);
	}

	if(start_thread_running)
	{
		start_thread_running = false;
		start_thread_joined = false;
	}

	if(!start_finish())
		return(false);

	if(ioctl(fd, IOCTL_VUPLUS_STOP_TRANSCODING, 0))
		Util::vlog("Encoder: IOCTL stop transcoding");

	Util::vlog("Encoder: starting draining");

	for(;;)
	{
		pfd.fd = fd;
		pfd.events = POLLIN;

		if((rv = poll(&pfd, 1, 10)) == 0)
			break;

		if(rv < 0)
		{
			Util::vlog("Encoder: poll error");
			break;
		}

		if(pfd.revents & POLLIN)
		{
			rv = read(fd, buffer, sizeof(buffer));
			Util::vlog("Encoder: STOP, drained %d bytes", rv);

			if(rv <= 0)
				break;
		}
		else
			break;
	}

	stopped = true;

	Util::vlog("Encoder: encoder STOP TRANSCODING done");

	return(true);
}

int Encoder::getfd() const throw()
{
	return(fd);
}

PidMap Encoder::getpids() const throw()
{
	return(pids);
}

string Encoder::getprop(string property) const throw()
{
	int		procfd;
	ssize_t	rv;
	string	path;
	char	tmp[256];

	path = string("/proc/stb/encoder/") + Util::int_to_string(id) + "/" + property;

	if((procfd = open(path.c_str(), O_RDONLY, 0)) < 0)
	{
		Util::vlog("Encoder::getprop: cannot open property %s", path.c_str());
		return("");
	}

	if((rv = read(procfd, tmp, sizeof(tmp))) <= 0)
	{
		Util::vlog("Encoder::getprop: cannot read from property %s", property.c_str());
		rv = 0;
	}

	tmp[rv] = '\0';

	close(procfd);

	return(tmp);
}

void Encoder::setprop(string property, string value) const throw()
{
	int		procfd;
	string	path;

	Util::vlog("setprop");

	return;

	path = string("/proc/stb/encoder/") + Util::int_to_string(id) + "/" + property;

	if((procfd = open(path.c_str(), O_WRONLY, 0)) < 0)
	{
		Util::vlog("Encoder::setprop: cannot open property %s", path.c_str());
		return;
	}

	if(write(procfd, value.c_str(), value.length()) != (ssize_t)value.length())
		Util::vlog("Encoder::getprop: cannot write to property %s", property.c_str());

	close(procfd);
}

string Encoder::model_id() const throw()
{
	static const char *models[] = { "undef", "VU+ Solo2", "VU+ Duo2" };

	if((model > model_undef) && (model < model_end))
		return(models[model]);
	else
		return("unknown");

}
