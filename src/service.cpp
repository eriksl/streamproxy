#include "config.h"
#include "trap.h"

#include "service.h"
#include "util.h"

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <boost/algorithm/string.hpp>

#include <string.h>

Service::Service(string service_in)
{
	typedef boost::algorithm::split_iterator<string::iterator> string_split_iterator;

	string_split_iterator	find_it;
	intvector::iterator		int_it;
	string::const_iterator	string_it;
	int						value;
	string					field;

	valid	= false;

	if(service_in.length() == 0)
	{
		Util::vlog("Service: invalid service (empty)");
		service_field.clear();
		return;
	}

	Util::vlog("Service: create service: %s", service_in.c_str());

	for(find_it = make_split_iterator(service_in, first_finder(":", boost::algorithm::is_equal()));
		(find_it != string_split_iterator()) && (service_field.size() < 10);
		find_it++)
	{
		value = 0;

		field = boost::copy_range<string>(*find_it);

		//Util::vlog("Service: field: \"%s\"", field.c_str());

		if(field.length() == 0)
		{
			Util::vlog("Service: invalid service (empty field)");
			service_field.clear();
			return;
		}

		if(service_field.size() > 0)
			service += ":";

		service += field;

		for(string_it = field.begin(); string_it != field.end(); string_it++)
		{
			value <<= 4;

			if((*string_it >= '0') && (*string_it <= '9'))
				value += char(*string_it) - '0';
			else
				if((*string_it >= 'a') && (*string_it <= 'f'))
					value += char(*string_it) - 'a' + 10;
				else
					if((*string_it >= 'A') && (*string_it <= 'F'))
						value += char(*string_it) - 'A' + 10;
					else
					{
						Util::vlog("Service: invalid service");
						service_field.clear();
						return;
					}
		}

		service_field.push_back(value);
	}

	valid = true;
}

bool Service::is_valid() const
{
	return(valid);
}

string Service::service_string() const
{
	if(!valid)
		throw(trap("Service: service invalid"));

	return(service);
}

Service::intvector Service::service_vector() const
{
	if(!valid)
		throw(trap("Service: service invalid"));

	return(service_field);
}
