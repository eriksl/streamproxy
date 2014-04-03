#include "clientsocket.h"
#include "service.h"
#include "livestreaming.h"
#include "livetranscoding.h"
#include "filestreaming.h"
#include "filetranscoding.h"
#include "vlog.h"
#include "url.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>

#include <boost/algorithm/string.hpp>

ClientSocket::ClientSocket(int fd_in, default_streaming_action default_action) throw()
{
	try
	{
		int			arg1;
		static		char read_buffer[1024];
		ssize_t		bytes_read;
		size_t		idx = string::npos;
		string		header;
		struct		pollfd pfd;
		time_t		start;

		stringvector lines;
		stringvector tokens;
		stringvector::iterator it1;
		stringvector::const_iterator it2;
		stringmap::const_iterator headit;

		Url::urlparam urlparams;
		Url::urlparam::const_iterator param_it;

		fd = fd_in;

		arg1 = fcntl(fd, F_GETFL, 0);
		if(fcntl(fd, F_SETFL, arg1 | O_NONBLOCK))
			throw(string("F_SETFL"));

#if 0
		arg1 = 1;

		if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&arg1, sizeof(arg1)))
			throw(string("ClientSocket: cannot set NODELAY"));
#endif

#if 0
		int arg2;

		arg1 = 0;
		arg2 = sizeof(arg1);

		if(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&arg1, (size_t *)&arg2))
			throw(string("ClientSocket: getsockopt 1"));

		vlog("ClientSocket: current buffer size: %d", arg1);

		arg1 = 256 * 1024;

		if(setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&arg1, sizeof(arg1)))
			throw(string("ClientSocket: setsockopt"));

		if(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&arg1, (size_t *)&arg2))
			throw(string("ClientSocket: getsockopt 2"));

		vlog("ClientSocket: new buffer size: %d", arg1);
#endif

		start = time(0);

		for(;;)
		{
			pfd.fd = fd;
			pfd.events = POLLIN | POLLRDHUP;

			if(poll(&pfd, 1, 1000) < 0)
				throw(string("ClientSocket: poll error"));

			if(pfd.revents & (POLLHUP | POLLRDHUP | POLLERR | POLLNVAL))
				throw(string("ClientSocket: peer disconnects"));

			if(pfd.revents & POLLIN)
			{
				if((bytes_read = read(fd, read_buffer, sizeof(read_buffer))) <= 0)
					throw(string("ClientSocket: peer disconnected"));

				request.append(read_buffer, bytes_read);

				if((idx = request.find("\r\n\r\n")) != string::npos)
					break;
			}

			if((time(0) - start) > 10)
				throw(string("ClientSocket: peer connection timeout"));
		}

		if(idx == string::npos)
			throw(string("ClientSocket: no request"));

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
			vlog("ClientSocket: line: \"%s\"", it1->c_str());

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

			vlog("ClientSocket: ignoring junk line: %s\n", it1->c_str());
		}

		if(lines.size() < 1)
			throw(string("ClientSocket: invalid request"));

		for(headit = headers.begin(); headit != headers.end(); headit++)
			vlog("ClientSocket: header[%s]: \"%s\"", headit->first.c_str(), headit->second.c_str());

		vlog("ClientSocket: url: %s", url.c_str());

		urlparams = Url(url).split();

		for(param_it = urlparams.begin(); param_it != urlparams.end(); param_it++)
			vlog("ClientSocket: parameter[%s] = \"%s\"", param_it->first.c_str(), param_it->second.c_str());

		if((urlparams[""] == "/livestream") && urlparams.count("service"))
		{
			Service service(urlparams["service"]);

			vlog("ClientSocket: live streaming request");
			(void)LiveStreaming(service, fd);
			vlog("ClientSocket: live streaming ends");

			return;
		}

		if((urlparams[""] == "/live") && urlparams.count("service"))
		{
			Service service(urlparams["service"]);

			vlog("ClientSocket: live transcoding request");
			(void)LiveTranscoding(service, fd);
			vlog("ClientSocket: live transcoding ends");

			return;
		}

		if((urlparams[""] == "/filestream") && urlparams.count("file"))
		{
			vlog("ClientSocket: file streaming request");
			(void)FileStreaming(urlparams["file"], fd);
			vlog("ClientSocket: file streaming ends");

			return;
		}

		if((urlparams[""] == "/file") && urlparams.count("file"))
		{
			vlog("ClientSocket: file transcoding request");
			(void)FileTranscoding(urlparams["file"], fd);
			vlog("ClientSocket: file transcoding ends");

			return;
		}

		if(urlparams[""].length() > 1)
		{
			if((urlparams[""].substr(1, 1) == "/"))
			{
				vlog("ClientSocket: default file request");

				if(default_action == action_stream)
				{
					vlog("ClientSocket: streaming file");
					(void)FileStreaming(urlparams["file"], fd);
				}
				else
				{
					vlog("ClientSocket: transcoding file");
					(void)FileTranscoding(urlparams["file"], fd);
				}

				vlog("ClientSocket: default file ends");

				return;
			}
			else
			{
				Service service(urlparams[""].substr(1));

				vlog("ClientSocket: default live request");

				if(service.is_valid())
				{
					if(default_action == action_transcode)
					{
						vlog("ClientSocket: streaming service");
						(void)LiveStreaming(service, fd);
					}
					else
					{
						vlog("ClientSocket: transcoding service");
						//(void)LiveTranscoding(service, fd);
					}

					vlog("ClientSocket: default live ends");

					return;
				}
			}
		}

		throw(string("ClientSocket: unknown request: ") + urlparams[""]);
	}
	catch(const string &e)
	{
		string reply;

		vlog("ClientSocket: exception: %s", e.c_str());
		reply = string("400 Bad request: ") + e + "\r\n\r\n";
		write(fd, reply.c_str(), reply.length());
	}
	catch(...)
	{
		string reply;

		vlog("ClientSocket: unknown exception");
		reply = "400 Bad request: \r\n\r\n";
		write(fd, reply.c_str(), reply.length());
	}
}

ClientSocket::~ClientSocket() throw()
{
	close(fd);
}
