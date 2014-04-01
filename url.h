#ifndef _url_h_
#define _url_h_

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <map>
using std::map;

class Url
{
	private:

		typedef vector<string> stringvector;
		string url;

	public:

		typedef map<string, string> urlparam;

		Url(const string &url)			throw();
		urlparam split() 		const	throw();

	private:

		Url();
		Url(const Url &);

		string percent_expand(const string &param) const throw();
};

#endif
