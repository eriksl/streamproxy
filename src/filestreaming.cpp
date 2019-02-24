#include "config.h"
#include "trap.h"

#include "filestreaming.h"
#include "util.h"
#include "queue.h"
#include "mpegts.h"
#include "time_offset.h"

#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>

#include <string>
using std::string;

FileStreaming::FileStreaming(string file, int socket_fd, string,
		const StreamingParameters &streaming_parameters,
		const ConfigMap &)
{
	size_t			max_fill_socket = 0;
	struct pollfd	pfd;
	off_t			file_offset = 0;
	const char *	http_ok			=	"HTTP/1.1 200 OK\r\n";
	const char *	http_partial	=	"HTTP/1.1 206 Partial Content\r\n";
	const char *	http_headers	=	"Connection: Close\r\n"
										"Content-Type: video/mpeg\r\n"
										"Server: Streamproxy\r\n"
										"Accept-Ranges: bytes\r\n";
	string			http_reply;
	Queue			socket_queue(1024 * 1024);
	int				time_offset_s = 0;
	off_t			byte_offset = 0;
	off_t			http_range = 0;
	int				pct_offset = 0;
	bool			partial = false;

	if(streaming_parameters.count("startfrom"))
		time_offset_s = TimeOffset(streaming_parameters.at("startfrom")).as_seconds();

	if(streaming_parameters.count("http_range"))
		byte_offset = Util::string_to_uint(streaming_parameters.at("http_range"));

	if(streaming_parameters.count("byte_offset"))
		byte_offset = Util::string_to_uint(streaming_parameters.at("byte_offset"));

	if(streaming_parameters.count("pct_offset"))
		pct_offset = Util::string_to_uint(streaming_parameters.at("pct_offset"));

	MpegTS stream(file, "", time_offset_s > 0);
	
	Util::vlog("FileStreaming: streaming file: %sd", file.c_str());
	Util::vlog("FileStreaming: byte_offset: %llu / %llu (%llu %%)", byte_offset, stream.stream_length,
			(byte_offset * 100) / stream.stream_length);
	Util::vlog("FileStreaming: pct_offset: %d", pct_offset);
	Util::vlog("FileStreaming: time_offset: %d", time_offset_s);

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

	Util::vlog("FileStreaming: file_offset: %lld", file_offset);

	if(partial)
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

	for(;;)
	{
		if(socket_queue.usage() < 50)
		{
			if(!socket_queue.read(stream.get_fd()))
			{
				Util::vlog("FileStreaming: eof");
				break;
			}
		}

		if(socket_queue.usage() > max_fill_socket)
			max_fill_socket = socket_queue.usage();

		pfd.fd		= socket_fd;
		pfd.events	= POLLRDHUP;

		if(socket_queue.length() > 0)
			pfd.events |= POLLOUT;

		if(poll(&pfd, 1, -1) <= 0)
			throw(trap("FileStreaming: poll error"));

		if(pfd.revents & (POLLRDHUP | POLLHUP))
		{
			Util::vlog("FileStreaming: client hung up");
			break;
		}

		if(pfd.revents & (POLLERR | POLLNVAL))
		{
			Util::vlog("FileStreaming: socket error");
			break;
		}

		if(pfd.revents & POLLOUT)
		{
			if(!socket_queue.write(socket_fd))
			{
				Util::vlog("FileStreaming: write socket error");
				break;
			}
		}
	}

	Util::vlog("FileStreaming: streaming ends, socket max queue fill: %d%%", max_fill_socket);
}
