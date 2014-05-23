#ifndef _trap_h_
#define _trap_h_

#include <string>
#include <exception>

class trap : public std::exception
{
	public:

		std::string	message;

		trap(std::string msg)								throw();
		virtual	const char *what()					const	throw();
		virtual	~trap()										throw();
};

class http_trap : public trap
{
	public:

		int			http_error;
		std::string	http_header;

		http_trap(std::string msg,
				int http_error, std::string http_header)	throw();

		virtual	const char *what()					const	throw();
		virtual	~http_trap()								throw();

};

#endif
