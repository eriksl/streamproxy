#ifndef _filestreaming_h_
#define _filestreaming_h_

#include "config.h"
#include "trap.h"

#include <string>
#include <sys/types.h>

class FileStreaming
{
	private:

		FileStreaming()					throw();
		FileStreaming(FileStreaming &)	throw();

	public:

		FileStreaming(std::string file, int socketfd,
				off_t byte_offset, int pct_offset, int time_offset_s)
										throw(trap);
};

#endif
