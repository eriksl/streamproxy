#include "config.h"
#include "trap.h"

#include <string>
using std::string;

#include <exception>
using std::exception;

trap::trap(string message_in) throw()
		:
	message(message_in)
{
	message += string(" (") + exception::what() + ")";
}

const char *trap::what() const throw()
{
	return(message.c_str());
}

trap::~trap() throw()
{
}

http_trap::http_trap(string message_in, int http_error_in, string http_header_in) throw()
		:
	trap(message_in),
	http_error(http_error_in),
	http_header(http_header_in)
{
}

const char *http_trap::what() const throw()
{
	return(message.c_str());
}

http_trap::~http_trap() throw()
{
}
