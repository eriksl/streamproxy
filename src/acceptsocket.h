#ifndef _acceptsocket_h_
#define _acceptsocket_h_

#include "config.h"

#include <string>
using std::string;

class AcceptSocket
{
	private:

		int				fd;
		struct addrinfo *gai_accept_address;

		AcceptSocket();
		AcceptSocket(const AcceptSocket &);

	public:
			AcceptSocket(string port)	throw(string);
			~AcceptSocket()				throw();
		int	accept()			const	throw(string);
		int	get_fd()			const	throw();
};

#endif
