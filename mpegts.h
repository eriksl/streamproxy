#ifndef _mpegts_h_
#define _mpegts_h_

#include <stdint.h>

#include <string>

#include "pidmap.h"

class MpegTS
{
	private:

		enum
		{
			syncbyte_value = 0x47,
			tspacket_size = 188,
		};
		
		MpegTS();
		MpegTS(const MpegTS &);

		int		fd;
		int		pat_pid;
		int		pmt_pid;
		int		pcr_pid;
		int		video_pid;
		PidMap	audio_pids;

	public:

		MpegTS(int fd)		throw();
		//~MpegTS()			throw();

		void probe()		throw(std::string);
};

#endif
