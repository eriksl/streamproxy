#ifndef _url_h_
#define _url_h_

#include "config.h"

#include <string>
using std::string;

#include <vector>
#include <map>

class Url
{
	private:

		typedef std::vector<string> stringvector;
		string url;

	public:

		typedef std::map<string, string> urlparam;

		Url(const string &url)			throw();
		urlparam split() 		const	throw();

	private:

		Url();
		Url(const Url &);

		string percent_expand(const string &param) const throw();
};

#endif
