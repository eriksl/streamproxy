#ifndef _mpegts_h_
#define _mpegts_h_

#include <map>
#include <string>

#include <stdint.h>

class MpegTS
{
	private:

		typedef std::basic_string<uint8_t> u8string;
		typedef unsigned int uint;

		enum
		{
			sync_byte_value = 0x47,
		};

		enum
		{
			table_pmt = 0x02,
		};

		enum
		{
			pmt_desc_language	= 0x0a,
			pmt_desc_ac3		= 0x6a,
		};

		enum
		{
			mpeg_streamtype_video_mpeg1	= 0x01,
			mpeg_streamtype_video_mpeg2	= 0x02,
			mpeg_streamtype_video_h264	= 0x1b,
			mpeg_streamtype_audio_mpeg1	= 0x03,
			mpeg_streamtype_audio_mpeg2	= 0x04,
			mpeg_streamtype_private_pes	= 0x06,
		};

		typedef struct
		{
			uint8_t byte[4];
		} mpeg_crc_t;

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
		} ts_packet_t;

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
		} section_table_header_t;

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
		} section_table_syntax_t;

		typedef struct
		{
			uint	program_high:8;
			uint	program_low:8;
			uint	pmt_pid_high:5;
			uint	reserved:3;
			uint	pmt_pid_low:8;
		} pat_entry_t;

		typedef struct
		{
			uint	pcrpid_high:5;
			uint	reserved_1:3;
			uint	pcrpid_low:8;
			uint	programinfo_length_high:2;
			uint	unused:2;
			uint	reserved_2:4;
			uint	programinfo_length_low:8;
			uint8_t	data[0];
		} pmt_header_t;

		typedef struct
		{
			uint	stream_type:8;

			uint	es_pid_high:5;
			uint	reserved_1:3;

			uint	es_pid_low:8;

			uint	es_length_high:2;
			uint	unused:2;
			uint	reserved_2:4;

			uint	es_length_low:8;

			uint8_t	descriptors[0];
		} pmt_es_entry_t;

		typedef struct
		{
			uint	id:8;
			uint	length:8;
			uint8_t	data[0];
		} pmt_ds_entry_t;

		typedef struct
		{
			uint8_t	lang[3];
			uint8_t	code;
		} pmt_ds_a_t;

		typedef std::map<int, int> mpegts_pat_t;

		MpegTS();
		MpegTS(const MpegTS &);

		bool			private_fd;
		int				fd;
		mpegts_pat_t	pat;
		u8string		raw_table_data;
		u8string		table_data;

		void init()											throw(std::string);
		bool read_table(int filter_pid, int filter_table)	throw(std::string);
		bool read_pat()										throw(std::string);
		bool read_pmt(int filter_pid)						throw(std::string);

	public:

		int	pmt_pid;
		int	pcr_pid;
		int	video_pid;
		int	audio_pid;

		MpegTS(int fd)				throw();
		MpegTS(std::string file)	throw(std::string);
		~MpegTS()					throw();

		int get_fd()		const	throw();
};
#endif
