#ifndef _encoder_h_
#define _encoder_h_

#include "trap.h"
#include "types.h"

#include <string>

class Encoder
{
	private:

		Encoder(const Encoder &);

	public:

		Encoder()												throw(trap);
		virtual ~Encoder()										throw();

		virtual std::string		getprop(std::string)	const	throw() = 0;
		virtual void			setprop(const std::string &,
								const std::string &)	const	throw() = 0;

		virtual bool			start_init()					throw() = 0;
		virtual bool			start_finish()					throw() = 0;
		virtual bool			stop()							throw() = 0;

		virtual int				getfd()					const	throw()	= 0;
		virtual PidMap			getpids()				const	throw() = 0;
};

#endif
