#include "service.h"
#include "vlog.h"

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <boost/algorithm/string.hpp>

#include <string.h>

Service::Service(string service_in) throw(string)
{
	typedef boost::algorithm::split_iterator<string::iterator> string_split_iterator;

	string_split_iterator	find_it;
	intvector::iterator		int_it;
	string::const_iterator	string_it;
	int						value;
	string					field;

	service = service_in;
	valid	= false;

	if(service.length() == 0)
	{
		vlog("Service: invalid service (empty)");
		service_field.clear();
		return;
	}

	if(service[service.length() - 1] == ':') // OpenWebIF leaves this in
		service.erase(service.length() - 1);

	vlog("Service: create service: %s", service.c_str());

	for(find_it = make_split_iterator(service, first_finder(":", boost::algorithm::is_equal()));
		find_it != string_split_iterator(); find_it++)
	{
		value = 0;

		field = boost::copy_range<string>(*find_it);

		//vlog("Service: field: \"%s\"", field.c_str());

		if(field.length() == 0)
		{
			vlog("Service: invalid service (empty field)");
			service_field.clear();
			return;
		}

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
						vlog("Service: invalid service");
						service_field.clear();
						return;
					}
		}

		service_field.push_back(value);
	}

	//for(int_it = service_field.begin(); int_it != service_field.end(); int_it++)
		//vlog("Service: service field: %x", *int_it);

	if(service_field.size() != 10)
	{
		vlog("Service: invalid service (invalid # of fields");
		service_field.clear();
		return;
	}

	valid = true;
}

bool Service::is_valid() const throw()
{
	return(valid);
}

string Service::service_string() const throw(string)
{
	if(!valid)
		throw(string("Service: service invalid"));

	return(service);
}

Service::intvector Service::service_vector() const throw(string)
{
	if(!valid)
		throw(string("Service: service invalid"));

	return(service_field);
}
