#include "filestreaming.h"
#include "util.h"
#include "queue.h"
#include "mpegts.h"

#include <unistd.h>
#include <poll.h>
#include <fcntl.h>

#include <string>
using std::string;

FileStreaming::FileStreaming(string file, int socket_fd, int time_offset_s) throw(string)
{
	size_t			max_fill_socket = 0;
	struct pollfd	pfd;
	string			httpok = "HTTP/1.0 200 OK\r\n"
						"Connection: Close\r\n"
						"Content-Type: video/mpeg\r\n"
						"\r\n";
	
	Util::vlog("FileStreaming: streaming file: %s from %d", file.c_str(), time_offset_s);

	MpegTS stream(file);

	if(stream.is_seekable)
		stream.seek((time_offset_s * 1000) + stream.first_pcr_ms);

	Queue socket_queue(32 * 1024);

	socket_queue.append(httpok.length(), httpok.c_str());

	for(;;)
	{
		if(socket_queue.usage() < 50)
		{
			if(!socket_queue.read(stream.get_fd()))
			{
				Util::vlog("FileStreaming: eof");
				break;
			}
		}

		if(socket_queue.usage() > max_fill_socket)
			max_fill_socket = socket_queue.usage();

		pfd.fd		= socket_fd;
		pfd.events	= POLLRDHUP;

		if(socket_queue.length() > 0)
			pfd.events |= POLLOUT;

		if(poll(&pfd, 1, -1) <= 0)
			throw(string("FileStreaming: poll error"));

		if(pfd.revents & (POLLRDHUP | POLLERR | POLLHUP | POLLNVAL))
		{
			Util::vlog("FileStreaming: socket error");
			break;
		}

		if(pfd.revents & POLLOUT)
		{
			if(!socket_queue.write(socket_fd))
			{
				Util::vlog("FileStreaming: write socket error");
				break;
			}
		}
	}

	Util::vlog("FileStreaming: streaming ends, socket max queue fill: %d%%", max_fill_socket);
}
