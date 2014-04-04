#ifndef _livestreaming_h_
#define _livestreaming_h_

#include "service.h"
#include <string>

class LiveStreaming
{
	private:

		LiveStreaming()					throw();
		LiveStreaming(LiveStreaming &)	throw();

	public:

		LiveStreaming(const Service &service, int socketfd,
				std::string webauth) throw(std::string);
};

#endif
