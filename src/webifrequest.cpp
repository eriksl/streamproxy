#include "config.h"
#include "trap.h"

#include "types.h"
#include "webifrequest.h"
#include "util.h"
#include "configmap.h"

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <boost/algorithm/string.hpp>

#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/sockios.h>
#include <stdlib.h>

static const struct addrinfo gai_webif_hints =
{
	.ai_flags       = AI_NUMERICHOST,
	.ai_family      = AF_INET6,
	.ai_socktype    = SOCK_STREAM,
	.ai_protocol    = 0,
	.ai_addrlen     = 0,
	.ai_addr        = 0,
	.ai_canonname   = 0,
	.ai_next        = 0,
};

WebifRequest::WebifRequest(const Service &service_in,
			string webauth, const ConfigMap &config_map_in)
	:
		service(service_in), config_map(config_map_in)
{
	struct addrinfo *gai_webif_address;
	struct linger so_linger;
	int rv;
	string request;

	fd			= -1;
	demuxer_id	= -1;

	if((rv = getaddrinfo("::", config_map.at("webifport").string_value.c_str(),
					&gai_webif_hints, &gai_webif_address)))
		throw(trap(string("WebifRequest: cannot get address for localhost") + gai_strerror(rv)));

	if(!gai_webif_address)
		throw(trap("WebifRequest: cannot get address for localhost"));

	if((fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
	{
		freeaddrinfo(gai_webif_address);
		throw(trap("WebifRequest: cannot create socket"));
	}

	if(setsockopt(fd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)))
	{
		freeaddrinfo(gai_webif_address);
		throw(trap("WebifRequest: cannot set linger"));
	}

	if(connect(fd, gai_webif_address->ai_addr, gai_webif_address->ai_addrlen))
	{
		freeaddrinfo(gai_webif_address);
		throw(trap("WebifRequest: cannot connect"));
	}

	freeaddrinfo(gai_webif_address);

	request = string("GET /web/stream?StreamService=") + service.service_string() + " HTTP/1.0\r\n";

	if(webauth.length())
		request += "Authorization: Basic " + webauth + "\r\n";

	request += "\r\n";

	Util::vlog("WebifRequest: send request to webif: \"%s\"", request.c_str());

	if(write(fd, request.c_str(), request.length()) != (ssize_t)request.length())
		throw(trap("WebifRequest: cannot send request"));
}

WebifRequest::~WebifRequest()
{
	close(fd);
}

void WebifRequest::poll()
{
	typedef vector<string> stringvector;

	int		bytes;
	ssize_t	bytes_read;
	size_t	sol, eol, idx;
	char	*buffer;
	string	demuxer;
	string	line;
	string	id, newid;
	string	valuestr;
	int		value;
	int		ord;

	stringvector tokens;
	stringvector::const_iterator it;

	if(ioctl(fd, SIOCINQ, &bytes))
		throw(trap("WebifRequest: ioctl SIOCINQ"));

	if(bytes == 0)
		return;

	buffer = (char *)malloc(bytes + 1);

	if((bytes_read = read(fd, buffer, bytes)) != bytes)
	{
		Util::vlog("WebifRequest: read returns %d", bytes_read);
		free(buffer);
		return;
	}

	buffer[bytes_read] = '\0';
	reply += buffer;
	free(buffer);

	if((sol = reply.rfind('+')) == string::npos)
		return;

	if((eol = reply.find('\n', sol)) == string::npos)
		return;

	line = reply.substr(sol, eol - sol);

	if(line.length() < 3)
		return;

	if(line[0] != '+')
		return;

	if((idx = line.find(":")) == string::npos)
		return;

	demuxer		= line.substr(1, idx - 1);
	line		= line.substr(idx + 1);
	demuxer_id	= strtoul(demuxer.c_str(), 0, 16);

	pids.clear();
	split(tokens, line, boost::is_any_of(","));

	for(it = tokens.begin(); it != tokens.end(); it++)
	{
		if((idx = it->find(':')) != string::npos)
		{
			id			= it->substr(idx + 1);
			valuestr	= it->substr(0, idx);
			value		= strtoul(valuestr.c_str(), 0, 16);

			if(pids.find(id) != pids.end())
			{
				for(ord = 1; ord < 32; ord++)
				{
					newid = id + "-" + Util::int_to_string(ord);
					
					if(pids.find(newid) == pids.end())
						break;
				}

				if(ord == 32)
					continue;

				id = newid;
			}

			pids[id] = value;
		}
	}
}

PidMap WebifRequest::get_pids() const
{
	return(pids);
}

int WebifRequest::get_demuxer_id() const
{
	return(demuxer_id);
}
