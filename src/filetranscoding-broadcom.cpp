#include "config.h"
#include "trap.h"

#include "filetranscoding-broadcom.h"
#include "util.h"
#include "queue.h"
#include "encoder-broadcom.h"
#include "types.h"
#include "mpegts.h"
#include "configmap.h"
#include "time_offset.h"
#include "stbtraits.h"

#include <string>
using std::string;

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

FileTranscodingBroadcom::FileTranscodingBroadcom(string file, int socket_fd, string,
		const stb_traits_t &stb_traits, const StreamingParameters &streaming_parameters,
		const ConfigMap &config_map)
{
	PidMap::const_iterator it;
	PidMap			pids, encoder_pids;
	int				encoder_fd, timeout;
	size_t			max_fill_socket = 0;
	ssize_t			bytes_read;
	struct pollfd	pfd[2];
	off_t			file_offset = 0;
	int				time_offset_s = 0;
	off_t			byte_offset = 0;
	off_t			http_range = 0;
	int				pct_offset = 0;
	string			audio_lang;
	bool			partial = false;
	encoder_state_t	encoder_state;
	Queue			socket_queue(1024 * 1024);
	const char *	http_ok			=	"HTTP/1.1 200 OK\r\n";
	const char *	http_partial	=	"HTTP/1.1 206 Partial Content\r\n";
	const char *	http_headers	=	"Connection: Close\r\n"
										"Content-Type: video/mpeg\r\n"
										"Server: Streamproxy\r\n"
										"Accept-Ranges: bytes\r\n";
	string			http_reply;

	if(streaming_parameters.count("startfrom"))
		time_offset_s = TimeOffset(streaming_parameters.at("startfrom")).as_seconds();

	if(streaming_parameters.count("http_range"))
		http_range = Util::string_to_uint(streaming_parameters.at("http_range"));

	if(streaming_parameters.count("byte_offset"))
		byte_offset = Util::string_to_uint(streaming_parameters.at("byte_offset"));

	if(streaming_parameters.count("pct_offset"))
		pct_offset = Util::string_to_uint(streaming_parameters.at("pct_offset"));

	audio_lang = config_map.at("audiolang").string_value;
	Util::vlog("FileTrancoding: audiolang: %s", audio_lang.c_str());
	MpegTS stream(file, audio_lang, time_offset_s > 0);

	Util::vlog("FileTrancoding: streaming file: %s", file.c_str());
	Util::vlog("FileTrancoding: byte_offset: %llu / %llu (%llu %%)", byte_offset, stream.stream_length,
			(byte_offset * 100) / stream.stream_length);
	Util::vlog("FileTrancoding: pct_offset: %d", pct_offset);
	Util::vlog("FileTrancoding: time_offset: %d", time_offset_s);

	if(http_range > 0)
	{
		Util::vlog("FileTranscodingBroadcom: performing http byte range seek");

		stream.seek_absolute(http_range);

		// Lie to the client because it will never get the exact position it requests due to ts packet alignment.
		// E.g. wget will refuse range operation this way. It won't hurt for viewing anyway.

		file_offset = http_range;
		partial = true;
	}
	else
	{
		if(byte_offset > 0)
		{
			Util::vlog("FileTranscodingBroadcom: performing byte_offset seek");

			stream.seek_absolute(byte_offset);

			// Lie to the client because it will never get the exact position it requests due to ts packet alignment.
			// E.g. wget will refuse range operation this way. It won't hurt for viewing anyway.

			file_offset = byte_offset;
		}
		else
		{
			if(pct_offset > 0)
			{
				Util::vlog("FileTranscodingBroadcom: performing pct_offset seek");
				file_offset = stream.seek_relative(pct_offset, 100);
			}
			else
			{
				if(stream.is_time_seekable && (time_offset_s > 0))
				{
					Util::vlog("FileTranscodingBroadcom: performing startfrom seek");
					file_offset = stream.seek_time((time_offset_s * 1000) + stream.first_pcr_ms);
				}
			}
		}
	}

	Util::vlog("FileTranscodingBroadcom: file_offset: %llu", file_offset);

	if(partial > 0)
		http_reply = http_partial;
	else
		http_reply = http_ok;

	http_reply += http_headers;
	http_reply += "Content-Length: " + Util::uint_to_string(stream.stream_length) + "\r\n";

	if(partial)
		http_reply += string("Content-Range: bytes ") +
			Util::uint_to_string(file_offset) + "-" +
			Util::uint_to_string(stream.stream_length - 1) + "/" +
			Util::uint_to_string(stream.stream_length) + "\r\n";

	http_reply += "\r\n";

	socket_queue.append(http_reply.length(), http_reply.c_str());

	for(it = pids.begin(); it != pids.end(); it++)
		Util::vlog("FileTranscodingBroadcom: pid[%s] = 0x%x", it->first.c_str(), it->second);

	encoder_buffer = new char[broadcom_magic_buffer_size];

	pids["pmt"]		= stream.pmt_pid;
	pids["video"]	= stream.video_pid;
	pids["audio"]	= stream.audio_pid;

	EncoderBroadcom encoder(pids, stb_traits, streaming_parameters);
	encoder_pids = encoder.getpids();

	for(it = encoder_pids.begin(); it != encoder_pids.end(); it++)
		Util::vlog("FileTranscodingBroadcom: encoder pid[%s] = 0x%x", it->first.c_str(), it->second);

	if((encoder_fd = encoder.getfd()) < 0)
		throw(trap("FileTranscodingBroadcom: transcoding: encoder: fd not open"));

	encoder_state = state_initial;

	for(;;)
	{
		if(socket_queue.usage() > max_fill_socket)
			max_fill_socket = socket_queue.usage();

		switch(encoder_state)
		{
			case(state_initial):
			{
				if(encoder.start_init())
					encoder_state = state_starting;
				break;
			}

			case(state_starting):
			{
				if(encoder.start_finish())
					encoder_state = state_running;
				break;
			}

			case(state_running):
			{
				break;
			}
		}

		pfd[0].fd		= encoder_fd;
		pfd[0].events	= POLLIN | POLLOUT;

		pfd[1].fd		= socket_fd;
		pfd[1].events	= POLLRDHUP;

		if(socket_queue.length() > 0)
			pfd[1].events |= POLLOUT;

		timeout = (encoder_state == state_starting) ? 1000 : -1;

		if(poll(pfd, 2, timeout) <= 0)
			throw(trap("FileTranscodingBroadcom: poll error"));

		if(pfd[0].revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			Util::vlog("FileTranscodingBroadcom: encoder error");
			break;
		}

		if(pfd[1].revents & (POLLRDHUP | POLLHUP))
		{
			Util::vlog("FileTranscodingBroadcom: client hung up");
			break;
		}

		if(pfd[1].revents & (POLLERR | POLLNVAL))
		{
			Util::vlog("FileTranscodingBroadcom: socket error");
			break;
		}

		if(pfd[0].revents & POLLOUT)
		{
			if((bytes_read = read(stream.get_fd(), encoder_buffer, broadcom_magic_buffer_size)) != broadcom_magic_buffer_size)
			{
				Util::vlog("FileTranscodingBroadcom: eof");
				break;
			}

			if((bytes_read = write(encoder_fd, encoder_buffer, bytes_read)) != broadcom_magic_buffer_size)
			{
				Util::vlog("FileTranscodingBroadcom: encoder error");
				break;
			}
		}

		if(pfd[0].revents & POLLIN)
		{
			if(!socket_queue.read(encoder_fd, broadcom_magic_buffer_size))
			{
				Util::vlog("FileTranscodingBroadcom: read encoder error");
				break;
			}
		}

		if(pfd[1].revents & POLLOUT)
		{
			if(!socket_queue.write(socket_fd))
			{
				Util::vlog("FileTranscodingBroadcom: write socket error");
				break;
			}
		}
	}

	Util::vlog("FileTranscodingBroadcom: streaming ends, socket max queue fill: %d%%", max_fill_socket);
}

FileTranscodingBroadcom::~FileTranscodingBroadcom()
{
	delete [] encoder_buffer;
	Util::vlog("FileTranscodingBroadcom: cleanup up");
}
