#ifndef _clientsocket_h_
#define _clientsocket_h_

#include <string>
#include <vector>
#include <map>

#include <stdint.h>

class ClientSocket
{
	private:

		typedef std::vector<std::string>			stringvector;
		typedef std::map<std::string, std::string>	stringmap;

		int				fd;
		std::string		request;
		std::string		url;
		stringmap		headers;

		ClientSocket();
		ClientSocket(const ClientSocket &);

		static const std::string	base64_chars;
		static bool 				is_base64(uint8_t c);
		static std::string			base64_decode(const std::string &in) throw();
		static bool					validate_user(std::string user, std::string password) throw();

	public:

		typedef enum
		{
			action_stream,
			action_transcode,
		} default_streaming_action;

				ClientSocket(int fd, bool use_web_authentication,
						default_streaming_action default_action) throw();
				~ClientSocket()	throw();
};

#endif
