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
		rv[""] = url;
		return(rv);
	}

	rv[""] = url.substr(0, needle);

	request = url.substr(needle + 1);
	boost::split(params, request, boost::is_any_of("&"));

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
