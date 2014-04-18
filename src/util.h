#ifndef _util_h_
#define _util_h_

#include "config.h"

#include <string>

class Util
{
	public:

		static	bool		foreground;

		static void			vlog(const char * format, ...)		throw();
		static std::string	int_to_string(int)					throw();
		static std::string	hex_to_string(int, int width = 4)	throw();
		static int			string_to_int(std::string)			throw();
		static std::string	float_to_string(double, int)		throw();
		static double		string_to_float(std::string)		throw();
};
#endif
