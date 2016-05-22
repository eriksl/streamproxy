#include "config.h"

#include "url.h"
#include "util.h"
#include "types.h"

#include <string>
using std::string;

#include <boost/algorithm/string.hpp>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <sstream>
#include <iomanip>
using std::ostringstream;
using std::hex;
using std::setfill;
using std::setw;

Url::Url(const string &url_in) throw() : url(url_in)
{
}

string Url::plus_expand(const string &param) const throw()
{
	return(boost::replace_all_copy(param, "+", " "));
}

string Url::percent_expand(const string &param) const throw()
{
	size_t	current = 0;
	size_t	needle;
	string	rv;
	string	hex;

	while(current < param.length())
	{
		needle = param.find('%', current);

		if(needle == string::npos)
		{
			rv += param.substr(current);
			break;
		}

		rv += param.substr(current, needle - current);

		if((needle + 3) > param.length())
		{
			rv += param.substr(current);
			break;
		}

		hex = param.substr(needle + 1, 2);

		if(isxdigit(hex[0]) && isxdigit(hex[1]))
			rv += (char)(strtoul(hex.c_str(), 0, 16) & 0xff);
		else
			rv += string("%") + hex;

		current = needle + 3;
	}

	return(rv);
}

UrlParameterMap Url::split() const throw()
{
	UrlParameterMap	rv;
	size_t			needle;
	string 			request;
	stringvector	params;
	stringvector::const_iterator it;

	if((needle = url.find('?')) == string::npos)
	{
		rv[""] = percent_expand(plus_expand(url));
		return(rv);
	}

	rv[""] = url.substr(0, needle);

	request = url.substr(needle + 1);
	boost::split(params, request, boost::is_any_of("&?"));

	for(it = params.begin(); it != params.end(); it++)
	{
		if(it->length() > 0)
		{
			if((needle = it->find('=')) == string::npos)
				rv[*it] = "";
			else
				if(needle > 0)
					rv[it->substr(0, needle)] = percent_expand(plus_expand(it->substr(needle + 1)));
		}
	}

	return(rv);
}

string Url::encode() const throw()
{
	string::const_iterator it;
	ostringstream conv;

	for(it = url.begin(); it != url.end(); it++)
	{
		if(*it > ' ')
			conv << setw(0) << *it;
		else
			conv << '%' << setw(2) << setfill('0') << hex << int(*it);
	}

	return(conv.str());
}
