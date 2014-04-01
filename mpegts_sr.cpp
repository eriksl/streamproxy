#include "mpegts_sr.h"
#include "vlog.h"

#include <unistd.h>
#include <stddef.h>

#include <string>
using std::string;

#include <boost/crc.hpp>

MpegTSSectionReader::MpegTSSectionReader(int fd_in) throw()
{
	fd = fd_in;
}

bool MpegTSSectionReader::probe(int filter_pid, int filter_table) throw(string)
{
	typedef boost::crc_optimal<32, 0x04c11db7, 0xffffffff, 0x0, false, false> boost_mpeg_crc_t;

	boost_mpeg_crc_t	my_crc;
	mpeg_crc_t			mpeg_crc;
	uint32_t			their_crc;

			packet_t		packet;
	const	table_header_t	*table;
	const	table_syntax_t	*syntax;
	const	uint8_t 		*payload_data;

	int		timeout;
	int		section_length = -1;
	int		section_length_remaining = -1;
	int		pid;
	int		cc = -1;
	int		packet_payload_offset;
	int		payload_length;

	raw_table_data.clear();
	table_data.clear();

	//vlog("\nstart probe\n");

	for(timeout = 0; timeout < 2000; timeout++)
	{
		if(read(fd, (void *)&packet, sizeof(packet)) != sizeof(packet))
			throw(string("MpegTSSectionReader::read: read error"));

		if(packet.header.sync_byte != MpegTSSectionReader::sync_byte_value)
			throw(string("MpegTSSectionReader: no sync byte found"));

		pid = (packet.header.pid_high << 8) | (packet.header.pid_low);

		if(pid != filter_pid)
			continue;

		if(packet.header.tei)
			continue;

		if(!packet.header.payload_present)
			continue;

		if((cc != -1) && (cc != packet.header.cc))
		{
			vlog("discontinuity: %d/%d", cc, packet.header.cc);
			goto retry;
		}

		cc = (packet.header.cc + 1) & 0x0f;

		if(packet.header.pusi)
		{
			if(table_data.length() > 0)
			{
				//vlog("start payload upexpected");
				goto retry;
			}
		}
		else
		{
			if(table_data.length() <= 0)
			{
				//vlog("continue payload unexpected");
				goto retry;
			}
		}

		//vlog("correct packet with pid: %x, %s", pid, packet.header.pusi ? "start" : "continuation");

		packet_payload_offset = sizeof(packet.header);

		//vlog("payload offset: %d", packet_payload_offset);

		if(packet.header.af)
			packet_payload_offset += packet.byte[sizeof(packet.header)] + 1;

		//vlog("payload offset after adaptation field: %d", packet_payload_offset);

		if(packet.header.pusi)
			packet_payload_offset += packet.byte[packet_payload_offset] + 1; // psi payload pointer byte

		//vlog("payload offset after section pointer field: %d", packet_payload_offset);

		if(table_data.length() == 0)
		{
			table = (const table_header_t *)&packet.byte[packet_payload_offset];

			raw_table_data.assign((const uint8_t *)table, offsetof(table_header_t, payload));

			//vlog("table id: %d", table->table_id);

			if(table->table_id != filter_table)
			{
				vlog("table %d != %d", table->table_id, filter_table);
				goto retry;
			}

			if(table->private_bit)
			{
				vlog("private != 0: %d", table->private_bit);
				goto retry;
			}

			if(!table->section_syntax)
			{
				vlog("section_syntax == 0: %d", table->section_syntax);
				goto retry;
			}

			if(table->reserved != 0x03)
			{
				vlog("reserved != 0x03: %x", table->reserved);
				goto retry;
			}

			if(table->section_length_unused != 0x00)
			{
				vlog("section length unused != 0x00: %x", table->section_length_unused);
				goto retry;
			}

			section_length = ((table->section_length_high << 8) | (table->section_length_low)) - offsetof(table_syntax_t, data);
			//vlog("section length: %d", section_length);

			if(section_length < 0)
			{
				vlog("section length < 0: %d", section_length);
				goto retry;
			}

			if(section_length_remaining < 0)
				section_length_remaining = section_length;

			syntax = (const table_syntax_t *)&table->payload;

			raw_table_data.append((const uint8_t *)syntax, offsetof(table_syntax_t, data));

			//vlog("tide: 0x%x", (syntax->tide_high << 8) | syntax->tide_low);

			if(syntax->reserved != 0x03)
			{
				vlog("syntax reserved != 0x03: %d", syntax->reserved);
				goto retry;
			}

			//vlog("currnext: %d", syntax->currnext);
			//vlog("version: %d", syntax->version);
			//vlog("ordinal: %d", syntax->ordinal);
			//vlog("last: %d", syntax->last);

			payload_length = sizeof(packet_t) - ((const uint8_t *)&syntax->data - &packet.byte[0]);
			//vlog("payload length: %d", payload_length);

			if(payload_length > section_length_remaining)
				payload_length = section_length_remaining;

			//vlog("payload length after trimming: %d", payload_length);

			raw_table_data.append((const uint8_t *)&syntax->data, payload_length);
			table_data.append((const uint8_t *)&syntax->data, payload_length);
		}
		else
		{
			payload_data = (const uint8_t *)&packet.byte[packet_payload_offset];
			payload_length = sizeof(packet_t) - packet_payload_offset;

			if(payload_length > section_length_remaining)
				payload_length = section_length_remaining;

			raw_table_data.append((const uint8_t *)payload_data, payload_length);
			table_data.append((const uint8_t *)payload_data, payload_length);
		}

		section_length_remaining -= payload_length;

		//vlog("appended table data, %d bytes, length: %d, section: %d, remaining: %d", payload_length, table_data.length(), section_length, section_length_remaining);

		if((section_length > 0) && (section_length_remaining == 0))
			break;

		//vlog("\nnext packet");
		continue;

retry:
		//vlog("\nprobe retry");

		section_length = -1;
		section_length_remaining = -1;
		raw_table_data.clear();
		table_data.clear();
	}

	if(timeout >= 2000)
	{
		//vlog("timeout");
		return(false);
	}

	if(section_length < (int)sizeof(mpeg_crc_t))
	{
		vlog("table too small");
		return(false);
	}

	my_crc.process_bytes(raw_table_data.data(), raw_table_data.length() - sizeof(mpeg_crc_t));

	mpeg_crc	= *(const mpeg_crc_t *)(&raw_table_data.data()[raw_table_data.length() - sizeof(mpeg_crc_t)]);
	their_crc	= (mpeg_crc.byte[0] << 24) | (mpeg_crc.byte[1] << 16) | (mpeg_crc.byte[2] << 8) | mpeg_crc.byte[3];

	if(my_crc.checksum() != their_crc)
	{
		vlog("crc mismatch: my crc: %x, their crc: %x\n", my_crc.checksum(), their_crc);
		return(false);
	}

	return(true);
}
