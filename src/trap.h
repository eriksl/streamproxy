#ifndef _trap_h_
#define _trap_h_

#include <string>
#include <exception>

class trap : public std::exception
{
	public:

		std::string	message;

		trap(std::string msg);
		virtual	const char *what() const noexcept;
		virtual	~trap();
};

class http_trap : public trap
{
	public:

		int			http_error;
		std::string	http_header;

		http_trap(std::string msg,
				int http_error, std::string http_header);

		virtual	const char *what() const noexcept;
		virtual	~http_trap();

};

#endif
