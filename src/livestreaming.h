#ifndef _livestreaming_h_
#define _livestreaming_h_

#include "config.h"
#include "trap.h"

#include "service.h"
#include "configmap.h"
#include "types.h"

#include <string>

class LiveStreaming
{
	private:

		LiveStreaming();
		LiveStreaming(LiveStreaming &);

	public:

		LiveStreaming(const Service &service, int socketfd, std::string webauth,
				const StreamingParameters &streaming_parameters,
				const ConfigMap &config_map);
};

#endif
