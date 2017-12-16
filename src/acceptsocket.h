#ifndef _acceptsocket_h_
#define _acceptsocket_h_

#include "config.h"
#include "trap.h"

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
			AcceptSocket(string port);
			~AcceptSocket();
		int	accept()			const;
		int	get_fd()			const;
};

#endif
