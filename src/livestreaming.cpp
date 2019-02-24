#include "config.h"
#include "trap.h"

#include "types.h"
#include "webifrequest.h"
#include "util.h"
#include "queue.h"
#include "service.h"
#include "demuxer.h"
#include "livestreaming.h"
#include "configmap.h"

#include <string>
using std::string;

#include <ctype.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>

LiveStreaming::LiveStreaming(const Service &service, int socketfd,
		const StreamingParameters &, const ConfigMap &config_map)
{
	PidMap::const_iterator it;
	bool			webifrequest_ok;
	time_t			timeout = time(0);
	PidMap			pids, encoder_pids;
	int				demuxer_id;
	int				demuxer_fd;
	size_t			max_fill_socket = 0;
	struct pollfd	pfd[2];
	string			httpok = "HTTP/1.0 200 OK\r\n"
						"Connection: Close\r\n"
						"Content-Type: video/mpeg\r\n"
						"\r\n";
	Queue			socket_queue(1024 * 1024);

	Util::vlog("LiveStreaming: %s", service.service_string().c_str());

	if(!service.is_valid())
		throw(http_trap("LiveStreaming: invalid service", 404, "Not found, service not found"));

	WebifRequest webifrequest(service, config_map);

	for(webifrequest_ok = false; (time(0) - timeout) < 60; )
	{
		usleep(100000);

		webifrequest.poll();

		pids = webifrequest.get_pids();

		if((pids.find("pat") != pids.end()) &&
				(pids.find("pcr") != pids.end()) &&
				(pids.find("pmt") != pids.end()) &&
				(pids.find("video") != pids.end()))
		{
			webifrequest_ok = true;
			break;
		}
	}

	if(!webifrequest_ok)
		throw(http_trap("LiveStreaming: tuning request to enigma failed (webif timeout)", 404, "Not found, service cannot be tuned"));

	demuxer_id = webifrequest.get_demuxer_id();

	for(it = pids.begin(); it != pids.end(); it++)
		Util::vlog("LiveStreaming: pid[%s] = 0x%x", it->first.c_str(), it->second);

	Demuxer demuxer(demuxer_id, pids);

	if((demuxer_fd = demuxer.getfd()) < 0)
		throw(trap("LiveStreaming: demuxer: fd not open"));

	socket_queue.append(httpok.length(), httpok.c_str());

	for(;;)
	{
		if(socket_queue.usage() > max_fill_socket)
			max_fill_socket = socket_queue.usage();

		pfd[0].fd		= demuxer_fd;
		pfd[0].events	= POLLIN;

		pfd[1].fd		= socketfd;
		pfd[1].events	= POLLRDHUP;

		if(socket_queue.length() > 0)
			pfd[1].events |= POLLOUT;

		if(poll(pfd, 2, -1) <= 0)
			throw(trap("LiveStreaming: poll error"));

		if(pfd[0].revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			Util::vlog("LiveStreaming: demuxer error");
			break;
		}

		if(pfd[1].revents & (POLLRDHUP | POLLHUP))
		{
			Util::vlog("LiveStreaming: client hung up");
			break;
		}

		if(pfd[1].revents & (POLLERR | POLLNVAL))
		{
			Util::vlog("LiveStreaming: socket error");
			break;
		}

		if(pfd[0].revents & POLLIN)
		{
			if(!socket_queue.read(demuxer_fd))
			{
				Util::vlog("LiveStreaming: read demuxer error");
				break;
			}
		}

		if(pfd[1].revents & POLLOUT)
		{
			if(!socket_queue.write(socketfd))
			{
				Util::vlog("LiveStreaming: write socket error");
				break;
			}
		}
	}

	Util::vlog("LiveStreaming: streaming ends, socket max queue fill: %d%%", max_fill_socket);
}
