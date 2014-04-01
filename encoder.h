#ifndef _encoder_h_
#define _encoder_h_

#include <pthread.h>

#include <string>
using std::string;

#include "pidmap.h"

class Encoder
{
	private:

		enum
		{
			IOCTL_VUPLUS_SET_VPID			= 1,
			IOCTL_VUPLUS_SET_APID			= 2,
			IOCTL_VUPLUS_SET_PMTPID			= 3,
			IOCTL_VUPLUS_START_TRANSCODING	= 100,
			IOCTL_VUPLUS_STOP_TRANSCODING	= 200,
		};

		Encoder();
		Encoder(const Encoder &);

		pthread_t	start_thread;
		bool		start_thread_running;
		bool		start_thread_joined;
		bool		stopped;
		int			fd;
		PidMap		pids;

		static void* start_thread_function(void *);

	public:

		Encoder(const PidMap &)					throw(string);
		~Encoder()								throw();

		void	start_init()					throw(string);
		void	start_finish()					throw(string);
		void	stop()							throw(string);

		int		getfd()					const	throw();
		PidMap	getpids()				const	throw();
};

#endif
