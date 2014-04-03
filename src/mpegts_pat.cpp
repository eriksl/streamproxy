#include "mpegts_pat.h"
#include "vlog.h"

#include <string>
using std::string;

MpegTSPat::MpegTSPat(int fd_in) throw() :
	MpegTSSectionReader(fd_in)
{
}

bool MpegTSPat::probe() throw(string)
{
	int		attempt;
	int		current, entries, program, pid;
	const	mpeg_pat_entry_t *entry;
	
	for(attempt = 0; attempt < 16; attempt++)
	{
		if(!MpegTSSectionReader::probe(0, 0))
			continue;

		entries = (table_data.length() - sizeof(mpeg_crc_t)) / sizeof(*entry);
		entry = (const mpeg_pat_entry_t *)table_data.data();

		for(current = 0; current < entries; current++)
		{
			program = (entry[current].program_high << 8) | (entry[current].program_low);
			pid = (entry[current].pmt_pid_high << 8) | (entry[current].pmt_pid_low);
			//vlog("MpegTSPAt: > program: %d -> pid %x", program, pid);

			if(entry[current].reserved != 0x07)
			{
				vlog("MpegTSPAt: > reserved != 0x07: 0x%x", entry[current].reserved);
				goto next;
			}

			pat[program] = pid;
		}

		return(true);

next:	(void)0;
	}

	return(false);
}
