#ifndef _encoder_broadcom_h_
#define _encoder_broadcom_h_

#include "config.h"
#include "trap.h"
#include "stbtraits.h"

#include "types.h"
#include <string>
#include <pthread.h>

class EncoderBroadcom
{
	private:

		enum
		{
			IOCTL_BROADCOM_START_TRANSCODING	= 100,
			IOCTL_BROADCOM_STOP_TRANSCODING	= 200,
		};

		EncoderBroadcom();
		EncoderBroadcom(const EncoderBroadcom &);

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

		EncoderBroadcom(const PidMap &,
				const stb_traits_t &,
				const StreamingParameters &);
		~EncoderBroadcom();

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
