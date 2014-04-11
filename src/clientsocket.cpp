#include "clientsocket.h"
#include "service.h"
#include "livestreaming.h"
#include "livetranscoding.h"
#include "filestreaming.h"
#include "filetranscoding.h"
#include "util.h"
#include "url.h"
#include "time_offset.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <shadow.h>

#include <boost/algorithm/string.hpp>

ClientSocket::ClientSocket(int fd_in, bool use_web_authentication,
		string require_auth_group, default_streaming_action default_action,
		string frame_size, string bitrate, string profile,
		string level, string bframes) throw()
{
	string	reply;

	try
	{
		static		char read_buffer[1024];
		ssize_t		bytes_read;
		size_t		idx = string::npos;
		string		header;
		struct		pollfd pfd;
		time_t		start;
		string		webauth, user, password;
		int			time_offset;

		stringvector lines;
		stringvector tokens;
		stringvector::iterator it1;
		stringvector::const_iterator it2;
		stringmap::const_iterator headit;

		Url::urlparam urlparams;
		Url::urlparam::const_iterator param_it;

		fd = fd_in;

		int arg1;

		arg1 = fcntl(fd, F_GETFL, 0);
		if(fcntl(fd, F_SETFL, arg1 | O_NONBLOCK))
			throw(string("ClientSocket: F_SETFL"));

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

		Util::vlog("ClientSocket: current buffer size: %d", arg1);

		arg1 = 256 * 1024;

		if(setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&arg1, sizeof(arg1)))
			throw(string("ClientSocket: setsockopt"));

		if(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&arg1, (size_t *)&arg2))
			throw(string("ClientSocket: getsockopt 2"));

		Util::vlog("ClientSocket: new buffer size: %d", arg1);
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

			if((time(0) - start) > 5)
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
			//Util::vlog("ClientSocket: line: \"%s\"", it1->c_str());

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

			Util::vlog("ClientSocket: ignoring junk line: %s\n", it1->c_str());
		}

		if(lines.size() < 1)
			throw(string("ClientSocket: invalid request"));

		if(use_web_authentication)
		{
			if(!headers.count("Authorization"))
			{
				Util::vlog("ClientSocket: no authorisation received from client");
				reply = string("HTTP/1.0 401 Unauthorized\r\n") +
					"WWW-Authenticate: Basic realm=\"OpenWebif\"\r\n" +
					"Content-Type: text/html\r\n" +
					"Connection: close\r\n\r\n";
				write(fd, reply.c_str(), reply.length());

				return;
			}

			webauth = headers.at("Authorization");

			if((idx = webauth.find(' ')) == string::npos)
				throw(string("ClientSocket: web authentication: invalid syntax (1)"));

			webauth = webauth.substr(idx + 1);
			user = base64_decode(webauth);

			if((idx = user.find(':')) == string::npos)
				throw(string("ClientSocket: web authentication: invalid syntax (2)"));

			password = user.substr(idx + 1);
			user = user.substr(0, idx);

			Util::vlog("ClientSocket: authentication: %s,%s", user.c_str(), password.c_str());

			if(!validate_user(user, password, require_auth_group))
				throw(string("Invalid authentication"));
		}

		for(headit = headers.begin(); headit != headers.end(); headit++)
			Util::vlog("ClientSocket: header[%s]: \"%s\"", headit->first.c_str(), headit->second.c_str());

		Util::vlog("ClientSocket: url: %s", url.c_str());

		urlparams = Url(url).split();

		for(param_it = urlparams.begin(); param_it != urlparams.end(); param_it++)
			Util::vlog("ClientSocket: parameter[%s] = \"%s\"", param_it->first.c_str(), param_it->second.c_str());

		if(urlparams.count("startfrom"))
			time_offset = TimeOffset(urlparams["startfrom"]).as_seconds();
		else
			time_offset = 0;

		if(urlparams.count("size"))
			frame_size = urlparams["size"];

		if(urlparams.count("bitrate"))
			bitrate = urlparams["bitrate"];

		if(urlparams.count("profile"))
			profile = urlparams["profile"];

		if(urlparams.count("level"))
			level = urlparams["level"];

		if(urlparams.count("bframes"))
			bframes = urlparams["bframes"];

		if((urlparams[""] == "/livestream") && urlparams.count("service"))
		{
			Service service(urlparams["service"]);

			Util::vlog("ClientSocket: live streaming request");
			(void)LiveStreaming(service, fd, webauth);
			Util::vlog("ClientSocket: live streaming ends");

			return;
		}

		if((urlparams[""] == "/live") && urlparams.count("service"))
		{
			Service service(urlparams["service"]);

			Util::vlog("ClientSocket: live transcoding request");
			(void)LiveTranscoding(service, fd, webauth, frame_size,
					bitrate, profile, level, bframes);
			Util::vlog("ClientSocket: live transcoding ends");

			return;
		}

		if((urlparams[""] == "/filestream") && urlparams.count("file"))
		{
			Util::vlog("ClientSocket: file streaming request");
			(void)FileStreaming(urlparams["file"], fd, time_offset);
			Util::vlog("ClientSocket: file streaming ends");

			return;
		}

		if((urlparams[""] == "/file") && urlparams.count("file"))
		{
			Util::vlog("ClientSocket: file transcoding request");
			(void)FileTranscoding(urlparams["file"], fd, time_offset, frame_size,
					bitrate, profile, level, bframes);
			Util::vlog("ClientSocket: file transcoding ends");

			return;
		}

		if(urlparams[""].length() > 1)
		{
			if((urlparams[""].substr(1, 1) == "/"))
			{
				Util::vlog("ClientSocket: default file request");

				if(default_action == action_stream)
				{
					Util::vlog("ClientSocket: streaming file");
					(void)FileStreaming(urlparams["file"], fd, time_offset);
				}
				else
				{
					Util::vlog("ClientSocket: transcoding file");
					(void)FileTranscoding(urlparams["file"], fd, time_offset, frame_size,
							bitrate, profile, level, bframes);
				}

				Util::vlog("ClientSocket: default file ends");

				return;
			}
			else
			{
				Service service(urlparams[""].substr(1));

				Util::vlog("ClientSocket: default live request");

				if(service.is_valid())
				{
					if(default_action == action_stream)
					{
						Util::vlog("ClientSocket: streaming service");
						(void)LiveStreaming(service, fd, webauth);
					}
					else
					{
						Util::vlog("ClientSocket: transcoding service");
						(void)LiveTranscoding(service, fd, webauth, frame_size,
							bitrate, profile, level, bframes);
					}

					Util::vlog("ClientSocket: default live ends");

					return;
				}
			}
		}

		throw(string("ClientSocket: unknown request: ") + urlparams[""]);
	}
	catch(const string &e)
	{
		Util::vlog("ClientSocket: exception: %s", e.c_str());
		reply = string("HTTP/1.0 400 Bad request: ") + e + "\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
		write(fd, reply.c_str(), reply.length());
	}
	catch(const std::exception &e)
	{
		Util::vlog("ClientSocket: unknown std exception: %s", typeid(e).name());
		reply = "HTTP/1.0 400 Bad request\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
		write(fd, reply.c_str(), reply.length());
	}
	catch(...)
	{
		Util::vlog("ClientSocket: unknown exception");
		reply = "HTTP/1.0 400 Bad request\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
		write(fd, reply.c_str(), reply.length());
	}
}

ClientSocket::~ClientSocket() throw()
{
	close(fd);
}

/*
	base64.cpp and base64.h

	Copyright (C) 2004-2008 René Nyffenegger

	This source code is provided 'as-is', without any express or implied
	warranty. In no event will the author be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this source code must not be misrepresented; you must not
		claim that you wrote the original source code. If you use this source code
		in a product, an acknowledgment in the product documentation would be
		appreciated but is not required.

	2. Altered source versions must be plainly marked as such, and must not be
		misrepresented as being the original source code.

	3. This notice may not be removed or altered from any source distribution.

	René Nyffenegger rene.nyffenegger@adp-gmbh.ch
*/

const std::string ClientSocket::base64_chars =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

bool ClientSocket::is_base64(unsigned char c)
{
	return(isalnum(c) || (c == '+') || (c == '/'));
}

std::string ClientSocket::base64_decode(const std::string &encoded_string) throw()
{
	int in_len = encoded_string.size();
	int i = 0;
	int j = 0;
	int in_ = 0;
	unsigned char char_array_4[4], char_array_3[3];
	std::string ret;

	while(in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
	{
		char_array_4[i++] = encoded_string[in_];
		in_++;

		if(i ==4)
		{
			for(i = 0; i <4; i++)
				char_array_4[i] = base64_chars.find(char_array_4[i]);

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for(i = 0; (i < 3); i++)
				ret += char_array_3[i];
			i = 0;
		}
	}

	if(i)
	{
		for (j = i; j <4; j++)
			char_array_4[j] = 0;

		for (j = 0; j <4; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]);

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for(j = 0; (j < i - 1); j++)
			ret += char_array_3[j];
	}

	return ret;
}

bool ClientSocket::validate_user(string user, string password, string require_auth_group) throw()
{
	struct passwd	*pw;
	struct group	*gr;
	struct spwd		*sp;
	gid_t			user_gid;
	int				ix;
	const char 		*group_user;
	bool			found;
	string			primary_group;
	string			encrypted;
	string			pw_password;

	if(!(pw = getpwnam(user.c_str())))
	{
		Util::vlog("ClientSocket: user %s not in passwd file");
		return(false);
	}

	user_gid = pw->pw_gid;
	pw_password = pw->pw_passwd;

	//Util::vlog("user_gid: %d, pw: %s", user_gid, pw_password.c_str());

	if((sp = getspnam(user.c_str())))
	{
		//Util::vlog("shadow pw: %s", pw_password.c_str());
		pw_password = sp->sp_pwdp;
	}

	if(require_auth_group.length())
	{
		if(!(gr = getgrnam(require_auth_group.c_str())))
		{
			//Util::vlog("ClientSocket: group %s not in groups file", require_auth_group.c_str());
			return(false);
		}

		found = false;

		if(gr->gr_gid == user_gid)
		{
			//Util::vlog("group in primary group of user, ok");
			found = true;
		}
		else
		{
			for(ix = 0; (group_user = gr->gr_mem[ix]); ix++)
			{
				//Util::vlog("checking as secondary group id, user %s", group_user);

				if(group_user == user)
				{
					found = true;
					//Util::vlog("group in secondary group of user, ok");
					break;
				}
			}
		}

		if(!found)
		{
			//Util::vlog("user not in group, reject");
			return(false);
		}
	}

	encrypted = crypt(password.c_str(), pw_password.c_str());

	return(encrypted == pw_password);
}
