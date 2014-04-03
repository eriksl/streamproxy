#ifndef _clientsocket_h_
#define _clientsocket_h_

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <map>
using std::map;

class ClientSocket
{
	private:

		typedef vector<string>		stringvector;
		typedef map<string, string>	stringmap;

		int			fd;
		string		request;
		string		url;
		stringmap	headers;

	public:

		typedef enum
		{
			action_stream,
			action_transcode,
		} default_streaming_action;

				ClientSocket(int fd, default_streaming_action default_action)	throw();
			    ~ClientSocket()													throw();
		void	run()															throw();

	private:

		ClientSocket();
		ClientSocket(const ClientSocket &);
};

#endif
