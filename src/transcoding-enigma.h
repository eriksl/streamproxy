#ifndef _transcoding_enigma_h_
#define _transcoding_enigma_h_

#include "config.h"
#include "trap.h"
#include "stbtraits.h"
#include "types.h"

#include <string>

class TranscodingEnigma
{
	private:

		TranscodingEnigma();
		TranscodingEnigma(TranscodingEnigma &);

	public:

		TranscodingEnigma(const std::string &service,
				int socketfd, std::string webauth,
				const stb_traits_t &stb_traits_in,
                const StreamingParameters &streaming_parameters);
};

#endif
