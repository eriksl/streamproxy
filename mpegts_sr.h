#ifndef _mpegts_sr_h_
#define _mpegts_sr_h_

#include <stdint.h>
#include <string>

class MpegTSSectionReader
{
	private:

		typedef std::basic_string<uint8_t> u8string;
		typedef unsigned int uint;

		enum
		{
			sync_byte_value = 0x47,
		};

		typedef union
		{
			struct
			{
				uint sync_byte:8;

				uint pid_high:5;
				uint tp:1;
				uint pusi:1;
				uint tei:1;

				uint pid_low:8;

				uint cc:4;
				uint payload_present:1;
				uint af:1;
				uint sc:2;
			} header;
			uint8_t	byte[188];
		} packet_t;

		typedef struct
		{
			uint	table_id:8;
			uint	section_length_high:2;
			uint	section_length_unused:2;
			uint	reserved:2;
			uint	private_bit:1;
			uint	section_syntax:1;
			uint	section_length_low:8;
			uint8_t	payload[0];
		} table_header_t;

		typedef struct
		{
			uint	tide_high:8;
			uint	tide_low:8;
			uint	currnext:1;
			uint	version:5;
			uint	reserved:2;
			uint	ordinal:8;
			uint	last:8;
			uint8_t	data[0];
		} table_syntax_t;

		typedef struct
		{
			uint8_t byte[4];
		} mpeg_crc_t;

		MpegTSSectionReader();
		MpegTSSectionReader(const MpegTSSectionReader &);

		int			fd;
		u8string	raw_table_data;

	protected:

		u8string	table_data;

	public:

		MpegTSSectionReader(int fd) throw();

		void clear();
		bool probe(int pid, int table) throw(std::string);
};

#endif
