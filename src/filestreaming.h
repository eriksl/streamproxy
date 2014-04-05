#ifndef _filestreaming_h_
#define _filestreaming_h_

#include <string>

class FileStreaming
{
	private:

		int file_fd;

		FileStreaming()					throw();
		FileStreaming(FileStreaming &)	throw();

	public:

		FileStreaming(std::string file, int socketfd, int time_offset)
				throw(std::string);
		~FileStreaming() throw();
};

#endif
