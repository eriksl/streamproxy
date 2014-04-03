#include "filetranscoding.h"
#include "vlog.h"
#include "queue.h"
#include "encoder.h"
#include "pidmap.h"
#include "mpegts_pat.h"
#include "mpegts_pmt.h"

#include <string>
using std::string;

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

FileTranscoding::FileTranscoding(string file, int socket_fd) throw(string)
{
	PidMap::const_iterator it;
	PidMap			pids, encoder_pids;
	int				file_fd, encoder_fd, timeout;
	size_t			max_fill_socket = 0;
	ssize_t			bytes_read;
	struct pollfd	pfd[2];
	encoder_state_t	encoder_state;
	string			httpok = "HTTP/1.0 200 OK\r\n"
						"Connection: Close\r\n"
						"Content-Type: video/mpeg\r\n"
						"\r\n";
	Queue			socket_queue(256 * 1024);

	vlog("transcoding file: %s", file.c_str());

	encoder_buffer = new char[vuplus_magic_buffer_size];

	if((file_fd = open(file.c_str(), O_RDONLY, 0)) < 0)
		throw(string("FileTranscoding: cannot open file " + file));

	MpegTSPat pat(file_fd);
	MpegTSPmt pmt(file_fd);
	MpegTSPat::pat_t::const_iterator pat_it;

	pat.probe();

	for(pat_it = pat.pat.begin(); pat_it != pat.pat.end(); pat_it++)
		if(pmt.probe(pat_it->second))
			break;

	if(pat_it == pat.pat.end())
		throw(string("FileTranscoding: transcoding: invalid transport stream"));

	pids["pmt"]		= pat_it->second;
	pids["video"]	= pmt.video_pid;
	pids["audio"]	= pmt.audio_pid;

	for(it = pids.begin(); it != pids.end(); it++)
		vlog("pid[%s] = %x", it->first.c_str(), it->second);

	Encoder encoder(pids);
	encoder_pids = encoder.getpids();

	for(it = encoder_pids.begin(); it != encoder_pids.end(); it++)
		vlog("encoder pid[%s] = %x", it->first.c_str(), it->second);

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
					vlog("state init -> starting");
				}
				break;
			}

			case(state_starting):
			{
				if(encoder.start_finish())
				{
					encoder_state = state_running;
					vlog("state starting -> running");
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
			//if(pfd[0].revents & POLLOUT)
				//vlog("FileTranscoding: encoder can write (poll)");

			if((bytes_read = read(file_fd, encoder_buffer, vuplus_magic_buffer_size)) != vuplus_magic_buffer_size)
			{
				vlog("FileTranscoding: eof");
				break;
			}

			if((bytes_read = write(encoder_fd, encoder_buffer, bytes_read)) != vuplus_magic_buffer_size)
			{
				vlog("FileTranscoding: encoder error");
				break;
			}

			//vlog("FileTranscoding: written %d bytes to encoder", bytes_read);
		}

		if(pfd[0].revents & POLLIN)
		{
			//vlog("FileTranscoding: encoder can read");

			if(!socket_queue.read(encoder_fd, vuplus_magic_buffer_size))
			{
				vlog("Transcoding: read encoder error");
				break;
			}

			//vlog("FileTranscoding: data in socket queue: %d", socket_queue.length());
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

	vlog("FileTranscoding: socket max queue fill: %d%%", max_fill_socket);
	vlog("FileTranscoding: streaming ends");
}

FileTranscoding::~FileTranscoding() throw()
{
	delete [] encoder_buffer;
	vlog("FileTranscoding: cleanup up");
}
