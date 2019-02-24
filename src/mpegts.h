#ifndef _mpegts_h_
#define _mpegts_h_

#include "config.h"
#include "trap.h"

#include <map>
#include <string>

#include <stdint.h>
#include <sys/types.h>

class MpegTS
{
	private:

		typedef std::basic_string<uint8_t> u8string;
		typedef unsigned int bf;

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
			find_pcr_max_probe	= 1000000,
			seek_max_attempts	= 32,
		};

		enum
		{
			pmt_desc_language	= 0x0a,
			pmt_desc_ac3		= 0x6a,
			pmt_desc_teletext	= 0x56,
			pmt_desc_subtitle	= 0x59,
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
				uint8_t	sync_byte;
				bf		pid_high:5;
				bf		tp:1;
				bf		pusi:1;
				bf		tei:1;
				uint8_t	pid_low;
				bf		cc:4;
				bf		payload_present:1;
				bf		af:1;
				bf		sc:2;
				uint8_t	payload[0];
				uint8_t	afield_length;
				uint8_t	afield[0];
			} header;
			uint8_t	byte[188];
		} ts_packet_t;

		typedef struct
		{
			bf		field_ext:1;
			bf		private_data:1;
			bf		splice_point:1;
			bf		contains_opcr:1;
			bf		contains_pcr:1;
			bf		priority:1;
			bf		gop_start:1;
			bf		discont:1;
			uint8_t	pcr_0; // 32 bits of the total 42 bits
			uint8_t	pcr_1; // of clock precision, which is
			uint8_t	pcr_2; // enough for seeking
			uint8_t	pcr_3; // tick = 1/45th second
		} ts_adaptation_field_t;

		typedef struct
		{
			uint8_t	table_id;
			bf		section_length_high:2;
			bf		section_length_unused:2;
			bf		reserved:2;
			bf		private_bit:1;
			bf		section_syntax:1;
			uint8_t	section_length_low;
			uint8_t	payload[0];
		} section_table_header_t;

		typedef struct
		{
			uint8_t	tide_high;
			uint8_t	tide_low;
			bf		currnext:1;
			bf		version:5;
			bf		reserved:2;
			uint8_t	ordinal;
			uint8_t	last;
			uint8_t	data[0];
		} section_table_syntax_t;

		typedef struct
		{
			uint8_t	program_high;
			uint8_t	program_low;
			bf		pmt_pid_high:5;
			bf		reserved:3;
			uint8_t	pmt_pid_low;
		} pat_entry_t;

		typedef struct
		{
			bf		pcrpid_high:5;
			bf		reserved_1:3;
			uint8_t	pcrpid_low;
			uint	programinfo_length_high:2;
			bf		unused:2;
			bf		reserved_2:4;
			uint8_t	programinfo_length_low;
			uint8_t	data[0];
		} pmt_header_t;

		typedef struct
		{
			uint8_t	stream_type;
			bf		es_pid_high:5;
			bf		reserved_1:3;
			uint8_t	es_pid_low;
			bf		es_length_high:2;
			bf		unused:2;
			bf		reserved_2:4;
			uint8_t	es_length_low;
			uint8_t	descriptors[0];
		} pmt_es_entry_t;

		typedef struct
		{
			uint8_t	id;
			uint8_t	length;
			uint8_t	data[0];
		} pmt_ds_entry_t;

		typedef struct
		{
			uint8_t	lang[3];
			uint8_t	code;
		} pmt_ds_a_t;

		typedef enum
		{
			direction_forward,
			direction_backward,
		} seek_direction_t;

		typedef std::map<int, int> mpegts_pat_t;

		MpegTS();
		MpegTS(const MpegTS &);

		bool			private_fd;
		int				fd;
		mpegts_pat_t	pat;
		u8string		raw_table_data;
		u8string		table_data;

		static void	parse_pts_ms(int pts_ms, int &h, int &m, int &s, int &ms);

		void	init();
		bool	read_table(int filter_pid, int filter_table);
		bool	read_pat();
		bool	read_pmt(int filter_pid);
		int		find_pcr_ms(seek_direction_t direction)	const;
		off_t	seek(int whence, off_t offset)			const;

	public:

		int		pmt_pid;
		int		pcr_pid;
		int		video_pid;
		int		audio_pid;
		std::string	audiolang;

		bool	request_time_seek;
		bool	is_time_seekable;

		int		first_pcr_ms;
		int		last_pcr_ms;
		off_t	eof_offset;
		off_t	stream_length;

		MpegTS(int fd, bool request_time_seek);
		MpegTS(std::string file, std::string audiolang, bool request_time_seek);
		~MpegTS();

		int		get_fd()									const;
		off_t	seek_absolute(off_t offset)					const;
		off_t	seek_relative(off_t offset, off_t limit)	const;
		off_t	seek_time(int pts_ms)						const;
};
#endif
