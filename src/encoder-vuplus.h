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
				const StreamingParameters &);
		~EncoderVuPlus();

		std::string	getprop(std::string)				const;
		void		setprop(const std::string &,
							const std::string &)		const;

		bool		start_init();
		bool		start_finish();
		bool		stop();

		int			getfd()								const;
		PidMap		getpids()							const;
};

#endif
