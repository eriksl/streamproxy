#ifndef _livetranscoding_h_
#define _livetranscoding_h_

#include "config.h"
#include "trap.h"

#include "service.h"
#include "configmap.h"

#include <string>

class LiveTranscoding
{
	private:

		enum
		{
			vuplus_magic_buffer_size = 256 * 188,
		};

		typedef enum
		{
			state_initial,
			state_starting,
			state_running
		} encoder_state_t;

		LiveTranscoding()					throw();
		LiveTranscoding(LiveTranscoding &)	throw();

	public:

		LiveTranscoding(const Service &service, int socketfd,
				std::string webauth, std::string frame_size,
				std::string bitrate, std::string profile,
				std::string level, std::string bframes,
				const ConfigMap &config_map)
			throw(trap);
};

#endif
