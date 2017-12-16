#ifndef _url_h_
#define _url_h_

#include "config.h"
#include "types.h"

#include <string>
#include <vector>
#include <map>

class Url
{
	private:

		typedef std::vector<std::string> stringvector;
		std::string url;

	public:

		Url(const std::string &url);

		UrlParameterMap		split() 	const;
		std::string			encode()	const;

	private:

		Url();
		Url(const Url &);

		std::string plus_expand(const std::string &param)		const;
		std::string percent_expand(const std::string &param)	const;
};

#endif
