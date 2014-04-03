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
	int				encoder_fd;
	size_t			max_fill_encoder = 0;
	size_t			max_fill_socket = 0;
	struct pollfd	pfd[3];
	string			httpok = "HTTP/1.0 200 OK\r\n"
						"Connection: Close\r\n"
						"Content-Type: video/mpeg\r\n"
						"\r\n";

	demuxer			= 0;
	encoder			= 0;
	encoder_queue	= 0;
	socket_queue	= 0;

	vlog("streaming service: %s", service.service_string().c_str());

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
		vlog("pid[%s] = %x", it->first.c_str(), it->second);

	if(mode == mode_transcode)
	{
		encoder = new Encoder(pids);
		encoder_pids = encoder->getpids();

		for(it = encoder_pids.begin(); it != encoder_pids.end(); it++)
			vlog("encoder pid[%s] = %x", it->first.c_str(), it->second);

		demuxer = new Demuxer(demuxer_id, encoder_pids);

		if((encoder_fd = encoder->getfd()) < 0)
			throw(string("encoder: fd not open"));

		encoder_queue = new Queue(512 * 1024);
		socket_queue  = new Queue(256 * 1024);

		encoder->start_init();
	}
	else
	{
		demuxer = new Demuxer(demuxer_id, pids);

		encoder_fd		= -1;
		encoder_queue	= 0;
		socket_queue = new Queue(512 * 1024);
	}

	if((demuxer_fd = demuxer->getfd()) < 0)
		throw(string("demuxer: fd not open"));

	socket_queue->append(httpok.length(), httpok.c_str());

	for(;;)
	{
		if(encoder_queue && (encoder_queue->length() > max_fill_encoder))
			max_fill_encoder = encoder_queue->length();

		if(socket_queue && (socket_queue->length() > max_fill_socket))
			max_fill_socket = socket_queue->length();

		if(encoder)
			encoder->start_finish(); 	// cleanup encoder setup thread when done

		pfd[0].fd		= demuxer_fd;
		pfd[0].events	= POLLIN;

		pfd[1].fd		= encoder_fd;	// encoder_fd == -1 when not transcoding
		pfd[1].events	= POLLIN;		// poll ignores fd's that are == -1

		pfd[2].fd		= socketfd;
		pfd[2].events	= POLLRDHUP;

		if(encoder_queue && (encoder_queue->length() >= vuplus_magic_buffer_size))
			pfd[1].events |= POLLOUT;

		if(socket_queue && (socket_queue->length() > 0))
			pfd[2].events |= POLLOUT;

		if(poll(pfd, 3, -1) <= 0)
			throw(string("streaming: poll error"));

		if(pfd[0].revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			vlog("LiveStreaming: demuxer error");
			break;
		}

		if(pfd[1].revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			vlog("streaming: encoder error");
			break;
		}

		if(pfd[2].revents & (POLLRDHUP | POLLERR | POLLHUP | POLLNVAL))
		{
			vlog("streaming: socket error");
			break;
		}

		if(pfd[0].revents & POLLIN)
		{
			Queue *dst_queue = encoder_queue ? encoder_queue : socket_queue;

			if(!dst_queue->read(demuxer_fd))
			{
				vlog("LiveStreaming: read demuxer error");
				break;
			}
		}

		if(pfd[1].revents & POLLOUT)
		{
			if(!encoder_queue->write(encoder_fd, vuplus_magic_buffer_size))
			{
				vlog("streaming: write encoder error");
				break;
			}
		}

		if(pfd[1].revents & POLLIN)
		{
			if(!socket_queue->read(encoder_fd, vuplus_magic_buffer_size))
			{
				vlog("streaming: read encoder error");
				break;
			}
		}

		if(pfd[2].revents & POLLOUT)
		{
			if(!socket_queue->write(socketfd))
			{
				vlog("LiveStreaming: write socket error");
				break;
			}
		}
	}

	if(encoder_queue)
		vlog("encoder max queue fill: %d%%", max_fill_encoder * 100 / encoder_queue->size());

	if(socket_queue)
		vlog("socket max queue fill: %d%%", max_fill_socket * 100 / socket_queue->size());

	vlog("streaming ends");
}

LiveStreaming::~LiveStreaming() throw()
{
	if(demuxer)
		delete(demuxer);

	if(encoder)
		delete(encoder);

	if(encoder_queue)
		delete(encoder_queue);

	if(socket_queue)
		delete(socket_queue);

	vlog("demuxer and encoder cleanup up");
}
