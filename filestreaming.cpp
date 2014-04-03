#include "filestreaming.h"
#include "vlog.h"
#include "queue.h"

#include <unistd.h>
#include <poll.h>
#include <fcntl.h>

#include <string>
using std::string;

FileStreaming::FileStreaming(string file, int socket_fd_in) throw(string) :
	socket_queue(32 * 1024)
{
	size_t			max_fill_socket = 0;
	struct pollfd	pfd;
	string			httpok = "HTTP/1.0 200 OK\r\n"
						"Connection: Close\r\n"
						"Content-Type: video/mpeg\r\n"
						"\r\n";
	
	vlog("FileStreaming: streaming file: %s", file.c_str());

	socket_fd = socket_fd_in;

	if((fd = open(file.c_str(), O_RDONLY, 0)) < 0)
		throw(string("FileStreaming: cannot open file " + file));

	socket_queue.append(httpok.length(), httpok.c_str());

	for(;;)
	{
		if(socket_queue.usage() < 50)
		{
			//vlog("FileStreaming: read file, before: length: %d, fill: %d",
					//socket_queue.length(), socket_queue.usage());

			if(!socket_queue.read(fd))
			{
				vlog("FileStreaming: eof");
				break;
			}

			//vlog("FileStreaming: read file, after: length: %d, fill: %d",
					//socket_queue.length(), socket_queue.usage());
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
			vlog("FileStreaming: socket error");
			break;
		}

		if(pfd.revents & POLLOUT)
		{
			//vlog("socket can write");
			if(!socket_queue.write(socket_fd))
			{
				vlog("FileStreaming: write socket error");
				break;
			}
		}
	}

	vlog("FileStreaming: socket max queue fill: %d%%", max_fill_socket);
	vlog("FileStreaming: streaming ends");
}

FileStreaming::~FileStreaming() throw()
{
	vlog("FileStreaming: cleanup up");
	close(fd);
}
