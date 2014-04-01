#include "acceptsocket.h"

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/dvb/dmx.h>

static const struct addrinfo gai_accept_hints =
{
	.ai_flags		= AI_PASSIVE,
	.ai_family		= AF_INET6,
	.ai_socktype	= SOCK_STREAM,
	.ai_protocol	= 0,
	.ai_addrlen		= 0,
	.ai_addr		= 0,
	.ai_canonname	= 0,
	.ai_next		= 0,
};

static const struct linger so_linger =
{
	.l_onoff	= 0,
	.l_linger	= 0,
};

AcceptSocket::AcceptSocket(string port) throw(string)
{
	struct addrinfo *gai_accept_address;
	int rv;

	if((fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
		throw("cannot create accept socket");

	if(setsockopt(fd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)))
	{
		close(fd);
		throw("cannot set linger on accept socket");
	}

	rv = 1;

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &rv, sizeof(rv)))
	{
		close(fd);
		throw("cannot set reuseaddr on accept socket");
	}

	if((rv = getaddrinfo(0, port.c_str(), &gai_accept_hints, &gai_accept_address)))
	{
		close(fd);
		throw(string("cannot get address info: ") + gai_strerror(rv));
	}

	if(!gai_accept_address)
	{
		close(fd);
		throw(string("cannot get address info: ") + gai_strerror(rv));
	}

	if(bind(fd, gai_accept_address->ai_addr, gai_accept_address->ai_addrlen))
	{
		close(fd);
		freeaddrinfo(gai_accept_address);
		throw("cannot bind accept socket");
	}

	freeaddrinfo(gai_accept_address);

	if(listen(fd, 4))
	{
		close(fd);
		throw("cannot listen on accept socket");
	}
}

int AcceptSocket::accept() const throw(string)
{
	int new_fd;

	if((new_fd = ::accept(fd, 0, 0)) < 0)
		throw("error in accept");

	return(new_fd);
}
