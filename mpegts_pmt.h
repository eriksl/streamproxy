#ifndef _mpegts_pmt_h_
#define _mpegts_pmt_h_

#include "mpegts_sr.h"

#include <string>
#include <map>

class MpegTSPmt : public MpegTSSectionReader
{
	private:

		enum
		{
			table_pmt		= 0x02,
		};

		enum
		{
			desc_language	= 0x0a,
			desc_ac3		= 0x6a,
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

		typedef unsigned int uint;
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

		MpegTSPmt();
		MpegTSPmt(const MpegTSPmt &);

	public:

		MpegTSPmt(int fd) throw();

		int	pcr_pid;
		int	video_pid;
		int	audio_pid;

		bool probe(int filter_pid) throw(std::string);
};

#endif
