#ifndef _webrequest_h_
#define _webrequest_h_

#include "config.h"
#include "types.h"
#include "configmap.h"
#include "stbtraits.h"

#include <string>

class WebRequest
{
	private:

		const ConfigMap			&config_map;
		const HeaderMap			&headers;
		const CookieMap			&cookies;
		const UrlParameterMap	&parameters;
		const stb_traits_t		&stb_traits;

		std::string page_info(std::string &mimetype)				const;
		std::string page_test_cookie(std::string &mimetype)			const;

	public:

		WebRequest(const ConfigMap &config_map, const HeaderMap &headers,
				const CookieMap &cookies,
				const UrlParameterMap &parameters,
				const stb_traits_t &stb_traits);
		std::string get(std::string &mimetype)						const;
};

#endif
