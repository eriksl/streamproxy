#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "queue.h"
#include "vlog.h"

Queue::Queue(int size_in) throw(string)
{
	buffer_size = size_in;

	if((buffer = (char *)malloc(buffer_size)) == 0)
		throw("Queue: malloc error");

	Queue::reset();
}

Queue::~Queue() throw()
{
	free(buffer);
}

void Queue::reset() throw()
{
	inptr	= 0;	// where the queue gets filled
	outptr	= 0;	// where the queue gets emptied
}
size_t Queue::size() const throw()
{
	return(buffer_size);
}

size_t Queue::length() const throw()
{
	size_t len;

	if(inptr >= outptr)
		len = inptr - outptr;
	else
		len = (buffer_size - outptr) + inptr;

	return(len);
}

size_t Queue::usage() const throw()
{
	return((length() * 100) / buffer_size);
}

void Queue::append(size_t len, const char *data) throw()
{
	size_t chunk;

	while(len > 0)
	{
		chunk = buffer_size - inptr;

		if(chunk > len)
			chunk = len;

		memcpy(&buffer[inptr], data, chunk);

		data	+= chunk;
		len		-= chunk;
		inptr	+= chunk;

		if(inptr >= buffer_size)
			inptr = 0;
	}
}

size_t Queue::extract(size_t data_size, char *data) throw()
{
	size_t chunk, len = 0;

	if(inptr < outptr) //  active data is wrapped
	{
		//vlog("data is wrapped");
		
		chunk = buffer_size - outptr;

		if(chunk > data_size)
			chunk = data_size;

		memcpy(data, &buffer[outptr], chunk);

		data		+= chunk;
		data_size	-= chunk;
		len			+= chunk;
		outptr		+= chunk;

		if(outptr >= buffer_size)
			outptr = 0;
	}

	if(data_size > 0)
	{
		//vlog("data is straight or remaining, out_length: %d, out_size: %d, inptr: %d, outptr: %d",
			//(int)out_length, (int)out_size, (int)queue->inptr, (int)queue->outptr);
	
		chunk = inptr - outptr;

		if(chunk > data_size)
			chunk = data_size;

		memcpy(data, &buffer[outptr], chunk);

		len		+= chunk;
		outptr	+= chunk;
	}

	return(len);
}

bool Queue::read(int fd, ssize_t maxread) throw()
{
	ssize_t	rv, chunk;

	chunk = buffer_size - inptr;

	if((size_t)chunk > (buffer_size / 4))
		chunk = buffer_size / 4;

	if((maxread > 0) && (chunk > maxread))
		chunk = maxread;

	if(chunk <= 0)
	{
		vlog("queue_read: chunk <= 0: %d", chunk);
		return(true);
	}

	rv = ::read(fd, &buffer[inptr], chunk);

	//vlog("*** queue_read: %d bytes received", rv);

	if(rv < 0)
	{
		if(errno == EAGAIN)
		{
			errno = 0;
			return(true);
		}
		else
			return(false);
	}

	if(rv == 0)
	{
		if(length() > 0)
			return(true);
		else
			return(false);
	}

	if((inptr < outptr) && ((inptr + rv) > outptr))
	{
		vlog("!!! queue overrun: inptr was: %d, inptr is %d, outptr = %d, rv is %d",
				inptr, inptr + rv, outptr, rv);
	}

	inptr += rv;

	if(inptr > buffer_size)
		vlog("**** queue_read: corruption");

	if(inptr >= buffer_size)
		inptr = 0;

	return(true);
}

bool Queue::write(int fd, ssize_t maxwrite) throw()
{
	ssize_t		chunk;
	ssize_t		rv;
	const void	*start;

	start = &buffer[outptr];

	if(inptr >= outptr)
		chunk = inptr - outptr;
	else
		chunk = buffer_size - outptr;

	if((maxwrite > 0) && (chunk > maxwrite))
		chunk = maxwrite;

	if(chunk > 0)
		rv = ::write(fd, start, chunk);
	else
		rv = 0;

	//vlog("*** queue_write: written %d bytes", rv);

	if(rv < 0)
	{
		if(errno == EAGAIN)
		{
			errno = 0;
			return(true);
		}
		else
			return(false);
	}

	if(rv == 0)
		return(false);

	outptr += rv;

	if(outptr > buffer_size)
		vlog("**** queue_write: corruption");

	if(outptr >= buffer_size)
		outptr = 0;

	return(true);
}
