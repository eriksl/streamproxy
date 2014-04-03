#ifndef _filestreaming_h_
#define _filestreaming_h_

#include "queue.h"

#include <string>

class FileStreaming
{
	private:

		FileStreaming()					throw();
		FileStreaming(FileStreaming &)	throw();

		int		fd;
		int		socket_fd;
		Queue	socket_queue;

	public:

		FileStreaming(std::string file, int socketfd) throw(std::string);
		~FileStreaming() throw();
};

#endif
