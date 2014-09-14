#include "config.h"
#include "trap.h"

#include "types.h"
#include "encoder.h"
#include "demuxer.h"
#include "util.h"
#include "stbtraits.h"

#include <string>
using std::string;

#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>

Encoder::Encoder(const PidMap &pids_in,
		const stb_traits_t &stb_traits_in,
		const StreamingParameters &streaming_parameters_in) throw(trap)
	:
		streaming_parameters(streaming_parameters_in),
		stb_traits(stb_traits_in)
{
	size_t					feature_index;
	const stb_feature_t		*feature = 0;
	string					value;
	int						int_value;
	int						pmt = -1, video = -1, audio = -1;
	int						attempt;

	start_thread_running	= false;
	start_thread_joined		= true;
	stopped					= false;

	for(PidMap::const_iterator it(pids_in.begin()); it != pids_in.end(); it++)
	{
		if((it->first != "pat") && (it->first != "pmt") &&
				(it->first != "audio") && (it->first != "video"))
			continue;

		PidMap::iterator it2;

		for(it2 = pids.begin(); it2 != pids.end(); it2++)
			if(it2->second == it->second)
				break;

		if((it2 != pids.end()) && (it->first == "video"))
			pids.erase(it2);
		if((it2 = pids.find(it->first)) != pids.end())
			pids.erase(it2);

		if(it->first == "pmt")
			pmt = it->second;

		if(it->first == "video")
			video = it->second;

		if(it->first == "audio")
			audio = it->second;

		pids[it->first] = it->second;
	}

	if((pmt == -1) || (video == -1) || (audio == -1))
		throw(trap("Encoder: missing pmt, video or audio pid"));

	for(StreamingParameters::const_iterator it(streaming_parameters.begin()); it != streaming_parameters.end(); it++)
	{
		for(feature_index = 0; feature_index < stb_traits.num_features; feature_index++)
		{
			feature = &stb_traits.features[feature_index];

			if(it->first == feature->id)
				break;
		}

		if(feature_index >= stb_traits.num_features)
		{
			Util::vlog("Encoder: no stb traits/feature entry for streaming parameter \"%s\"", it->first.c_str());
			continue;
		}

		Util::vlog("Encoder: found streaming parameter == stb_feature: \"%s\" [%s]", it->first.c_str(), it->second.c_str());

		if(!feature->settable)
		{
			Util::vlog("Encoder: feature not settable, skip");
			continue;
		}

		switch(feature->type)
		{
			case(stb_traits_type_bool):
			{
				if((it->second == "off") || (it->second == "false") || (it->second == "0"))
					value = "off";
				else
					if((it->second == "on") || (it->second == "true") || (it->second == "1"))
						value = "on";
					else
					{
						Util::vlog("Encoder: invalid bool value: \"\%s\"", it->second.c_str());
						continue;
					}

				break;
			}

			case(stb_traits_type_int):
			{
				int_value = strtol(it->second.c_str(), 0, 0);

				if(int_value < feature->value.int_type.min_value)
				{
					Util::vlog("Encoder: integer value %s too small (%d)",
							it->second.c_str(), feature->value.int_type.min_value);
					continue;
				}

				if(int_value > feature->value.int_type.max_value)
				{
					Util::vlog("Encoder: integer value %s too large (%d)",
							it->second.c_str(), feature->value.int_type.max_value);
					continue;
				}

				int_value *= feature->value.int_type.scaling_factor;
				value = Util::int_to_string(int_value);

				break;
			}

			case(stb_traits_type_string):
			{
				if(it->second.length() < feature->value.string_type.min_length)
				{
					Util::vlog("Encoder: string value %s too short (%d)",
							it->second.c_str(), feature->value.string_type.min_length);
					continue;
				}

				if(it->second.length() > feature->value.string_type.max_length)
				{
					Util::vlog("Encoder: string value %s too long (%d)",
							it->second.c_str(), feature->value.string_type.max_length);
					continue;
				}

				value = it->second;

				break;
			}

			case(stb_traits_type_string_enum):
			{
				const char * const *enum_value;

				for(enum_value = feature->value.string_enum_type.enum_values; *enum_value != 0; enum_value++)
					if(it->second == *enum_value)
						break;

				if(!*enum_value)
				{
					Util::vlog("Encoder: invalid enum value: \"%s\"", it->second.c_str());
					continue;
				}

				value = *enum_value;

				break;
			}

			default:
			{
				throw(trap("Encoder: unknown feature type"));
			}
		}

		setprop(feature->api_data, value);
	}

	// try multiples times with a little delay, as often multiple
	// requests are sent from a single http client, resulting in multiple
	// streamproxy threads, all of them having the encoder open
	// for a short while

	fd = -1;

	if(stb_traits.encoders > 0)
	{
		for(attempt = 0; attempt < 32; attempt++)
		{
			if((fd = open("/dev/bcm_enc0", O_RDWR, 0)) >= 0)
			{
				Util::vlog("Encoder: bcm_enc0 open");
				break;
			}

			Util::vlog("Encoder: waiting for encoder 0 to become available, attempt %d", attempt);

			usleep(100000);
		}
	}

	if((stb_traits.encoders > 1) && (fd < 0))
	{
		for(attempt = 0; attempt < 32; attempt++)
		{
			if((fd = open("/dev/bcm_enc1", O_RDWR, 0)) >= 0)
			{
				Util::vlog("Encoder: bcm_enc1 open");
				break;
			}

			Util::vlog("Encoder: waiting for encoder 1 to become available, attempt %d", attempt);

			usleep(100000);
		}
	}

	if(fd < 0)
		throw(trap("no encoders available"));

	Util::vlog("pmt: %d", pmt);
	Util::vlog("video: %d", video);
	Util::vlog("audio: %d", audio);
	Util::vlog("start ioctl");

	if(ioctl(fd, IOCTL_VUPLUS_SET_PMTPID, pmt) ||
			ioctl(fd, IOCTL_VUPLUS_SET_VPID, video) ||
			ioctl(fd, IOCTL_VUPLUS_SET_APID, audio))
	{
			throw(trap("Encoder: cannot init encoder"));
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

void Encoder::setprop(const string &property, const string &value) const throw()
{
	int		procfd;
	string	path;

	Util::vlog("setprop: %s=%s", property.c_str(), value.c_str());

	path = string("/proc/stb/encoder/") + Util::int_to_string(id) + "/" + property;

	if((procfd = open(path.c_str(), O_WRONLY, 0)) < 0)
	{
		Util::vlog("Encoder::setprop: cannot open property %s", path.c_str());
		return;
	}

	if(write(procfd, value.c_str(), value.length()) != (ssize_t)value.length())
		Util::vlog("Encoder::setprop: cannot write to property %s", property.c_str());

	close(procfd);
}
