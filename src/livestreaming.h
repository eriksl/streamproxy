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

		LiveStreaming()					throw();
		LiveStreaming(LiveStreaming &)	throw();

	public:

		LiveStreaming(const Service &service, int socketfd, std::string webauth,
				const StreamingParameters &streaming_parameters,
				const ConfigMap &config_map) throw(trap);
};

#endif
