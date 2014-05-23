#ifndef _time_offset_h_
#define _time_offset_h_

#include "config.h"
#include "trap.h"

#include <string>

class TimeOffset
{
	private:

		int seconds;

		TimeOffset();
		TimeOffset(const TimeOffset &);

	public:

		TimeOffset(std::string timestring) throw(trap);
		int as_seconds() const throw();
};

#endif
