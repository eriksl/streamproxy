#include "config.h"
#include "trap.h"

#include "clientsocket.h"
#include "service.h"
#include "livestreaming.h"
#include "livetranscoding-broadcom.h"
#include "filestreaming.h"
#include "filetranscoding-broadcom.h"
#include "transcoding-enigma.h"
#include "util.h"
#include "url.h"
#include "types.h"
#include "webrequest.h"
#include "stbtraits.h"

#include <stdlib.h>
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

#include <string>
using std::string;
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>

ClientSocket::ClientSocket(int fd_in,
		default_streaming_action default_action,
		const ConfigMap &config_map_in,
		const stb_traits_t &stb_traits_in)
	:
		fd(fd_in), config_map(config_map_in),
		stb_traits(stb_traits_in)
{
	const char *http_error_headers =
		"Content-Type: text/html\r\n"
		"Connection: close\r\n"
		"Accept-Ranges: bytes\r\n"
		"\r\n";

	string	reply, message;
	try
	{
		static char			read_buffer[1024];
		ssize_t				bytes_read;
		size_t				idx = string::npos;
		string				header, cookie, value;
		struct				pollfd pfd;
		time_t				start;
		string				webauth, user, password;
		string				range_header;
		string				mimetype;
		stringvector		lines;
		stringvector		tokens;

		stringvector::iterator it1;
		stringvector::const_iterator it2;
		HeaderMap::const_iterator headit;
		CookieMap::const_iterator cookit;
		StreamingParameters::const_iterator spit;

		int arg1;

		arg1 = fcntl(fd, F_GETFL, 0);
		if(fcntl(fd, F_SETFL, arg1 | O_NONBLOCK))
			throw(trap("ClientSocket: F_SETFL"));

#if 0
		arg1 = 1;

		if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&arg1, sizeof(arg1)))
			throw(trap("ClientSocket: cannot set NODELAY"));
#endif

#if 0
		int arg2;

		arg1 = 0;
		arg2 = sizeof(arg1);

		if(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&arg1, (size_t *)&arg2))
			throw(trap("ClientSocket: getsockopt 1"));

		Util::vlog("ClientSocket: current buffer size: %d", arg1);

		arg1 = 256 * 1024;

		if(setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&arg1, sizeof(arg1)))
			throw(trap("ClientSocket: setsockopt"));

		if(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&arg1, (size_t *)&arg2))
			throw(trap("ClientSocket: getsockopt 2"));

		Util::vlog("ClientSocket: new buffer size: %d", arg1);
#endif

		start = time(0);

		for(;;)
		{
			pfd.fd = fd;
			pfd.events = POLLIN | POLLRDHUP;

			if(poll(&pfd, 1, 1000) < 0)
				throw(trap("ClientSocket: poll error"));

			if(pfd.revents & (POLLHUP | POLLRDHUP | POLLERR | POLLNVAL))
				throw(trap("ClientSocket: peer disconnects"));

			if(pfd.revents & POLLIN)
			{
				if((bytes_read = read(fd, read_buffer, sizeof(read_buffer))) <= 0)
					throw(trap("ClientSocket: peer disconnected"));

				request.append(read_buffer, bytes_read);

				if((idx = request.find("\r\n\r\n")) != string::npos)
					break;
			}

			if((time(0) - start) > 5)
				throw(trap("ClientSocket: peer connection timeout"));
		}

		if(idx == string::npos)
			throw(http_trap("ClientSocket: no request", 400, "Bad request 1"));

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
				boost::algorithm::to_lower(header);

				if((idx = it1->find_first_not_of(": ", idx)) != string::npos)
				{
					value = it1->substr(idx);

					if(header == "cookie")
					{
						//Util::vlog("cookie: \"%s\", value: \"%s\"", header.c_str(), value.c_str());

						split(tokens, value, boost::is_any_of(";"));

						for(it2 = tokens.begin(); it2 != tokens.end(); it2++)
						{
							if((idx = it2->find('=')) != string::npos)
							{
								cookie	= it2->substr(0, idx);
								value	= it2->substr(idx + 1);

								boost::trim(cookie);
								boost::trim(value);

								//Util::vlog("cookie \"%s\"=\"%s\"", cookie.c_str(), value.c_str());
								cookies[cookie] = value;
							}
						}
					}
					else
						headers[header] = it1->substr(idx);
				}

				continue;
			}

			Util::vlog("ClientSocket: ignoring junk line: %s\n", it1->c_str());
		}

		if(lines.size() < 1)
			throw(http_trap("ClientSocket: invalid request", 400, "Bad request 2"));

		if(config_map.at("auth").int_value)
		{
			if(!headers.count("authorization"))
			{
				reply = string("Unauthorized\r\nWWW-Authenticate: Basic realm=\"OpenWebif\"\r\n");
				throw(http_trap("ClientSocket: no authorisation received from client", 401, reply));
			}

			webauth = headers["authorization"];

			if((idx = webauth.find(' ')) == string::npos)
				throw(http_trap("ClientSocket: web authentication: invalid syntax (1)", 400, "Bad request: invalid auth syntax 1"));

			webauth = webauth.substr(idx + 1);
			user = base64_decode(webauth);

			if((idx = user.find(':')) == string::npos)
				throw(http_trap("ClientSocket: web authentication: invalid syntax (2)", 400, "Bad request: invalid auth syntax 2"));

			password = user.substr(idx + 1);
			user = user.substr(0, idx);

			Util::vlog("ClientSocket: authentication: %s,%s", user.c_str(), password.c_str());

			if(!validate_user(user, password, config_map.at("group").string_value))
				throw(http_trap("Invalid authentication", 403, "Forbidden, invalid authentication"));
		}

		if(headers.count("range"))
		{
			range_header = headers.at("range");

			if((range_header.length() > 7) && (range_header.substr(0, 6) == "bytes="))
			{
				range_header = range_header.substr(6);

				if(range_header.find('-') == (range_header.length() - 1))
					streaming_parameters["http_range"] = range_header;
			}
		}

		for(headit = headers.begin(); headit != headers.end(); headit++)
			Util::vlog("ClientSocket: header[%s]: \"%s\"", headit->first.c_str(), headit->second.c_str());

		for(cookit = cookies.begin(); cookit != cookies.end(); cookit++)
			Util::vlog("ClientSocket: cookie[%s]: \"%s\"", cookit->first.c_str(), cookit->second.c_str());

		Util::vlog("ClientSocket: url: %s", url.c_str());

		urlparams = Url(url).split();

		Util::vlog("\nclientsocket: streaming parameters before defaults from config:");
		for(spit = streaming_parameters.begin(); spit != streaming_parameters.end(); spit++)
			Util::vlog("    %s = %s\n", spit->first.c_str(), spit->second.c_str());

		check_add_defaults_from_config();

		Util::vlog("\nclientsocket: streaming parameters after defaults:");
		for(spit = streaming_parameters.begin(); spit != streaming_parameters.end(); spit++)
			Util::vlog("    %s = %s", spit->first.c_str(), spit->second.c_str());

		check_add_urlparams();

		Util::vlog("\nclientsocket: streaming parameters after url params:");
		for(spit = streaming_parameters.begin(); spit != streaming_parameters.end(); spit++)
			Util::vlog("    %s = %s", spit->first.c_str(), spit->second.c_str());

		add_default_params();

		Util::vlog("\nclientsocket: streaming parameters after setting default params:");
		for(spit = streaming_parameters.begin(); spit != streaming_parameters.end(); spit++)
			Util::vlog("    %s = %s", spit->first.c_str(), spit->second.c_str());

		if((urlparams[""] == "/livestream") && urlparams.count("service"))
		{
			Service service(urlparams["service"]);

			Util::vlog("ClientSocket: live streaming request");
			(void)LiveStreaming(service, fd, streaming_parameters, config_map);
			Util::vlog("ClientSocket: live streaming ends");

			return;
		}

		if((urlparams[""] == "/live") && urlparams.count("service"))
		{
			Service service(urlparams["service"]);

			Util::vlog("ClientSocket: live transcoding request");

			switch(stb_traits.transcoding_type)
			{
				case(stb_transcoding_broadcom):
				{
					Util::vlog("ClientSocket: transcoding service broadcom");
					(void)LiveTranscodingBroadcom(service, fd, stb_traits, streaming_parameters, config_map);
					break;
				}

				case(stb_transcoding_enigma):
				{
					Util::vlog("ClientSocket: transcoding service enigma");
					(void)TranscodingEnigma(service.service_string(), fd, webauth, stb_traits, streaming_parameters);
					break;
				}

				default:
				{
					throw(http_trap(string("not a supported stb for transcoding"), 400, "Bad request"));
				}
			}

			Util::vlog("ClientSocket: live transcoding ends");

			return;
		}

		if((urlparams[""] == "/filestream") && urlparams.count("file"))
		{
			Util::vlog("ClientSocket: file streaming request");
			(void)FileStreaming(urlparams["file"], fd, webauth, streaming_parameters, config_map);
			Util::vlog("ClientSocket: file streaming ends");

			return;
		}

		if((urlparams[""] == "/file") && urlparams.count("file"))
		{
			Util::vlog("ClientSocket: file transcoding request");

			switch(stb_traits.transcoding_type)
			{
				case(stb_transcoding_broadcom):
				{
					Util::vlog("ClientSocket: transcoding service broadcom");
					(void)FileTranscodingBroadcom(urlparams["file"], fd, webauth, stb_traits, streaming_parameters, config_map);
					break;
				}

				case(stb_transcoding_enigma):
				{
					string service(string("1:0:1:0:0:0:0:0:0:0:") + Url(urlparams.at("file")).encode());

					Util::vlog("ClientSocket: transcoding service enigma");
					(void)TranscodingEnigma(service, fd, webauth, stb_traits, streaming_parameters);
					break;
				}

				default:
				{
					throw(http_trap(string("not a supported stb for transcoding"), 400, "Bad request"));
				}
			}

			Util::vlog("ClientSocket: file transcoding ends");

			return;
		}

		if(urlparams[""] == "/")
		{
			urlparams[""] = "/web";
			urlparams["request"] = "info";
		}

		if(urlparams[""] == "/web")
		{
			Util::vlog("ClientSocket: request for web");

			string data;
			WebRequest webrequest(config_map, headers, cookies, urlparams, stb_traits);

			data = webrequest.get(mimetype);

			if(!data.length())
				throw(http_trap("ClientSocket: web request failed", 500, "Internal server error (handler failed)"));

			reply =	"HTTP/1.1 200 OK\r\n";
			reply += "Content-type: " + mimetype + "; charset=utf8\r\n";
			reply += "Connection: close\r\n\r\n";
			reply += data;

			write(fd, reply.c_str(), reply.length());

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
					(void)FileStreaming(urlparams["file"], fd, webauth, streaming_parameters, config_map);
				}
				else
				{
					switch(stb_traits.transcoding_type)
					{
						case(stb_transcoding_broadcom):
						{
							Util::vlog("ClientSocket: transcoding service broadcom");
							(void)FileTranscodingBroadcom(urlparams["file"], fd, webauth, stb_traits, streaming_parameters, config_map);
							break;
						}

						case(stb_transcoding_enigma):
						{
							string service(string("1:0:1:0:0:0:0:0:0:0:") + Url(urlparams.at("file")).encode());

							Util::vlog("ClientSocket: transcoding service enigma");
							(void)TranscodingEnigma(service, fd, webauth, stb_traits, streaming_parameters);
							break;
						}

						default:
						{
							throw(http_trap(string("not a supported stb for transcoding"), 400, "Bad request"));
						}
					}
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
						(void)LiveStreaming(service, fd, streaming_parameters, config_map);
					}
					else
					{
						Util::vlog("ClientSocket: default live transcoding request");

						switch(stb_traits.transcoding_type)
						{
							case(stb_transcoding_broadcom):
							{
								Util::vlog("ClientSocket: transcoding service broadcom");
								(void)LiveTranscodingBroadcom(service, fd, stb_traits, streaming_parameters, config_map);
								break;
							}

							case(stb_transcoding_enigma):
							{
								Util::vlog("ClientSocket: transcoding service enigma");
								(void)TranscodingEnigma(service.service_string(), fd, webauth, stb_traits, streaming_parameters);
								break;
							}

							default:
							{
								throw(http_trap(string("not a supported stb for transcoding"), 400, "Bad request"));
							}
						}
					}

					Util::vlog("ClientSocket: default live ends");

					return;
				}
			}
		}

		throw(http_trap(string("unknown url: ") + urlparams[""], 404, "Not found"));
	}
	catch(const http_trap &e)
	{
		reply = string("HTTP/1.1 ") + Util::int_to_string(e.http_error) + " " + e.http_header;
		reply += string("\r\n") + http_error_headers;

		message = reply;
		boost::replace_all(message, "\r\n", "@");
		Util::vlog("ClientSocket: http_trap: %s (%d: %s)", e.what(), e.http_error, message.c_str());

		write(fd, reply.c_str(), reply.length());
	}
	catch(const trap &e)
	{
		reply = string("HTTP/1.1 400 Bad request: ") + e.what() + "\r\n";
		reply += http_error_headers;
		Util::vlog("ClientSocket: trap: %s", e.what());
		write(fd, reply.c_str(), reply.length());
	}
	catch(...)
	{
		reply = string("HTTP/1.1 400 Bad request") + "\r\n";
		reply += http_error_headers;
		Util::vlog("ClientSocket: unknown exception");
		write(fd, reply.c_str(), reply.length());
	}
}

ClientSocket::~ClientSocket()
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

std::string ClientSocket::base64_decode(const std::string &encoded_string)
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

bool ClientSocket::validate_user(string user, string password, string require_auth_group)
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

bool ClientSocket::get_feature_value(string feature_name, string value_in, string &value_out, string &api_data) const
{
	const stb_feature_t *feature = 0;
	size_t				feature_idx;

	for(feature_idx = 0; feature_idx < stb_traits.num_features; feature_idx++)
	{
		feature = &stb_traits.features[feature_idx];

		if((feature_name == feature->id) && feature->settable)
			break;
	}

	if(feature_idx >= stb_traits.num_features)
		return(false);

	if(feature->api_data)
		api_data = feature->api_data;

	switch(feature->type)
	{
		case(stb_traits_type_bool):
		{
			if((value_in == "false") || (value_in == "off") || (value_in == "0"))
				value_out = "0";
			else
				if((value_in == "true") || (value_in == "on") || (value_in == "1"))
					value_out = "1";
				else
					return(false);

			return(true);
		}

		case(stb_traits_type_int):
		{
			int64_t value = Util::string_to_int(value_in);

			if((feature->value.int_type.min_value <= value) && (value <= feature->value.int_type.max_value))
			{
				value_out = Util::int_to_string(value);
				return(true);
			}

			if(feature->value.int_type.scaling_factor > 1)
			{
				value /= feature->value.int_type.scaling_factor;

				if((feature->value.int_type.min_value <= value) && (value <= feature->value.int_type.max_value))
				{
					value_out = Util::int_to_string(value);
					return(true);
				}
			}

			return(false);
		}

		case(stb_traits_type_string):
		{
			if((feature->value.string_type.min_length <= value_in.length()) && (value_in.length() <= feature->value.string_type.max_length))
			{
				value_out = value_in;
				return(true);
			}

			return(false);
		}

		case(stb_traits_type_string_enum):
		{
			int			item_ix;
			const char	*item;

			for(item_ix = 0; item_ix < 64; item_ix++)
			{
				if(!(item = feature->value.string_enum_type.enum_values[item_ix]))
					break;

				if(value_in == item)
					break;
			}

			if(item)
			{
				value_out = item;
				return(true);
			}

			return(false);
		}
	}

	return(false);
}

void ClientSocket::check_add_defaults_from_config()
{
	ConfigMap::const_iterator	it;
	string						value_out;
	string						api_data;

	for(it = config_map.begin(); it != config_map.end(); it++)
	{
		if(get_feature_value(it->first, it->second.string_value, value_out, api_data))
		{
			streaming_parameters[it->first] = value_out;
			Util::vlog("clientsocket: accept config default %s = %s", it->first.c_str(), it->second.string_value.c_str());
		}
		else
			Util::vlog("clientsocket: reject config default %s = %s", it->first.c_str(), it->second.string_value.c_str());
	}
}

void ClientSocket::add_default_params()
{
	const stb_feature_t *feature = 0;
	size_t				feature_idx;
	string				value;

	for(feature_idx = 0; feature_idx < stb_traits.num_features; feature_idx++)
	{
		feature = &stb_traits.features[feature_idx];

		if(!feature->settable)
			continue;

		if(streaming_parameters.count(feature->id))
		{
			Util::vlog("clientsocket: reject default %s, it is already set to %s",
					feature->id, streaming_parameters.at(feature->id).c_str());
		}
		else
		{
			switch(feature->type)
			{
				case(stb_traits_type_bool):
				{
					if(feature->value.bool_type.default_value)
						value = "on";
					else
						value = "off";

					break;
				}

				case(stb_traits_type_int):
				{
					value = Util::int_to_string(feature->value.int_type.default_value);

					break;
				}

				case(stb_traits_type_string):
				{
					value = feature->value.string_type.default_value;

					break;
				}

				case(stb_traits_type_string_enum):
				{
					value = feature->value.string_enum_type.default_value;

					break;
				}

				default:
				{
					value = "<unknown>";

					break;
				}
			}

			Util::vlog("clientsocket: accept default %s = %s",
					feature->id, value.c_str());

			streaming_parameters[feature->id] = value;
		}
	}
}

void ClientSocket::check_add_urlparams()
{
	UrlParameterMap::const_iterator	it;
	string							param, value;
	string							value_out;
	string							api_data;

	for(it = urlparams.begin(); it != urlparams.end(); it++)
	{
		if(it->first == "")
			continue;

		Util::vlog("clientsocket: get parameter[%s] = \"%s\"", it->first.c_str(), it->second.c_str());

		if((it->first == "startfrom") || (it->first == "pct_offset") ||
				(it->first == "byte_offset"))
		{
			streaming_parameters[it->first] = it->second;

			Util::vlog("ClientSocket: set aspecific streaming parameter[%s] = \"%s\"",
					it->first.c_str(), streaming_parameters.at(it->first).c_str());

			continue;
		}

		param = it->first;
		value = it->second;

		if(param == "height") // workaround for Xtrend streaming
		{
			Util::vlog("ClientSocket: Xtrend workaround active");

			if(value == "224")
			{
				param = "size";
				value = "416x224";
			}
			else if(value == "480")
			{
				param = "size";
				value = "480p";
			}
			else if(value == "576")
			{
				param = "size";
				value = "576p";
			}
			else if(value == "720")
			{
				param = "size";
				value = "720p";
			}
		}

		if(get_feature_value(param, value, value_out, api_data))
		{
			Util::vlog("clientsocket: accept streaming specific param %s = %s", param.c_str(), value.c_str());
			streaming_parameters[param] = value_out;
		}
		else
			Util::vlog("clientsocket: reject streaming specific param %s = %s", param.c_str(), value.c_str());
	}
}
