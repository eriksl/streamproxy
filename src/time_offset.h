#ifndef _time_offset_h_
#define _time_offset_h_

#include "config.h"

#include <string>

class TimeOffset
{
	private:

		int seconds;

		TimeOffset();
		TimeOffset(const TimeOffset &);

	public:

		TimeOffset(std::string timestring) throw(std::string);
		int as_seconds() const throw();
};

#endif
