#include "config.h"
#include "trap.h"

#include "types.h"
#include "webifrequest.h"
#include "util.h"
#include "queue.h"
#include "service.h"
#include "demuxer.h"
#include "encoder-broadcom.h"
#include "configmap.h"
#include "livetranscoding-broadcom.h"
#include "stbtraits.h"

#include <string>
using std::string;

#include <ctype.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>

LiveTranscodingBroadcom::LiveTranscodingBroadcom(const Service &service, int socketfd,
		const stb_traits_t &stb_traits,
		const StreamingParameters &streaming_parameters,
		const ConfigMap &config_map)
{
	PidMap::const_iterator it;
	bool			webifrequest_ok;
	time_t			timeout = time(0);
	PidMap			pids, encoder_pids;
	int				demuxer_id;
	int				demuxer_fd;
	int				encoder_fd;
	size_t			max_fill_encoder = 0;
	size_t			max_fill_socket = 0;
	struct pollfd	pfd[3];
	encoder_state_t	encoder_state;
	string			httpok = "HTTP/1.0 200 OK\r\n"
						"Connection: Close\r\n"
						"Content-Type: video/mpeg\r\n"
						"\r\n";

	Queue			encoder_queue(1024 * 1024);
	Queue			socket_queue(1024 * 1024);

	Util::vlog("LiveTranscodingBroadcom: %s", service.service_string().c_str());

	if(!service.is_valid())
		throw(http_trap("LiveTranscodingBroadcom: invalid service", 404, "Not found, unknown service"));

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
		throw(http_trap("LiveTranscodingBroadcom: tuning request to enigma failed (webif timeout)", 404, "Not found, cannot tune to service"));

	demuxer_id = webifrequest.get_demuxer_id();

	for(it = pids.begin(); it != pids.end(); it++)
		Util::vlog("LiveTranscodingBroadcom: pid[%s] = 0x%x", it->first.c_str(), it->second);

	EncoderBroadcom encoder(pids, stb_traits, streaming_parameters);
	encoder_pids = encoder.getpids();

	for(it = encoder_pids.begin(); it != encoder_pids.end(); it++)
		Util::vlog("LiveTranscodingBroadcom: encoder pid[%s] = 0x%x", it->first.c_str(), it->second);

	Demuxer demuxer(demuxer_id, encoder_pids);

	if((encoder_fd = encoder.getfd()) < 0)
		throw(trap("LiveTranscodingBroadcom: encoder: fd not open"));

	if((demuxer_fd = demuxer.getfd()) < 0)
		throw(trap("LiveTranscodingBroadcom: demuxer: fd not open"));

	socket_queue.append(httpok.length(), httpok.c_str());

	encoder_state = state_initial;

	for(;;)
	{
		if(encoder_queue.length() > max_fill_encoder)
			max_fill_encoder = encoder_queue.usage();

		if(socket_queue.length() > max_fill_socket)
			max_fill_socket = socket_queue.usage();

		switch(encoder_state)
		{
			case(state_initial):
			{
				if(encoder.start_init())
				{
					encoder_state = state_starting;
					Util::vlog("LiveTranscodingBroadcom: state init -> starting");
				}
				break;
			}

			case(state_starting):
			{
				if(encoder.start_finish())
				{
					encoder_state = state_running;
					Util::vlog("LiveTranscodingBroadcom: state starting -> running");
				}
				break;
			}

			case(state_running):
			{
				break;
			}
		}

		pfd[0].fd		= demuxer_fd;
		pfd[0].events	= POLLIN;

		pfd[1].fd		= encoder_fd;	// encoder_fd == -1 when not transcoding
		pfd[1].events	= POLLIN;		// poll ignores fd's that are == -1

		pfd[2].fd		= socketfd;
		pfd[2].events	= POLLRDHUP;

		if(encoder_queue.length() >= broadcom_magic_buffer_size)
			pfd[1].events |= POLLOUT;

		if(socket_queue.length() > 0)
			pfd[2].events |= POLLOUT;

		if(poll(pfd, 3, -1) <= 0)
			throw(trap("LiveTranscodingBroadcom: streaming: poll error"));

		if(pfd[0].revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			Util::vlog("LiveTranscodingBroadcom: demuxer error");
			break;
		}

		if(pfd[1].revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			Util::vlog("LiveTranscodingBroadcom: encoder error");
			break;
		}

		if(pfd[2].revents & (POLLRDHUP | POLLHUP))
		{
			Util::vlog("LiveTranscodingBroadcom: client hung up");
			break;
		}

		if(pfd[2].revents & (POLLERR | POLLNVAL))
		{
			Util::vlog("LiveTranscodingBroadcom: socket error");
			break;
		}

		if(pfd[0].revents & POLLIN)
		{
			if(!encoder_queue.read(demuxer_fd))
			{
				Util::vlog("LiveTranscodingBroadcom: read demux error");
				break;
			}
		}

		if(pfd[1].revents & POLLOUT)
		{
			if(!encoder_queue.write(encoder_fd, broadcom_magic_buffer_size))
			{
				Util::vlog("LiveTranscodingBroadcom: write encoder error");
				break;
			}
		}

		if(pfd[1].revents & POLLIN)
		{
			if(!socket_queue.read(encoder_fd, broadcom_magic_buffer_size))
			{
				Util::vlog("LiveTranscodingBroadcom: read encoder error");
				break;
			}
		}

		if(pfd[2].revents & POLLOUT)
		{
			if(!socket_queue.write(socketfd))
			{
				Util::vlog("LiveTranscodingBroadcom: write socket error");
				break;
			}
		}
	}

	Util::vlog("LiveTranscodingBroadcom: streaming ends, encoder max queue fill: %d%%", max_fill_encoder);
	Util::vlog("LiveTranscodingBroadcom: socket max queue fill: %d%%", max_fill_socket);
}
