#ifndef _filetranscoding_h_
#define _filetranscoding_h_

#include "config.h"
#include "trap.h"

#include <string>

class FileTranscoding
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

		FileTranscoding()					throw();
		FileTranscoding(FileTranscoding &)	throw();

		char *encoder_buffer;

	public:

		FileTranscoding(std::string file, int socketfd,
				int pct_offset, int time_offset_s,
				std::string frame_size,
				std::string bitrate, std::string profile,
				std::string level, std::string bframes)
											throw(trap);
		~FileTranscoding()					throw();
};

#endif
