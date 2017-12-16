#ifndef _filestreaming_h_
#define _filestreaming_h_

#include "config.h"
#include "trap.h"
#include "types.h"
#include "configmap.h"

#include <string>
#include <sys/types.h>

class FileStreaming
{
	private:

		FileStreaming();
		FileStreaming(FileStreaming &);

	public:

		FileStreaming(std::string file, int socketfd, std::string webauth,
				const StreamingParameters &streaming_parameters,
				const ConfigMap &config_map);
};

#endif
