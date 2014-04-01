#include <unistd.h>

#include <string>
using std::string;

#include "mpegts.h"
#include "vlog.h"

MpegTS::MpegTS(int fd_in) throw()
{
	fd = fd_in;
}

//MpegTS::~MpegTS() throw()
//{
//}

void MpegTS::probe() throw(string)
{


	
}
