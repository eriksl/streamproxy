#ifndef _webifrequest_h_
#define _webifrequest_h_

#include "config.h"
#include "trap.h"

#include "service.h"
#include "types.h"
#include "configmap.h"

#include <string>

class WebifRequest
{
	private:

		int					fd;
		const Service		&service;
		const ConfigMap		&config_map;
		std::string			reply;
		PidMap				pids;
		int					demuxer_id;

		WebifRequest();
		WebifRequest(const WebifRequest &);

	public:

		WebifRequest(const Service &service, const ConfigMap &config_map);
		~WebifRequest();

		void	poll();
		PidMap	get_pids()				const;
		int		get_demuxer_id()		const;
};

#endif
