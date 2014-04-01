#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>

#include <boost/algorithm/string.hpp>

#include "service.h"
#include "streamingsocket.h"
#include "livestreaming.h"
#include "vlog.h"
#include "url.h"

StreamingSocket::StreamingSocket(int fd_in, default_streaming_action default_action) throw()
{
	try
	{
		int arg1;
		static	char read_buffer[1024];
		ssize_t	bytes_read;
		size_t	idx = string::npos;
		string	header;
		struct	pollfd pfd;
		time_t	start;
		bool	url_live, url_stream, url_default;
		string	service_id;

		stringvector lines;
		stringvector tokens;
		stringvector::iterator it1;
		stringvector::const_iterator it2;
		stringmap::const_iterator headit;

		Url::urlparam urlparams;
		Url::urlparam::const_iterator param_it;
		Url::urlparam::const_iterator service_it;

		LiveStreaming::streaming_mode mode;

		fd = fd_in;

		arg1 = fcntl(fd, F_GETFL, 0);
		if(fcntl(fd, F_SETFL, arg1 | O_NONBLOCK))
			throw(string("F_SETFL"));

#if 0
		arg1 = 1;

		if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&arg1, sizeof(arg1)))
			throw(string("StreamingSocket:: cannot set NODELAY"));
#endif

#if 0
		int arg2;

		arg1 = 0;
		arg2 = sizeof(arg1);

		if(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&arg1, (size_t *)&arg2))
			throw(string("getsockopt 1"));

		vlog("current buffer size: %d", arg1);

		arg1 = 256 * 1024;

		if(setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&arg1, sizeof(arg1)))
			throw(string("setsockopt"));

		if(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&arg1, (size_t *)&arg2))
			throw(string("getsockopt 2"));

		vlog("new buffer size: %d", arg1);
#endif

		start = time(0);

		for(;;)
		{
			pfd.fd = fd;
			pfd.events = POLLIN | POLLRDHUP;

			if(poll(&pfd, 1, 1000) < 0)
				throw(string("poll error"));

			if(pfd.revents & (POLLHUP | POLLRDHUP | POLLERR | POLLNVAL))
				throw(string("peer disconnects"));

			if(pfd.revents & POLLIN)
			{
				if((bytes_read = read(fd, read_buffer, sizeof(read_buffer))) <= 0)
					throw(string("peer disconnected"));

				request.append(read_buffer, bytes_read);

				if((idx = request.find("\r\n\r\n")) != string::npos)
					break;
			}

			if((time(0) - start) > 10)
				throw(string("peer connection timeout"));
		}

		if(idx == string::npos)
			throw(string(""));

		split(lines, request, boost::is_any_of("\r\n"));

		for(it1 = lines.begin(); it1 != lines.end(); it1++)
		{
			if((*it1 == "") || (*it1 == "\n") || (*it1 == "\r\n"))
			{
				lines.erase(it1);
				it1 = lines.begin();
			}
		}

		for(it1 = lines.begin(); it1 != lines.end(); it1++)
		{
			vlog("line: \"%s\"", it1->c_str());

			if(it1->find("GET ") == 0)
			{
				split(tokens, *it1, boost::is_any_of(" "));

				if((tokens.size() != 3) || (tokens[0] != "GET") ||
						((tokens[2] != "HTTP/1.1") && (tokens[2] != "HTTP/1.0")))
					continue;

				url = tokens[1];
				continue;
			}

			if((idx = it1->find(':')) != string::npos)
			{
				header = it1->substr(0, idx);

				if((idx = it1->find_first_not_of(": ", idx)) != string::npos)
					headers[header] = it1->substr(idx);

				continue;
			}

			vlog("ignoring junk line: %s\n", it1->c_str());
		}

		if(lines.size() < 1)
			throw(string(""));

		for(headit = headers.begin(); headit != headers.end(); headit++)
			vlog("header[%s]: \"%s\"", headit->first.c_str(), headit->second.c_str());

		vlog("url: %s", url.c_str());

		urlparams = Url(url).split();

		for(param_it = urlparams.begin(); param_it != urlparams.end(); param_it++)
			vlog("parameter[%s] = \"%s\"", param_it->first.c_str(), param_it->second.c_str());

		url_stream = url_live = url_default = 0;

		if(((url_stream = (urlparams[""] == "/stream"))) || ((url_live = (urlparams[""] == "/live"))) ||
				(url_default = Service(urlparams[""].substr(1)).is_valid()))
		{
			if((service_it = urlparams.find("service")) != urlparams.end())
				service_id = service_it->second;
			else
				service_id = "";

			if(url_default)
			{
				if(default_action == action_transcode)
					mode = LiveStreaming::mode_transcode;
				else
					mode = LiveStreaming::mode_stream;

				service_id = urlparams[""].substr(1);
			}
			else
			{
				if(url_stream)
				{
					mode = LiveStreaming::mode_stream;
				}
				else
				{
					if(url_live)
					{
						mode = LiveStreaming::mode_transcode;
					}
					else
					{
						mode = LiveStreaming::mode_stream; // keep GCC happy
					}
				}
			}

			Service service(service_id);

			if(service.is_valid())
			{
				vlog("streaming request");
				(void)LiveStreaming(service, fd, mode);
				vlog("streaming ends");
				return;
			}
		}

		if(urlparams[""] == "/file")
		{
			vlog("file transcoding request");
			return;
		}

		throw(string("unknown request: ") + urlparams[""]);
	}
	catch(const string &e)
	{
		string reply;

		vlog("exception: %s", e.c_str());
		reply = string("400 Bad request: ") + e + "\r\n\r\n";
		write(fd, reply.c_str(), reply.length());
	}
	catch(...)
	{
		string reply;

		vlog("unknown exception");
		reply = "400 Bad request: \r\n\r\n";
		write(fd, reply.c_str(), reply.length());
	}
}

StreamingSocket::~StreamingSocket() throw()
{
	close(fd);
}
