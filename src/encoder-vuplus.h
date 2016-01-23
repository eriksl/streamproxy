#ifndef _encoder_vuplus_h_
#define _encoder_vuplus_h_

#include "config.h"
#include "trap.h"
#include "stbtraits.h"

#include "types.h"
#include <string>
#include <pthread.h>

class EncoderVuPlus
{
	private:

		enum
		{
			IOCTL_VUPLUS_START_TRANSCODING	= 100,
			IOCTL_VUPLUS_STOP_TRANSCODING	= 200,
		};

		EncoderVuPlus();
		EncoderVuPlus(const EncoderVuPlus &);

		pthread_t					start_thread;
		bool						start_thread_running;
		bool						start_thread_joined;
		bool						stopped;
		int							fd;
		int							encoder;
		PidMap						pids;
		const StreamingParameters	&streaming_parameters;
		const stb_traits_t			&stb_traits;

		static void* start_thread_function(void *);

	public:

		EncoderVuPlus(const PidMap &,
				const stb_traits_t &,
				const StreamingParameters &)					throw(trap);
		~EncoderVuPlus()										throw();

		std::string	getprop(std::string)				const	throw();
		void		setprop(const std::string &,
							const std::string &)		const	throw();

		bool		start_init()								throw();
		bool		start_finish()								throw();
		bool		stop()										throw();

		int			getfd()								const	throw();
		PidMap		getpids()							const	throw();
};

#endif
