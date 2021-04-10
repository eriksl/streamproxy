#include "config.h"
#include "trap.h"

#include "types.h"
#include "encoder-broadcom.h"
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

EncoderBroadcom::EncoderBroadcom(const PidMap &pids_in,
		const stb_traits_t &stb_traits_in,
		const StreamingParameters &streaming_parameters_in)
	:
		streaming_parameters(streaming_parameters_in),
		stb_traits(stb_traits_in)
{
	size_t					feature_index;
	const stb_feature_t		*feature = 0;
	string					value;
	int						int_value;
	int						pmt = -1, video = -1, audio = -1, default_audio_pid = -1;
	int						attempt;
	int						ioctl_set_pmtpid;
	int						ioctl_set_vpid;
	int						ioctl_set_apid;

	fd						= -1;
	encoder					= -1;
	start_thread_running	= false;
	start_thread_joined		= true;
	stopped					= false;

	for(PidMap::const_iterator it(pids_in.begin()); it != pids_in.end(); it++)
	{
		if (it->first == "default_audio_pid")
			default_audio_pid = it->second;

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

	if (default_audio_pid != -1)
		pids["audio"] = audio = default_audio_pid;

	if((pmt == -1) || (video == -1) || (audio == -1))
		throw(trap("EncoderBroadcom: missing pmt, video or audio pid"));

	// try multiples times with a little delay, as often multiple
	// requests are sent from a single http client, resulting in multiple
	// streamproxy threads, all of them having the encoder open
	// for a short while

	if(stb_traits.encoders > 0)
	{
		for(attempt = 0; attempt < 64; attempt++)
		{
			if((fd = open("/dev/bcm_enc0", O_RDWR, 0)) >= 0)
			{
				encoder = 0;
				Util::vlog("EncoderBroadcom: bcm_enc0 open");
				break;
			}

			Util::vlog("EncoderBroadcom: waiting for encoder 0 to become available, attempt %d", attempt);

			usleep(100000);
		}
	}

	if((stb_traits.encoders > 1) && (encoder < 0))
	{
		for(attempt = 0; attempt < 64; attempt++)
		{
			if((fd = open("/dev/bcm_enc1", O_RDWR, 0)) >= 0)
			{
				encoder = 1;
				Util::vlog("EncoderBroadcom: bcm_enc1 open");
				break;
			}

			Util::vlog("EncoderBroadcom: waiting for encoder 1 to become available, attempt %d", attempt);

			usleep(100000);
		}
	}

	if(encoder < 0)
		throw(trap("no encoders available"));

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
			Util::vlog("EncoderBroadcom: no stb traits/feature entry for streaming parameter \"%s\"", it->first.c_str());
			continue;
		}

		Util::vlog("EncoderBroadcom: found streaming parameter == stb_feature: \"%s\" [%s]", it->first.c_str(), it->second.c_str());

		if(!feature->settable)
		{
			Util::vlog("EncoderBroadcom: feature not settable, skip");
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
						Util::vlog("EncoderBroadcom: invalid bool value: \"\%s\"", it->second.c_str());
						continue;
					}

				break;
			}

			case(stb_traits_type_int):
			{
				int_value = strtol(it->second.c_str(), 0, 0);

				if(int_value < feature->value.int_type.min_value)
				{
					Util::vlog("EncoderBroadcom: integer value %s too small (%d)",
							it->second.c_str(), feature->value.int_type.min_value);
					continue;
				}

				if(int_value > feature->value.int_type.max_value)
				{
					Util::vlog("EncoderBroadcom: integer value %s too large (%d)",
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
					Util::vlog("EncoderBroadcom: string value %s too short (%d)",
							it->second.c_str(), feature->value.string_type.min_length);
					continue;
				}

				if(it->second.length() > feature->value.string_type.max_length)
				{
					Util::vlog("EncoderBroadcom: string value %s too long (%d)",
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
					Util::vlog("EncoderBroadcom: invalid enum value: \"%s\"", it->second.c_str());
					continue;
				}

				value = *enum_value;

				break;
			}

			default:
			{
				throw(trap("EncoderBroadcom: unknown feature type"));
			}
		}

		setprop(feature->api_data, value);
	}

	Util::vlog("pmt: %d", pmt);
	Util::vlog("video: %d", video);
	Util::vlog("audio: %d", audio);
	Util::vlog("start ioctl");

	if(stb_traits.quirks & stb_quirk_bcm_mips)
	{
		ioctl_set_pmtpid = 3;
		ioctl_set_vpid = 1;
		ioctl_set_apid = 2;
	}
	else
	{
		ioctl_set_pmtpid = 13;
		ioctl_set_vpid = 11;
		ioctl_set_apid = 12;
	}

	if(ioctl(fd, ioctl_set_pmtpid, pmt) ||
			ioctl(fd, ioctl_set_vpid, video) ||
			ioctl(fd, ioctl_set_apid, audio))
	{
			throw(trap("EncoderBroadcom: cannot init encoder"));
	}
}

EncoderBroadcom::~EncoderBroadcom()
{
	stop();
	close(fd);
}

void* EncoderBroadcom::start_thread_function(void * arg)
{
	EncoderBroadcom *_this = (EncoderBroadcom *)arg;

	//Util::vlog("EncoderBroadcom: start thread: start ioctl");

	_this->start_thread_joined	= false;
	_this->start_thread_running	= true;
	_this->stopped				= false;

	if(ioctl(_this->fd, IOCTL_BROADCOM_START_TRANSCODING, 0))
		Util::vlog("EncoderBroadcom: IOCTL start transcoding");

	_this->start_thread_running = false;

	return((void *)0);
}

bool EncoderBroadcom::start_init()
{
	//Util::vlog("EncoderBroadcom: START TRANSCODING start");

	if(start_thread_running || !start_thread_joined)
	{
		Util::vlog("EncoderBroadcom: start thread already running");
		return(true);
	}

	if(pthread_create(&start_thread, 0, &start_thread_function, (void *)this))
	{
		Util::vlog("EncoderBroadcom: pthread create failed");
		return(false);
	}

	//Util::vlog("EncoderBroadcom: START TRANSCODING done");
	return(true);
}

bool EncoderBroadcom::start_finish()
{
	if(!start_thread_running && !start_thread_joined)
	{
		//Util::vlog("EncoderBroadcom: START detects start thread stopped");
		pthread_join(start_thread, 0);
		//Util::vlog("EncoderBroadcom: START joined start thread");
		start_thread_joined = true;

		return(true);
	}

	return(false);
}

bool EncoderBroadcom::stop()
{
	struct pollfd pfd;
	static char buffer[4096];
	ssize_t rv;

	//Util::vlog("EncoderBroadcom: STOP TRANSCODING begin");

	if(stopped)
	{
		Util::vlog("EncoderBroadcom: already stopped");
		return(true);
	}

	if(start_thread_running)
	{
		start_thread_running = false;
		start_thread_joined = false;
	}

	if(!start_finish())
		return(false);

	if(ioctl(fd, IOCTL_BROADCOM_STOP_TRANSCODING, 0))
		Util::vlog("EncoderBroadcom: IOCTL stop transcoding");

	Util::vlog("EncoderBroadcom: starting draining");

	for(;;)
	{
		pfd.fd = fd;
		pfd.events = POLLIN;

		if((rv = poll(&pfd, 1, 10)) == 0)
			break;

		if(rv < 0)
		{
			Util::vlog("EncoderBroadcom: poll error");
			break;
		}

		if(pfd.revents & POLLIN)
		{
			rv = read(fd, buffer, sizeof(buffer));
			Util::vlog("EncoderBroadcom: STOP, drained %d bytes", rv);

			if(rv <= 0)
				break;
		}
		else
			break;
	}

	stopped = true;

	Util::vlog("EncoderBroadcom: encoder STOP TRANSCODING done");

	return(true);
}

int EncoderBroadcom::getfd() const
{
	return(fd);
}

PidMap EncoderBroadcom::getpids() const
{
	return(pids);
}

string EncoderBroadcom::getprop(string property) const
{
	int		procfd;
	ssize_t	rv;
	string	path;
	char	tmp[256];

	path = string("/proc/stb/encoder/") + Util::int_to_string(encoder) + "/" + property;

	if((procfd = open(path.c_str(), O_RDONLY, 0)) < 0)
	{
		Util::vlog("EncoderBroadcom::getprop: cannot open property %s", path.c_str());
		return("");
	}

	if((rv = read(procfd, tmp, sizeof(tmp))) <= 0)
	{
		Util::vlog("EncoderBroadcom::getprop: cannot read from property %s", property.c_str());
		rv = 0;
	}

	tmp[rv] = '\0';

	close(procfd);

	return(tmp);
}

void EncoderBroadcom::setprop(const string &property, const string &value) const
{
	int		procfd;
	string	path;

	Util::vlog("setprop: %s=%s", property.c_str(), value.c_str());

	path = string("/proc/stb/encoder/") + Util::int_to_string(encoder) + "/" + property;

	if((procfd = open(path.c_str(), O_WRONLY, 0)) < 0)
	{
		Util::vlog("EncoderBroadcom::setprop: cannot open property %s", path.c_str());
		return;
	}

	if(write(procfd, value.c_str(), value.length()) != (ssize_t)value.length())
		Util::vlog("EncoderBroadcom::setprop: cannot write to property %s", property.c_str());

	close(procfd);
}
