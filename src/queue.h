#ifndef _queue_h_
#define	_queue_h_

#include "config.h"
#include "trap.h"

#include <sys/types.h>

#include <string>
using std::string;

class Queue
{
	private:

		size_t	inptr;
		size_t	outptr;
		size_t	buffer_size;
		char	*buffer;

		Queue();
		Queue(const Queue &);

	public:

		Queue(int size);
		~Queue();

		void	reset();
		size_t	size()									const;
		size_t	length()								const;
		size_t	usage()									const;
		void	append(size_t length, const char *data);
		size_t	extract(size_t length, char *data);
		bool	read(int fd, ssize_t maxread = -1);
		bool	write(int fd, ssize_t maxwrite = -1);
};

#endif
