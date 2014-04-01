#ifndef _demuxer_h_
#define _demuxer_h_

#include "pidmap.h"

#include <stdlib.h>

#include <string>
using std::string;

#include <map>
using std::map;

class Demuxer
{
	private:

		Demuxer();
		Demuxer(const Demuxer &);

	private:

		static const int buffer_size = 1024 * 188;

		PidMap	pids;
		int		id;
		int		fd;

	public:

		Demuxer(int id, const PidMap &)	throw(string);
		~Demuxer()						throw();

		int getfd()				const	throw();
};

#endif
