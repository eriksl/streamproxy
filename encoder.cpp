#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#include "pidmap.h"
#include "encoder.h"
#include "demuxer.h"
#include "vlog.h"

#include <string>
using std::string;

Encoder::Encoder(const PidMap &pids_in) throw(string)
{
	PidMap::const_iterator it;
	PidMap::iterator it2;
	PidMap::const_iterator pmt, video, audio;
	int encoder;
	char encoder_device[128];

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
		throw(string("encoder: missing pmt, video or audio pid"));

	for(encoder = 0; encoder < 8; encoder++)
	{
		snprintf(encoder_device, sizeof(encoder_device), "/dev/bcm_enc%d", encoder);
		errno = 0;
		vlog("open encoder %s", encoder_device);

		if((fd = open(encoder_device, O_RDWR, 0)) < 0)
		{
			if(errno == ENOENT)
			{
				vlog("not found %s", encoder_device);
				break;
			}

			vlog("encoder %d is busy", encoder);
		}
		else
			break;
	}

	if(fd < 0)
		throw(string("cannot open encoder"));

	vlog("encoder ioctl SET PMTPID: 0x%x", pmt->second);
	vlog("encoder ioctl SET VPID: 0x%x", video->second);
	vlog("encoder ioctl SET APID: 0x%x", audio->second);

	if(ioctl(fd, IOCTL_VUPLUS_SET_PMTPID, pmt->second) ||
			ioctl(fd, IOCTL_VUPLUS_SET_VPID, video->second) ||
			ioctl(fd, IOCTL_VUPLUS_SET_APID, audio->second))
	{
			vlog("cannot set pids on encoder");
			throw(string("cannot init encoder"));
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

	vlog("encoder start thread: start ioctl");

	_this->start_thread_joined	= false;
	_this->start_thread_running	= true;
	_this->stopped				= false;

	if(ioctl(_this->fd, IOCTL_VUPLUS_START_TRANSCODING, 0))
		vlog("IOCTL start transcoding");

	_this->start_thread_running = false;

	return((void *)0);
}

bool Encoder::start_init() throw()
{
	vlog("encoder START TRANSCODING start");

	if(start_thread_running || !start_thread_joined)
	{
		vlog("encoder start: start thread already running");
		return(true);
	}

	if(pthread_create(&start_thread, 0, &start_thread_function, (void *)this))
	{
		vlog("encoder start: pthread create failed");
		return(false);
	}

	vlog("encoder START TRANSCODING done");
	return(true);
}

bool Encoder::start_finish() throw()
{
	if(!start_thread_running && !start_thread_joined)
	{
		vlog("encoder START detects start thread stopped");
		pthread_join(start_thread, 0);
		vlog("encoder START joined start thread");
		start_thread_joined = true;

		return(true);
	}

	return(false);
}

bool Encoder::stop() throw()
{
	struct pollfd pfd;
	char buffer[4096];
	ssize_t rv;

	vlog("encoder STOP TRANSCODING begin");

	if(stopped)
	{
		vlog("already stopped");
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
		vlog("IOCTL stop transcoding");

	vlog("starting draining");

	for(;;)
	{
		pfd.fd = fd;
		pfd.events = POLLIN;

		if((rv = poll(&pfd, 1, 10)) == 0)
			break;

		if(rv < 0)
		{
			vlog("poll error");
			break;
		}

		if(pfd.revents & POLLIN)
		{
			rv = read(fd, buffer, sizeof(buffer));
			vlog("encoder STOP, drained %d bytes", rv);

			if(rv <= 0)
				break;
		}
		else
			break;
	}

	stopped = true;

	vlog("draining done");
	vlog("encoder STOP TRANSCODING done");

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
