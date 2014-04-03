#ifndef _clientsocket_h_
#define _clientsocket_h_

#include <string>
#include <vector>
#include <map>

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

	public:

		typedef enum
		{
			action_stream,
			action_transcode,
		} default_streaming_action;

				ClientSocket(int fd, default_streaming_action default_action)	throw();
			    ~ClientSocket()													throw();
		void	run()															throw();

};

#endif
