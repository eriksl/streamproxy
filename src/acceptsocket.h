#ifndef _acceptsocket_h_
#define _acceptsocket_h_

#include <string>
using std::string;

class AcceptSocket
{
	private:

		int fd;

		AcceptSocket();
		AcceptSocket(const AcceptSocket &);

	public:
			AcceptSocket(string port)	throw(string);
		int	accept()			const	throw(string);
};

#endif
