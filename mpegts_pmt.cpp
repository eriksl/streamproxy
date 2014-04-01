#include "mpegts_pmt.h"
#include "vlog.h"

#include "stddef.h"

#include <boost/algorithm/string.hpp>

#include <string>
using std::string;


MpegTSPmt::MpegTSPmt(int fd_in) throw() :
	MpegTSSectionReader(fd_in)
{
}

bool MpegTSPmt::probe(int filter_pid) throw(string)
{
	int		attempt, programinfo_length, esinfo_length;
	int		es_pid, es_data_length, es_data_skip, es_data_offset;
	int		ds_data_skip, ds_data_offset;
	bool	private_stream_is_ac3;
	string	stream_language;

	const	uint8_t			*es_data;
	const	pmt_header_t	*pmt_header;
	const	pmt_es_entry_t	*es_entry;
	const	pmt_ds_entry_t	*ds_entry;
	const	pmt_ds_a_t		*ds_a;

	pcr_pid = video_pid = audio_pid = -1;
	
	for(attempt = 0; attempt < 16; attempt++)
	{
		if(!MpegTSSectionReader::probe(filter_pid, table_pmt))
			continue;

		pmt_header = (const pmt_header_t *)table_data.data();
		pcr_pid = (pmt_header->pcrpid_high << 8) | pmt_header->pcrpid_low;
		programinfo_length = (pmt_header->programinfo_length_high << 8) |
			pmt_header->programinfo_length_low;

		if(pmt_header->reserved_1 != 0x07)
		{
			vlog("\n> reserved_1: %x", pmt_header->reserved_1);
			continue;
		}

		//vlog("> pcr_pid: %x", pcr_pid);
		//vlog("> program info length: %d", programinfo_length);

		if(pmt_header->unused != 0x00)
		{
			vlog("> unused: %x", pmt_header->unused);
			continue;
		}

		if(pmt_header->reserved_2 != 0x0f)
		{
			vlog("> reserved_2: %x", pmt_header->reserved_2);
			continue;
		}

		es_data			= &(table_data.data()[offsetof(pmt_header_t, data) + programinfo_length]);
		es_data_length	= table_data.length() - offsetof(pmt_header_t, data);
		es_data_skip	= offsetof(pmt_es_entry_t, descriptors);

		for(es_data_offset = 0; (es_data_offset + es_data_skip + (int)sizeof(uint32_t)) < es_data_length; )
		{
			es_entry		= (const pmt_es_entry_t *)&es_data[es_data_offset];
			esinfo_length	= (es_entry->es_length_high << 8) | es_entry->es_length_low;
			es_pid			= (es_entry->es_pid_high << 8) | es_entry->es_pid_low;

			//vlog("\n>> stream type: %x", es_entry->stream_type);

			if(es_entry->reserved_1 != 0x07)
			{
				vlog(">> reserved 1: %x", es_entry->reserved_1);
				goto next;
			}

			//vlog(">> pid: %x", es_pid);

			if(es_entry->reserved_2 != 0x0f)
			{
				vlog(">> reserved 2: %x", es_entry->reserved_2);
				goto next;
			}

			if(es_entry->unused != 0x00)
			{
				vlog(">> unused: %x", es_entry->unused);
				goto next;
			}

			//vlog(">> esinfo_length: %d", esinfo_length);

			switch(es_entry->stream_type)
			{
				case(mpeg_streamtype_video_mpeg1):
				case(mpeg_streamtype_video_mpeg2):
				case(mpeg_streamtype_video_h264):
				{
					if(video_pid < 0)
						video_pid = es_pid;
					break;
				}

				case(mpeg_streamtype_audio_mpeg1):
				case(mpeg_streamtype_audio_mpeg2):
				case(mpeg_streamtype_private_pes):	// ac3
				{
					private_stream_is_ac3 = false;

					ds_data_skip = es_data_offset + offsetof(pmt_es_entry_t, descriptors); 

					for(ds_data_offset = 0; (ds_data_offset + 2) < esinfo_length; )
					{
						ds_entry = (const pmt_ds_entry_t *)&es_data[ds_data_skip + ds_data_offset];

						//vlog("\n>>> offset: %d", ds_data_offset);
						//vlog(">>> descriptor id: %x", ds_entry->id);
						//vlog(">>> length: %d", ds_entry->length);

						switch(ds_entry->id)
						{
							case(desc_language):
						{
								ds_a = (const pmt_ds_a_t *)&ds_entry->data;
								//vlog(">>>> lang: %c%c%c [%d]", ds_a->lang[0],
										// ds_a->lang[1], ds_a->lang[2], ds_a->code);

								stream_language.assign((const char *)&ds_a->lang, offsetof(pmt_ds_a_t, code));

								break;
							}

							case(desc_ac3):
							{
								private_stream_is_ac3 = true;
								break;
							}
						}

						ds_data_offset += ds_entry->length + offsetof(pmt_ds_entry_t, data);
					}

					if(!boost::iequals(stream_language, "nar"))
					{
						if(private_stream_is_ac3 || (audio_pid < 0)) // ac3 stream has preference
							audio_pid = es_pid;
					}
				}
			}

next:		es_data_offset += es_data_skip + esinfo_length;
		}

		return(true);
	}

	return(false);
}
