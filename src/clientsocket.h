#ifndef _clientsocket_h_
#define _clientsocket_h_

#include "config.h"

#include "types.h"
#include "configmap.h"

#include <stdint.h>
#include <string>
#include <vector>
#include <map>

class ClientSocket
{
	private:

		typedef std::vector<std::string> stringvector;

		int					fd;
		std::string			request;
		std::string			url;
		HeaderMap			headers;
		CookieMap			cookies;
		UrlParameterMap		urlparams;
		const ConfigMap&	config_map;

		ClientSocket();
		ClientSocket(const ClientSocket &);

		static const std::string	base64_chars;
		static bool 				is_base64(uint8_t c);
		static std::string			base64_decode(const std::string &in) throw();
		static bool					validate_user(std::string user, std::string password,
									std::string require_group) throw();

	public:

		typedef enum
		{
			action_stream,
			action_transcode,
		} default_streaming_action;

		ClientSocket(int fd,
				default_streaming_action default_action,
				const ConfigMap &config_map) throw();
		~ClientSocket()	throw();
};

#endif
