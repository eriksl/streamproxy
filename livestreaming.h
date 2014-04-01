#ifndef _livestreaming_h_
#define _livestreaming_h_

#include "streamingsocket.h"
#include "service.h"
#include "demuxer.h"
#include "encoder.h"
#include "queue.h"

#include <string>

class LiveStreaming
{
	private:

		enum
		{
			vuplus_magic_buffer_size = 256 * 188,
		};

		LiveStreaming()					throw();
		LiveStreaming(LiveStreaming &)	throw();

		Demuxer *demuxer;
		Encoder *encoder;
		Queue	*encoder_queue;
		Queue	*socket_queue;

	public:

		typedef enum
		{
			mode_stream,
			mode_transcode,
		} streaming_mode;

		LiveStreaming(const Service &service, int socketfd, streaming_mode mode) throw(std::string);
		~LiveStreaming() throw();
};

#endif
