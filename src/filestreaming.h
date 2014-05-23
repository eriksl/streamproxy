#ifndef _filestreaming_h_
#define _filestreaming_h_

#include "config.h"
#include "trap.h"

#include <string>

class FileStreaming
{
	private:

		FileStreaming()					throw();
		FileStreaming(FileStreaming &)	throw();

	public:

		FileStreaming(std::string file, int socketfd, int time_offset_s)
				throw(trap);
};

#endif
