#include "pidmap.h"
#include "webifrequest.h"
#include "vlog.h"
#include "queue.h"
#include "service.h"
#include "demuxer.h"
#include "livestreaming.h"

#include <string>
using std::string;

#include <ctype.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>

LiveStreaming::LiveStreaming(const Service &service, int socketfd) throw(string)
{
	PidMap::const_iterator it;
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
	Queue			socket_queue(512 * 1024);

	vlog("LiveStreaming: %s", service.service_string().c_str());

	if(!service.is_valid())
		throw(string("LiveStreaming: invalid service"));

	WebifRequest webifrequest(service);

	while((time(0) - timeout) < 30)
	{
		sleep(1);

		webifrequest.poll();

		pids = webifrequest.get_pids();

		if((pids.find("pat") != pids.end()) &&
				(pids.find("pcr") != pids.end()) &&
				(pids.find("pmt") != pids.end()) &&
				(pids.find("video") != pids.end()))
			break;
	}

	demuxer_id = webifrequest.get_demuxer_id();

	for(it = pids.begin(); it != pids.end(); it++)
		vlog("LiveStreaming: pid[%s] = %x", it->first.c_str(), it->second);

	Demuxer demuxer(demuxer_id, pids);

	if((demuxer_fd = demuxer.getfd()) < 0)
		throw(string("LiveStreaming: demuxer: fd not open"));

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
			throw(string("LiveStreaming: poll error"));

		if(pfd[0].revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			vlog("LiveStreaming: demuxer error");
			break;
		}

		if(pfd[1].revents & (POLLRDHUP | POLLERR | POLLHUP | POLLNVAL))
		{
			vlog("LiveStreaming: socket error");
			break;
		}

		if(pfd[0].revents & POLLIN)
		{
			if(!socket_queue.read(demuxer_fd))
			{
				vlog("LiveStreaming: read demuxer error");
				break;
			}
		}

		if(pfd[1].revents & POLLOUT)
		{
			if(!socket_queue.write(socketfd))
			{
				vlog("LiveStreaming: write socket error");
				break;
			}
		}
	}

	vlog("LiveStreaming: streaming ends, socket max queue fill: %d%%", max_fill_socket);
}
