#include "config.h"
#include "trap.h"

#include "mpegts.h"
#include "util.h"

#include <unistd.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <boost/crc.hpp>

#include <vector>
#include <string>
using std::string;

#include <boost/algorithm/string.hpp>

MpegTS::MpegTS(int fd_in, bool request_time_seek_in)
	:	private_fd(false),
		fd(fd_in),
		request_time_seek(request_time_seek_in)
{
	init();
}

MpegTS::MpegTS(string filename, string audiolang_in, bool request_time_seek_in)
	:	private_fd(true),
		audiolang(audiolang_in),
		request_time_seek(request_time_seek_in)
{
	if((fd = open(filename.c_str(), O_RDONLY, 0)) < 0)
		throw(trap("MpegTS::MpegTS: cannot open file"));

	init();
}

MpegTS::~MpegTS()
{
	if(private_fd)
		close(fd);
}

void MpegTS::init()
{
	mpegts_pat_t::const_iterator it;
	struct stat filestat;

	if(fstat(fd, &filestat))
		throw(trap("MpegTS::init: cannot stat"));

	Util::vlog("MpegTS::init: file length: %lld Mb", filestat.st_size  >> 20);

	eof_offset = stream_length = filestat.st_size;
	eof_offset = (eof_offset / sizeof(ts_packet_t)) * sizeof(ts_packet_t);

	if(!read_pat())
		throw(trap("MpegTS::init: invalid transport stream (no suitable pat)"));

	for(it = pat.begin(); it != pat.end(); it++)
		if(read_pmt(it->second))
			break;

	if(it == pat.end())
		throw(trap("MpegTS::init: invalid transport stream (no suitable pmt)"));

	pmt_pid = it->second;
	is_time_seekable = false;

	if(request_time_seek)
	{
		Util::vlog("MpegTS: start find pcr");

		if(lseek(fd, 0, SEEK_SET) == (off_t)-1)
			Util::vlog("MpegTS::init seek to sof fails");
		else
		{
			Util::vlog("MpegTS: start find first pcr");

			first_pcr_ms = find_pcr_ms(direction_forward);

			if(lseek(fd, eof_offset, SEEK_SET) == (off_t)-1)
				Util::vlog("MpegTS::init: seek to eof fails");
			else
			{
				Util::vlog("MpegTS: start find last pcr");
				last_pcr_ms	= find_pcr_ms(direction_backward);

				if(last_pcr_ms < first_pcr_ms)
					Util::vlog("MpegTS::init: pcr wraparound, cannot seek this stream, first pcr: %d, last pcr: %d", first_pcr_ms / 1000, last_pcr_ms / 1000);
				else
					if((first_pcr_ms >= 0) && (last_pcr_ms >= 0))
						is_time_seekable = true;
			}
		}

		Util::vlog("MpegTS: find pcr done");
	}

	if(!is_time_seekable)
	{
		first_pcr_ms	= -1;
		last_pcr_ms		= -1;
	}

	if(lseek(fd, 0, SEEK_SET) == (off_t)-1)
		Util::vlog("MpegTS::init: seek to sof fails");

	//Util::vlog("first_pcr_ms = %d", first_pcr_ms);
	//Util::vlog("last_pcr_ms = %d", last_pcr_ms);
	//Util::vlog("eof_offset is at %lld", eof_offset);
}

bool MpegTS::read_table(int filter_pid, int filter_table)
{
	typedef boost::crc_optimal<32, 0x04c11db7, 0xffffffff, 0x0, false, false> boost_mpeg_crc_t;

	boost_mpeg_crc_t	my_crc;
	mpeg_crc_t			mpeg_crc;
	uint32_t			their_crc;

			ts_packet_t				packet;
	const	section_table_header_t	*table;
	const	section_table_syntax_t	*syntax;
	const	uint8_t 				*payload_data;

	int		timeout;
	int		section_length = -1;
	int		section_length_remaining = -1;
	int		pid;
	int		cc = -1;
	int		packet_payload_offset;
	int		payload_length;

	raw_table_data.clear();
	table_data.clear();

	for(timeout = 0; timeout < 2000; timeout++)
	{
		if(read(fd, (void *)&packet, sizeof(packet)) != sizeof(packet))
			throw(trap("MpegTS::read_table: read error"));

		if(packet.header.sync_byte != MpegTS::sync_byte_value)
			throw(trap("MpegTS::read_table: no sync byte found"));

		pid = (packet.header.pid_high << 8) | (packet.header.pid_low);

		if(pid != filter_pid)
			continue;

		if(packet.header.tei)
			continue;

		if(!packet.header.payload_present)
			continue;

		if((cc != -1) && (cc != packet.header.cc))
		{
			//Util::vlog("MpegTS::read_table discontinuity: %d/%d", cc, packet.header.cc);
			goto retry;
		}

		cc = (packet.header.cc + 1) & 0x0f;

		if(packet.header.pusi)
		{
			if(table_data.length() > 0)
			{
				//Util::vlog("MpegTS::read_table: start payload upexpected");
				goto retry;
			}
		}
		else
		{
			if(table_data.length() <= 0)
			{
				//Util::vlog("MpegTS::read_table: continue payload unexpected");
				goto retry;
			}
		}

		//Util::vlog("MpegTS::read_table: correct packet with pid: 0x%x, %s", pid, packet.header.pusi ? "start" : "continuation");

		packet_payload_offset = offsetof(ts_packet_t, header.payload);

		//Util::vlog("MpegTS::read_table: payload offset: %d", packet_payload_offset);

		if(packet.header.af)
			packet_payload_offset = offsetof(ts_packet_t, header.afield) + packet_payload_offset;

		//Util::vlog("MpegTS::read_table: payload offset after adaptation field: %d", packet_payload_offset);

		if(packet.header.pusi)
			packet_payload_offset += packet.byte[packet_payload_offset] + 1; // psi payload pointer byte

		//Util::vlog("MpegTS::read_table: payload offset after section pointer field: %d", packet_payload_offset);

		if(table_data.length() == 0)
		{
			table = (const section_table_header_t *)&packet.byte[packet_payload_offset];

			raw_table_data.assign((const uint8_t *)table, offsetof(section_table_header_t, payload));

			//Util::vlog("MpegTS::read_table: table id: %d", table->table_id);

			if(table->table_id != filter_table)
			{
				Util::vlog("MpegTS::read_table: table %d != %d", table->table_id, filter_table);
				goto retry;
			}

			if(table->private_bit)
			{
				Util::vlog("MpegTS::read_table: private != 0: %d", table->private_bit);
				goto retry;
			}

			if(!table->section_syntax)
			{
				Util::vlog("MpegTS::read_table: section_syntax == 0: %d", table->section_syntax);
				goto retry;
			}

			if(table->reserved != 0x03)
			{
				Util::vlog("MpegTS::read_table: reserved != 0x03: 0x%x", table->reserved);
				goto retry;
			}

			if(table->section_length_unused != 0x00)
			{
				Util::vlog("MpegTS::read_table: section length unused != 0x00: 0x%x", table->section_length_unused);
				goto retry;
			}

			section_length = ((table->section_length_high << 8) | (table->section_length_low)) - offsetof(section_table_syntax_t, data);
			//Util::vlog("MpegTS::read_table: section length: %d", section_length);

			if(section_length < 0)
			{
				Util::vlog("MpegTS::read_table: section length < 0: %d", section_length);
				goto retry;
			}

			if(section_length_remaining < 0)
				section_length_remaining = section_length;

			syntax = (const section_table_syntax_t *)&table->payload;

			raw_table_data.append((const uint8_t *)syntax, offsetof(section_table_syntax_t, data));

			//Util::vlog("MpegTS::read_table: tide: 0x%x", (syntax->tide_high << 8) | syntax->tide_low);

			if(syntax->reserved != 0x03)
			{
				Util::vlog("MpegTS::read_table: syntax reserved != 0x03: %d", syntax->reserved);
				goto retry;
			}

			//Util::vlog("MpegTS::read_table: currnext: %d", syntax->currnext);
			//Util::vlog("MpegTS::read_table: version: %d", syntax->version);
			//Util::vlog("MpegTS::read_table: ordinal: %d", syntax->ordinal);
			//Util::vlog("MpegTS::read_table: last: %d", syntax->last);

			payload_length = sizeof(ts_packet_t) - ((const uint8_t *)&syntax->data - &packet.byte[0]);
			//Util::vlog("MpegTS::read_table: payload length: %d", payload_length);

			if(payload_length > section_length_remaining)
				payload_length = section_length_remaining;

			//Util::vlog("MpegTS::read_table: payload length after trimming: %d", payload_length);

			raw_table_data.append((const uint8_t *)&syntax->data, payload_length);
			table_data.append((const uint8_t *)&syntax->data, payload_length);
		}
		else
		{
			payload_data = (const uint8_t *)&packet.byte[packet_payload_offset];
			payload_length = sizeof(ts_packet_t) - packet_payload_offset;

			if(payload_length > section_length_remaining)
				payload_length = section_length_remaining;

			raw_table_data.append((const uint8_t *)payload_data, payload_length);
			table_data.append((const uint8_t *)payload_data, payload_length);
		}

		section_length_remaining -= payload_length;

		if((section_length > 0) && (section_length_remaining == 0))
			break;

		continue;

retry:
		section_length = -1;
		section_length_remaining = -1;
		raw_table_data.clear();
		table_data.clear();
	}

	if(table_data.length() == 0)
	{
		//Util::vlog("MpegTS::read_table: timeout");
		return(false);
	}

	if(section_length < (int)sizeof(mpeg_crc_t))
	{
		Util::vlog("MpegTS::read_table: table too small");
		return(false);
	}

	my_crc.process_bytes(raw_table_data.data(), raw_table_data.length() - sizeof(mpeg_crc_t));

	mpeg_crc	= *(const mpeg_crc_t *)(&raw_table_data.data()[raw_table_data.length() - sizeof(mpeg_crc_t)]);
	their_crc	= (mpeg_crc.byte[0] << 24) | (mpeg_crc.byte[1] << 16) | (mpeg_crc.byte[2] << 8) | mpeg_crc.byte[3];

	if(my_crc.checksum() != their_crc)
	{
		Util::vlog("MpegTS::read_table: crc mismatch: my crc: 0x%x, their crc: 0x%x", my_crc.checksum(), their_crc);
		return(false);
	}

	return(true);
}

bool MpegTS::read_pat()
{
	int		attempt;
	int		current, entries, program, pid;
	const	pat_entry_t *entry;

	for(attempt = 0; attempt < 16; attempt++)
	{
		if(!read_table(0, 0))
			continue;

		entries = (table_data.length() - sizeof(mpeg_crc_t)) / sizeof(*entry);
		entry = (const pat_entry_t *)table_data.data();

		for(current = 0; current < entries; current++)
		{
			program = (entry[current].program_high << 8) | (entry[current].program_low);
			pid = (entry[current].pmt_pid_high << 8) | (entry[current].pmt_pid_low);
			//Util::vlog("MpegTS::read_pat > program: %d -> pid 0x%x", program, pid);

			if(entry[current].reserved != 0x07)
			{
				Util::vlog("MpegTS::read_pat > reserved != 0x07: 0x%x", entry[current].reserved);
				goto next_pat_entry;
			}

			pat[program] = pid;
		}

		return(true);

next_pat_entry:
		(void)0;
	}

	return(false);
}


static	int getselectedlang_prio(string stream_language, string audiolang)				// audiolang has struct  aaa bbb ccc/ddd eee fff/ggg hhh iii/jjj
{	std::vector <std::string> 	autolangprios_vector;									// aaa bbb ccc: Prio 1 languages
	std::vector <std::string>	autolang_vector;										// ddd eee fff: Prio 2 languages
	int							prio;													// ggg hhh iii: Prio 3 languages
																						// jjj: Prio 4 languages
																						// if one found in streamlang then returns prio 1..4
    boost::split(autolangprios_vector, audiolang, boost::is_any_of("/"));
	prio = 1;
	for(auto& autolang: autolangprios_vector)
	{
		if (!autolang.empty())
		{
			//Util::vlog("MpegTS::getselectedlang: selected languages prio %d: %s", prio, autolang.c_str());
			boost::split(autolang_vector, autolang, boost::is_any_of(" \t"));
			for(auto &singlelang: autolang_vector)
				if (!singlelang.empty())
				{
                    if(boost::starts_with(stream_language, singlelang))					// if selected is prefix
						return prio;
				}
		}
		prio++;
	}

	return 256;
}


bool MpegTS::read_pmt(int filter_pid)
{
	int		attempt, programinfo_length, esinfo_length;
	int		es_pid, es_data_length, es_data_skip, es_data_offset;
	int		ds_data_skip, ds_data_offset;
	bool	private_stream_is_ac3;
	bool	private_stream_is_audio;
	int		audioac3_pid, audiolang_pid, audiolangac3_pid;
	int		audiolang_prio, audiolang_prio_act;
	string	audiolang_choose, audiolangac3_choose, audiolang_fallback;
	string	stream_language;


	const	uint8_t			*es_data;
	const	pmt_header_t	*pmt_header;
	const	pmt_es_entry_t	*es_entry;
	const	pmt_ds_entry_t	*ds_entry;
	const	pmt_ds_a_t		*ds_a;

	pcr_pid = video_pid = audio_pid = audioac3_pid = audiolang_pid = audiolangac3_pid = -1;
	audiolang_prio_act = 255;															// lower prio than 1,2,3,4 for autolanguage.audio1-4
	audiolang_choose = audiolangac3_choose = "";

	for(attempt = 0; attempt < 16; attempt++)
	{
		if(!read_table(filter_pid, table_pmt))
			break;

		pmt_header = (const pmt_header_t *)table_data.data();
		pcr_pid = (pmt_header->pcrpid_high << 8) | pmt_header->pcrpid_low;
		programinfo_length = (pmt_header->programinfo_length_high << 8) |
			pmt_header->programinfo_length_low;

		if(pmt_header->reserved_1 != 0x07)
		{
			Util::vlog("MpegTS::read_pmt > reserved_1: 0x%x", pmt_header->reserved_1);
			continue;
		}

		//Util::vlog("MpegTS::read_pmt: > pcr_pid: 0x%x", pcr_pid);
		//Util::vlog("MpegTS::read_pmt: > program info length: %d", programinfo_length);

		if(pmt_header->unused != 0x00)
		{
			Util::vlog("MpegTS::read_pmt: > unused: 0x%x", pmt_header->unused);
			continue;
		}

		if(pmt_header->reserved_2 != 0x0f)
		{
			Util::vlog("MpegTS::read_pmt: > reserved_2: 0x%x", pmt_header->reserved_2);
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

			if(es_entry->reserved_1 != 0x07)
			{
				Util::vlog("MpegTS::read_pmt: reserved 1: 0x%x", es_entry->reserved_1);
				goto next_descriptor_entry;
			}

			//Util::vlog("MpegTS::read_pmt: >> pid: 0x%x", es_pid);

			if(es_entry->reserved_2 != 0x0f)
			{
				Util::vlog("MpegTS::read_pmt: reserved 2: 0x%x", es_entry->reserved_2);
				goto next_descriptor_entry;
			}

			if(es_entry->unused != 0x00)
			{
				Util::vlog("MpegTS::read_pmt: unused: 0x%x", es_entry->unused);
				goto next_descriptor_entry;
			}

			//Util::vlog("MpegTS::read_pmt: esinfo_length: %d", esinfo_length);

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
					private_stream_is_audio = true;
					stream_language = "";

					ds_data_skip = es_data_offset + offsetof(pmt_es_entry_t, descriptors); 

					for(ds_data_offset = 0; (ds_data_offset + 2) < esinfo_length; )
					{
						ds_entry = (const pmt_ds_entry_t *)&es_data[ds_data_skip + ds_data_offset];

						//Util::vlog("MpegTS::read_pmt: >>> offset: %d", ds_data_offset);
						//Util::vlog("MpegTS::read_pmt: >>> descriptor id: 0x%x", ds_entry->id);
						//Util::vlog("MpegTS::read_pmt: >>> length: %d", ds_entry->length);

						switch(ds_entry->id)
						{
							case(pmt_desc_language):
						{
								ds_a = (const pmt_ds_a_t *)&ds_entry->data;
								//Util::vlog("MpegTS::read_pmt: >>>> lang: %c%c%c [%d]", ds_a->lang[0],
										// ds_a->lang[1], ds_a->lang[2], ds_a->code);

								stream_language.assign((const char *)&ds_a->lang, offsetof(pmt_ds_a_t, code));

								break;
							}

							case(pmt_desc_ac3):
							{
								private_stream_is_ac3 = true;
								break;
							}
							case(pmt_desc_teletext):
							case(pmt_desc_subtitle):
							{
								private_stream_is_audio = false;
								break;
							}
						}

						ds_data_offset += ds_entry->length + offsetof(pmt_ds_entry_t, data);
					}

					if (!private_stream_is_audio)
						Util::vlog("MpegTS::found teletext or subtitle with audiolang [%s]: pid: %d", stream_language.c_str(), es_pid);
					else
					{
						if (private_stream_is_ac3)
							Util::vlog("MpegTS::found audiolang [%s]: pid: %d, [AC3]", stream_language.c_str(), es_pid);
						else
							Util::vlog("MpegTS::found audiolang [%s]: pid: %d", stream_language.c_str(), es_pid);
					}

					if (private_stream_is_audio && !(boost::iequals(stream_language, "nar")) )
					{
						if((audiolang_prio = getselectedlang_prio(stream_language, audiolang)) > 0)	// if audiolang found with prio 1..4
						{
							if (audiolang_prio < audiolang_prio_act)								// better prio found
								audiolangac3_pid = audiolang_pid = -1;
							if (audiolang_prio <= audiolang_prio_act)								// better prio found
							{
								audiolang_prio_act = audiolang_prio;
								if (private_stream_is_ac3)
								{
									audiolangac3_pid = es_pid;										// first AC3 with language
									audiolangac3_choose = stream_language;
								}
								else
								{
									audiolang_pid = es_pid;											// first with language
									audiolang_choose = stream_language;
								}
							}
						}

						if(private_stream_is_ac3)
						{
							if (audioac3_pid == -1)
							{
								audio_pid = audioac3_pid = es_pid;							// first AC3 Audio
								audiolang_fallback = stream_language;
							}
							else
								if (audio_pid == -1)
								{
									audio_pid = es_pid;										// First Audio
									audiolang_fallback = stream_language;
								}
						}
					}
				}
			}

next_descriptor_entry:
			es_data_offset += es_data_skip + esinfo_length;
		}


		if (audiolangac3_pid != -1)
		{
			audiolang_pid = audiolangac3_pid;
			audiolang_choose = audiolangac3_choose;
		}

		if (audiolang_pid != -1)
		{
			audio_pid = audiolang_pid;              // language has preference
			Util::vlog("MpegTS::=> choose audiolang [%s] with prio %d, pid: %d", audiolang_choose.c_str(), audiolang_prio_act, audio_pid);
		}
		else
		{
			if (audio_pid != -1)
				Util::vlog("MpegTS::=> no audiolang [%s] found, use audiolang [%s]: pid: %d", audiolang.c_str(), audiolang_fallback.c_str(), audio_pid);
		}

		return(true);
	}

	return(false);
}

int MpegTS::get_fd() const
{
	return(fd);
}

void MpegTS::parse_pts_ms(int pts_ms, int &h, int &m, int &s, int &ms)
{
	h		 = pts_ms / (60 * 60 * 1000);
	pts_ms	-=     h  * (60 * 60 * 1000);
	m		 = pts_ms / (60 * 1000);
	pts_ms	-=     m  * (60 * 1000);
	s		 = pts_ms / (1000);
	pts_ms	-=     s  * (1000);
	ms		 =     pts_ms;
}

int MpegTS::find_pcr_ms(seek_direction_t direction) const
{
	ts_packet_t				packet;
	ts_adaptation_field_t	*afield;
	int						attempt, pcr_ms = -1, pid;
	int						ms, h, m, s;
	off_t					offset;

	for(attempt = 0; (pcr_ms < 0) && (attempt < find_pcr_max_probe); attempt++)
	{
		if(direction == direction_backward)
		{
			offset = (off_t)sizeof(ts_packet_t) * 2 * -1;

			if(lseek(fd, offset, SEEK_CUR) == (off_t)-1)
			{
				Util::vlog("MpegTS::find_pcr_ms: lseek failed");
				return(-1);
			}
		}

		if(read(fd, &packet, sizeof(packet)) != sizeof(packet))
		{
			Util::vlog("MpegTS::find_pcr_ms: read error");
			return(-1);
		}

		if(packet.header.sync_byte != sync_byte_value)
		{
			Util::vlog("MpegTS::find_pcr_ms: no sync byte");
			return(-1);
		}

		pid = (packet.header.pid_high << 8) | packet.header.pid_low;

		//Util::vlog("MpegTS::find_pcr_ms: pid: %d", pid);

		if(pid != pcr_pid)
			continue;

		if(!packet.header.af)
		{
			//Util::vlog("MpegTS::find_pcr_ms: no adaptation field");
			continue;
		}

		afield = (ts_adaptation_field_t *)&packet.header.afield;

		if(!afield->contains_pcr)
		{
			//Util::vlog("MpegTS::find_pcr_ms: attempt: %d, adaptation field does not have pcr field", attempt);
			continue;
		}

		// read 32 bits of the total 42 bits of clock precision, which is
		// enough for seeking, one tick = 1/45th second
		
		pcr_ms = uint32_t(afield->pcr_0 << 24) | uint32_t(afield->pcr_1 << 16) |
				uint32_t(afield->pcr_2 <<  8) | uint32_t(afield->pcr_3 <<  0);
		pcr_ms /= 90;
		pcr_ms <<= 1;
	}

	if(attempt >= find_pcr_max_probe)
	{
		Util::vlog("MpegTS::find_pcr_ms: no pcr found");
		return(-1);
	}

	parse_pts_ms(pcr_ms, h, m, s, ms);

	//Util::vlog("PCR found after %d packets", attempt);
	//Util::vlog("PCR = %d ms (%02d:%02d:%02d:%03d)", pcr_ms, h, m, s, ms);

	return(pcr_ms);
}

off_t MpegTS::seek(int whence, off_t offset) const
{
	ts_packet_t	packet;
	off_t		actual_offset;
	off_t		new_offset;

	offset = ((offset / (off_t)sizeof(ts_packet_t)) * (off_t)sizeof(ts_packet_t));

	if(lseek(fd, offset, whence) < 0)
		throw(trap("MpegTS::seek: lseek (1)"));

	if(read(fd, &packet, sizeof(packet)) != sizeof(packet))
		throw(trap("MpegTS::seek: read error"));

	if(packet.header.sync_byte != sync_byte_value)
		throw(trap("MpegTS::seek: no sync byte"));

	new_offset = (off_t)0 - (off_t)sizeof(packet);

	if((actual_offset = lseek(fd, new_offset, SEEK_CUR)) < 0)
		throw(trap("MpegTS::seek: lseek (2)"));

	return(actual_offset);
}

off_t MpegTS::seek_absolute(off_t offset) const
{
	return(seek(SEEK_SET, offset));
}

off_t MpegTS::seek_relative(off_t roffset, off_t limit) const
{
	off_t offset;

	if((roffset < 0) || (roffset > limit))
		throw(trap("MpegTS::seek_relative: value out of range"));

	offset = (eof_offset / limit) * roffset;

	return(seek(SEEK_SET, offset));
}

off_t MpegTS::seek_time(int pts_ms) const
{
	int		h, m, s, ms;
	int		attempt;
	off_t	lower_bound_offset	= 0;
	int		lower_bound_pts_ms	= first_pcr_ms;
	off_t	upper_bound_offset	= eof_offset;
	int		upper_bound_pts_ms	= last_pcr_ms;
	off_t	disect_offset;
	int		disect_pts_ms;
	off_t	current_offset;

	if(!is_time_seekable)
		throw(trap("MpegTS::seek: stream is not seekable"));

	if(pts_ms < first_pcr_ms)
	{
		Util::vlog("MpegTS::seek: seek pts beyond start of file");
		return(-1);
	}

	if(pts_ms > last_pcr_ms)
	{
		Util::vlog("MpegTS::seek: pts beyond end of file");
		return(-1);
	}

	for(attempt = 0; attempt < seek_max_attempts; attempt++)
	{
		disect_offset = (lower_bound_offset + upper_bound_offset) / 2;

		parse_pts_ms(pts_ms, h, m, s, ms);
		//Util::vlog("MpegTS::seek: seek for [%02d:%02d:%02d.%03d] between ", h, m, s, ms);
		parse_pts_ms(lower_bound_pts_ms, h, m, s, ms);
		//Util::vlog("MpegTS::seek:  [%02d:%02d:%02d.%03d", h, m, s, ms);
		parse_pts_ms(upper_bound_pts_ms, h, m, s, ms);
		//Util::vlog("MpegTS::seek:  -%02d:%02d:%02d.%03d], ", h, m, s, ms);
		//Util::vlog("MpegTS::seek:  offset = %lld [%lld-%lld], ", disect_offset, lower_bound_offset, upper_bound_offset);

		if((current_offset = seek(SEEK_SET, disect_offset)) == 1)
		{
			Util::vlog("MpegTS::seek: seek fails");
			return(-1);
		}

		//Util::vlog("MpegTS::seek: current offset = %lld (%lld%%)", current_offset, (current_offset * 100) / eof_offset);

		if((disect_pts_ms = find_pcr_ms(direction_forward)) < 0)
		{
			Util::vlog("MpegTS::seek: eof");
			return(-1);
		}

		parse_pts_ms(disect_pts_ms, h, m, s, ms);
		//Util::vlog("MpegTS::seek: disect=[%02d:%02d:%02d.%03d]", h, m, s, ms);

		if(disect_pts_ms < 0)
		{
			Util::vlog("MpegTS::seek failed to find pts");
			return(-1);
		}

		if(((disect_pts_ms > pts_ms) && ((disect_pts_ms - pts_ms) < 8000)) || 
			((pts_ms >= disect_pts_ms) && ((pts_ms - disect_pts_ms) < 8000)))
		{
			//Util::vlog("MpegTS::seek: found");
			return(current_offset);
		}

		if(disect_pts_ms < pts_ms) // seek higher
		{
			lower_bound_offset	= disect_offset;
			lower_bound_pts_ms	= disect_pts_ms;

			//Util::vlog("MpegTS::seek: not found: change lower bound");
		}
		else
		{
			upper_bound_offset	= disect_offset;
			upper_bound_pts_ms	= disect_pts_ms;

			//Util::vlog("MpegTS::seek: not found: change upper bound");
		}
	}

	return(-1);
}
