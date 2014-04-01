#ifndef _acceptsocket_h_
#define _acceptsocket_h_

#include <string>
using std::string;

class AcceptSocket
{
	public:
			AcceptSocket(string port)	throw(string);
		int	accept()			const	throw(string);

	private:

		int fd;

	private:

		AcceptSocket(const AcceptSocket &); // delete copy constructor
};

#endif
