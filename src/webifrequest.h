#ifndef _webifrequest_h_
#define _webifrequest_h_

#include "config.h"

#include "service.h"
#include "types.h"

#include <string>

class WebifRequest
{
	private:

		int				fd;
		const Service	&service;
		std::string		reply;
		PidMap			pids;
		int				demuxer_id;

		WebifRequest();
		WebifRequest(const WebifRequest &);

	public:

		WebifRequest(const Service &service, std::string webauth) throw(std::string);
		~WebifRequest();

		void	poll()							throw(std::string);
		PidMap	get_pids()				const	throw();
		int		get_demuxer_id()		const	throw();
};

#endif
