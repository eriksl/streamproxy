#ifndef _mpegts_pat_h_
#define _mpegts_pat_h_

#include "mpegts_sr.h"
#include <string>
#include <map>

class MpegTSPat : public MpegTSSectionReader
{
	private:

		typedef unsigned int uint;
		typedef struct
		{
			uint	program_high:8;
			uint	program_low:8;
			uint	pmt_pid_high:5;
			uint	reserved:3;
			uint	pmt_pid_low:8;
		} mpeg_pat_entry_t;

		typedef struct
		{
			uint8_t byte[4];
		} mpeg_crc_t;

		MpegTSPat();
		MpegTSPat(const MpegTSPat &);

	public:

		typedef std::map<int, int> pat_t;

		pat_t pat;

		MpegTSPat(int fd) throw();

		bool probe() throw(std::string);
};

#endif
