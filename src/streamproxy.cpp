#include "acceptsocket.h"
#include "clientsocket.h"
#include "vlog.h"
#include "enigma_settings.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>

#include <sstream>
using std::ostringstream;

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <map>
using std::map;

#include <boost/program_options.hpp>
namespace bpo = boost::program_options;

typedef struct
{
	ClientSocket::default_streaming_action	default_action;
	AcceptSocket							*accept_socket;
} listen_socket_t;

typedef vector<string> multiparameter_t;
typedef map<string, listen_socket_t> listen_action_t;

static void sigchld(int) // prevent Z)ombie processes
{
	vlog("streamproxy: wait for child");
	waitpid(0, 0, 0);
	signal(SIGCHLD, sigchld);
	vlog("streamproxy: wait for child done");
}

int main(int argc, char **argv)
{
	bpo::options_description	desc("Use single or multiple pairs of port_number:default_action either with --listen or positional");
	ostringstream				convert;

	try
	{
		multiparameter_t						listen_parameters;
		multiparameter_t::const_iterator		it;
		listen_action_t							listen_action;
		listen_action_t::iterator				it2;
		size_t									ix;
		ssize_t									rv;
		string									port;
		string									action_str;
		ClientSocket::default_streaming_action	action;
		struct pollfd							*pfd;
		int										new_socket;
		static const char						*action_name[2] = { "stream", "transcode" };
		EnigmaSettings							settings;
		bpo::positional_options_description		po_desc;
		bpo::variables_map						vm;
		bool									use_web_authentication;
		time_t									start;

		if(settings.exists("config.OpenWebif.auth") && settings.as_string("config.OpenWebif.auth") == "true")
			use_web_authentication = true;
		else
			use_web_authentication = false;

		vlog("web auth: %d", use_web_authentication);

		po_desc.add("listen", -1);

		desc.add_options()
			("foreground,f",	bpo::bool_switch(&foreground)->implicit_value(true),	"run in foreground (don't become a daemon)")
			("listen,l",		bpo::value<multiparameter_t>(&listen_parameters),		"listen to tcp port with default action");

		bpo::store(bpo::command_line_parser(argc, argv).options(desc).positional(po_desc).run(), vm);
		bpo::notify(vm);

		//fprintf(stderr, "foreground: %d\n", foreground);

		for(it = listen_parameters.begin(); it != listen_parameters.end(); it++)
		{
			ix = it->find(':');

			if((ix == string::npos) || (ix == 0))
				throw(string("positional parameter should consist of <port>:<default action>"));

			port		= it->substr(0, ix);
			action_str	= it->substr(ix + 1);

			if(action_str == "stream")
				action = ClientSocket::action_stream;
			else
				if(action_str == "transcode")
					action = ClientSocket::action_transcode;
				else
					throw(string("default action should be either stream or transcode"));

			listen_action[port].default_action = action;
		}

		if(listen_action.size() == 0)
			throw(string("no listen_port:default_action parameters given"));

		pfd = new struct pollfd[listen_action.size()];

		for(it2 = listen_action.begin(), ix = 0; it2 != listen_action.end(); it2++, ix++)
		{
			it2->second.accept_socket = new AcceptSocket(it2->first);
			pfd[ix].fd		= it2->second.accept_socket->get_fd();
			pfd[ix].events	= POLLIN;
			fprintf(stderr, "> %s -> %s,%d\n", it2->first.c_str(), action_name[it2->second.default_action], it2->second.accept_socket->get_fd());
		}

		signal(SIGCHLD, sigchld);

		for(;;)
		{
			errno = 0;

			if((rv = poll(pfd, listen_action.size(), -1)) < 0)
			{
				if(errno == EINTR)
					errno = 0;
				else
					throw(string("poll error"));
			}

			for(ix = 0; ix < listen_action.size(); ix++)
			{
				if(pfd[ix].revents & (POLLERR | POLLNVAL | POLLHUP))
					throw(string("poll error on fd"));

				if(!(pfd[ix].revents & POLLIN))
					continue;

				for(it2 = listen_action.begin(); it2 != listen_action.end(); it2++)
					if(it2->second.accept_socket->get_fd() == pfd[ix].fd)
						break;

				if(it2 == listen_action.end())
					throw(string("poll success on non-existent fd"));

				new_socket = it2->second.accept_socket->accept();

				vlog("streamproxy: accept new connection on port %s, default action: %s, fd %d",
						it2->first.c_str(), action_name[it2->second.default_action], new_socket);

				if(fork()) // parent
					close(new_socket);
				else
				{
					(void)ClientSocket(new_socket, use_web_authentication, it2->second.default_action);
					_exit(0);
				}

				start = time(0);
				while((time(0) - start) < 2) // primitive connection rate limiting
				{
					vlog("* sleep *");
					sleep(2);
				}
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
