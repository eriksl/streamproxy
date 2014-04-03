#ifndef _queue_h_
#define	_queue_h_

#include <stdlib.h>

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

		Queue(int size)		throw(string);
		~Queue()			throw();

		void	reset()											throw();
		size_t	size()									const	throw();
		size_t	length()								const	throw();
		size_t	usage()									const	throw();
		void	append(size_t length, const char *data)			throw();
		size_t	extract(size_t length, char *data)				throw();
		bool	read(int fd, ssize_t maxread = -1)				throw();
		bool	write(int fd, ssize_t maxwrite = -1)			throw();
};

#endif
