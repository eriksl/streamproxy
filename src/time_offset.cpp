#include "time_offset.h"
#include "vlog.h"

#include <string>
using::std::string;

#include <boost/range/algorithm/count.hpp>

#include <stdlib.h>

TimeOffset::TimeOffset(string timestring) throw(string)
{
	int c;
	size_t ix;
	string part;

	if(timestring.length() < 1)
		throw(string("TimeOffset: invalid timestring, empty"));

	c = boost::count(timestring, ':');
	seconds = 0;

	switch(c)
	{
		case(2):
		{
			vlog("TimeOffset: h timestring: ", timestring.c_str());
			ix = timestring.find(':');
			part = timestring.substr(0, ix);
			vlog("TimeOffset: h part: %s", part.c_str());
			seconds += strtoul(part.c_str(), 0, 10) * 60 * 60;
			timestring.erase(0, ix + 1);

			// fall through
		}

		case(1):
		{
			vlog("TimeOffset: m timestring: ", timestring.c_str());
			ix = timestring.find(':');
			part = timestring.substr(0, ix);
			vlog("TimeOffset: m part: %s", part.c_str());
			seconds += strtoul(part.c_str(), 0, 10) * 60;
			timestring.erase(0, ix + 1);

			// fall through
		}

		case(0):
		{
			vlog("TimeOffset: s timestring: ", timestring.c_str());
			ix = timestring.find(':');
			part = timestring.substr(0, ix);
			vlog("TimeOffset: s part: %s", part.c_str());
			seconds += strtoul(part.c_str(), 0, 10);

			break;
		}

		default:
		{
			throw(string("TimeOffset: invalid timestring, to many colons"));
		}
	}

	vlog("TimeOffset: seconds: %d", seconds);
}

int TimeOffset::as_seconds() const throw()
{
	return(seconds);
}
