#ifndef _livetranscoding_broadcom_h_
#define _livetranscoding_broadcom_h_

#include "config.h"
#include "trap.h"

#include "service.h"
#include "configmap.h"
#include "types.h"
#include "stbtraits.h"

#include <string>

class LiveTranscodingBroadcom
{
	private:

		enum
		{
			broadcom_magic_buffer_size = 256 * 188,
		};

		typedef enum
		{
			state_initial,
			state_starting,
			state_running
		} encoder_state_t;

		LiveTranscodingBroadcom();
		LiveTranscodingBroadcom(LiveTranscodingBroadcom &);

	public:

		LiveTranscodingBroadcom(const Service &service, int socketfd,
				const stb_traits_t &stb_traits,
				const StreamingParameters &streaming_parameters,
				const ConfigMap &config_map);
};

#endif
