#ifndef _streamingsocket_h_
#define _streamingsocket_h_

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <map>
using std::map;

class StreamingSocket
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

				StreamingSocket(int fd, default_streaming_action default_action)	throw();
			    ~StreamingSocket()													throw();
		void	run()																throw();

	private:

		StreamingSocket(); // delete default constructors
		StreamingSocket(const StreamingSocket &);
};

#endif
