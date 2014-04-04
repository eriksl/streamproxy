#include "acceptsocket.h"
#include "clientsocket.h"
#include "vlog.h"
#include "mpegts_pat.h"
#include "mpegts_pmt.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>

#include <sstream>
using std::ostringstream;

#include <string>
using std::string;

#include <boost/program_options.hpp>

int main(int argc, char **argv)
{
	namespace bpo = boost::program_options;
	bpo::options_description	desc("Options");
	bpo::variables_map			vm;
	ostringstream				convert;

	try
	{
		int		new_fd;
		string	port;
		string	default_action_arg;
		ClientSocket::default_streaming_action default_action;

		desc.add_options()
			("port",		bpo::value<string>(&port)->default_value("8002"),					"bind to tcp port")
			("default",		bpo::value<string>(&default_action_arg)->default_value("stream"),	"default action when only serviceref is given, can be either stream or transcode");

		bpo::store(bpo::parse_command_line(argc, argv, desc), vm);
		bpo::notify(vm);

		vlog("streamproxy: port: %s", port.c_str());
		vlog("streamproxy: default action: %s", default_action_arg.c_str());

		if(default_action_arg == "stream")
			default_action = ClientSocket::action_stream;
		else
		{
			if(default_action_arg == "transcode")
				default_action = ClientSocket::action_transcode;
			else
			{
				vlog("streamproxy: default action should be either \"stream\" or \"action\"");
				exit(1);
			}
		}

		AcceptSocket accept_socket(port);

		for(;;)
		{
			new_fd = accept_socket.accept();

			vlog("streamproxy: accept new connection: %d", new_fd);

			if(fork()) // parent
				close(new_fd);
			else
			{
				(void)ClientSocket(new_fd, default_action);
				_exit(0);
			}
		}
	}
	catch(const string &e)
	{
		fprintf(stderr, "streamproxy: %s\n", e.c_str());
		exit(1);
	}
	catch(bpo::error &e)
	{
		fprintf(stderr, "streamproxy: %s\n", e.what());
		convert.str("");
		convert << desc;
		fprintf(stderr, "streamproxy: %s\n", convert.str().c_str());
		exit(1);
	}
	catch(...)
	{
		vlog("streamproxy: default exception");
		exit(1);
	}

	return(0);
}
