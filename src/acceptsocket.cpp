#include "acceptsocket.h"

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>

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
	int rv;

	gai_accept_address = 0;

	if((fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
		throw("AcceptSocket: cannot create accept socket");

	if(setsockopt(fd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)))
		throw(string("AcceptSocket: cannot set linger on accept socket"));

	rv = 1;

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &rv, sizeof(rv)))
		throw(string("AcceptSocket: cannot set reuseaddr on accept socket"));

	if((rv = getaddrinfo(0, port.c_str(), &gai_accept_hints, &gai_accept_address)))
		throw(string("AcceptSocket: cannot get address info: ") + gai_strerror(rv));

	if(!gai_accept_address)
		throw(string("AcceptSocket: cannot get address info: ") + gai_strerror(rv));

	if(bind(fd, gai_accept_address->ai_addr, gai_accept_address->ai_addrlen))
		throw(string("AcceptSocket: cannot bind accept socket"));

	if(listen(fd, 4))
		throw(string("AcceptSocket: cannot listen on accept socket"));
}

AcceptSocket::~AcceptSocket() throw()
{
	if(gai_accept_address)
		freeaddrinfo(gai_accept_address);

	if(fd >= 0)
		close(fd);
}

int AcceptSocket::accept() const throw(string)
{
	int new_fd;

	if((new_fd = ::accept(fd, 0, 0)) < 0)
		throw(string("AcceptSocket: error in accept"));

	return(new_fd);
}
