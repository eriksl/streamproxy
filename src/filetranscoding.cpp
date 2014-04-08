#include "filetranscoding.h"
#include "vlog.h"
#include "queue.h"
#include "encoder.h"
#include "pidmap.h"
#include "mpegts.h"

#include <string>
using std::string;

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

FileTranscoding::FileTranscoding(string file, int socket_fd, int time_offset_s) throw(string)
{
	PidMap::const_iterator it;
	PidMap			pids, encoder_pids;
	int				encoder_fd, timeout;
	size_t			max_fill_socket = 0;
	ssize_t			bytes_read;
	struct pollfd	pfd[2];
	encoder_state_t	encoder_state;
	string			httpok = "HTTP/1.0 200 OK\r\n"
						"Connection: Close\r\n"
						"Content-Type: video/mpeg\r\n"
						"\r\n";
	Queue			socket_queue(1024 * 1024);

	vlog("FileTranscoding: transcoding %s from %d", file.c_str(), time_offset_s);

	encoder_buffer = new char[vuplus_magic_buffer_size];

	MpegTS stream(file);

	pids["pmt"]		= stream.pmt_pid;
	pids["video"]	= stream.video_pid;
	pids["audio"]	= stream.audio_pid;

	if(stream.is_seekable)
		stream.seek((time_offset_s * 1000) + stream.first_pcr_ms);

	for(it = pids.begin(); it != pids.end(); it++)
		vlog("FileTranscoding: pid[%s] = %x", it->first.c_str(), it->second);

	Encoder encoder(pids);
	encoder_pids = encoder.getpids();

	for(it = encoder_pids.begin(); it != encoder_pids.end(); it++)
		vlog("FileTranscoding: encoder pid[%s] = %x", it->first.c_str(), it->second);

	if((encoder_fd = encoder.getfd()) < 0)
		throw(string("FileTranscoding: transcoding: encoder: fd not open"));

	encoder_state = state_initial;

	socket_queue.append(httpok.length(), httpok.c_str());

	for(;;)
	{
		if(socket_queue.usage() > max_fill_socket)
			max_fill_socket = socket_queue.usage();

		switch(encoder_state)
		{
			case(state_initial):
			{
				if(encoder.start_init())
				{
					encoder_state = state_starting;
					//vlog("state init -> starting");
				}
				break;
			}

			case(state_starting):
			{
				if(encoder.start_finish())
				{
					encoder_state = state_running;
					//vlog("state starting -> running");
				}
				break;
			}

			case(state_running):
			{
				break;
			}
		}

		pfd[0].fd		= encoder_fd;
		pfd[0].events	= POLLIN | POLLOUT;

		pfd[1].fd		= socket_fd;
		pfd[1].events	= POLLRDHUP;

		if(socket_queue.length() > 0)
			pfd[1].events |= POLLOUT;

		timeout = (encoder_state == state_starting) ? 100 : -1;

		if(poll(pfd, 2, timeout) <= 0)
			throw(string("FileTranscoding: poll error"));

		if(pfd[0].revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			vlog("FileTranscoding: encoder error");
			break;
		}

		if(pfd[1].revents & (POLLRDHUP | POLLERR | POLLHUP | POLLNVAL))
		{
			vlog("FileTranscoding: socket error");
			break;
		}

		if(pfd[0].revents & POLLOUT)
		{
			if((bytes_read = read(stream.get_fd(), encoder_buffer, vuplus_magic_buffer_size)) != vuplus_magic_buffer_size)
			{
				vlog("FileTranscoding: eof");
				break;
			}

			if((bytes_read = write(encoder_fd, encoder_buffer, bytes_read)) != vuplus_magic_buffer_size)
			{
				vlog("FileTranscoding: encoder error");
				break;
			}
		}

		if(pfd[0].revents & POLLIN)
		{
			if(!socket_queue.read(encoder_fd, vuplus_magic_buffer_size))
			{
				vlog("FileTranscoding: read encoder error");
				break;
			}
		}

		if(pfd[1].revents & POLLOUT)
		{
			if(!socket_queue.write(socket_fd))
			{
				vlog("FileTranscoding: write socket error");
				break;
			}
		}
	}

	vlog("FileTranscoding: streaming ends, socket max queue fill: %d%%", max_fill_socket);
}

FileTranscoding::~FileTranscoding() throw()
{
	delete [] encoder_buffer;
	vlog("FileTranscoding: cleanup up");
}
