#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/sockios.h>
#include <stdlib.h>

#include "pidmap.h"
#include "webifrequest.h"
#include "vlog.h"

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <boost/algorithm/string.hpp>

static const struct addrinfo gai_webif_hints =
{
	.ai_flags       = AI_NUMERICHOST | AI_NUMERICSERV,
	.ai_family      = AF_INET,
	.ai_socktype    = SOCK_STREAM,
	.ai_protocol    = 0,
	.ai_addrlen     = 0,
	.ai_addr        = 0,
	.ai_canonname   = 0,
	.ai_next        = 0,
};

WebifRequest::WebifRequest(const Service &service_in) throw(string) :
		service(service_in)
{
	struct addrinfo *gai_webif_address;
	struct linger so_linger;
	int rv;
	string request;

	fd			= -1;
	demuxer_id	= -1;

	if((rv = getaddrinfo("127.0.0.1", "80", &gai_webif_hints, &gai_webif_address)))
		throw(string("webifrequest: cannot get address for localhost") + gai_strerror(rv));

	if(!gai_webif_address)
		throw(string("webifrequest: cannot get address for localhost"));

	if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		freeaddrinfo(gai_webif_address);
		throw(string("webifrequest: cannot create socket"));
	}

	if(setsockopt(fd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)))
	{
		freeaddrinfo(gai_webif_address);
		throw(string("webifrequest: cannot set linger"));
	}

	if(connect(fd, gai_webif_address->ai_addr, gai_webif_address->ai_addrlen))
	{
		freeaddrinfo(gai_webif_address);
		throw(string("webifrequest: cannot connect"));
	}

	freeaddrinfo(gai_webif_address);

	request = string("GET /web/stream?StreamService=") + service.service_string() + " HTTP/1.0\r\n\r\n";

	if(write(fd, request.c_str(), request.length()) != (ssize_t)request.length())
		throw(string("webifrequest: cannot send request"));
}

WebifRequest::~WebifRequest()
{
	close(fd);
}

void WebifRequest::poll() throw(string)
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
	char	ordstr[8];
	int		ord;

	stringvector tokens;
	stringvector::const_iterator it;

	if(ioctl(fd, SIOCINQ, &bytes))
		throw(string("webifrequest: ioctl SIOCINQ"));

	if(bytes == 0)
		return;

	buffer = (char *)malloc(bytes + 1);

	if((bytes_read = read(fd, buffer, bytes)) != bytes)
	{
		vlog("webifrequest: read returns %d", bytes_read);
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
					snprintf(ordstr, sizeof(ordstr), "%d", ord);
					newid = id + "-" + ordstr;
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

PidMap WebifRequest::get_pids() const throw()
{
	return(pids);
}

int WebifRequest::get_demuxer_id() const throw()
{
	return(demuxer_id);
}
