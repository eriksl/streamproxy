#ifndef _service_h_
#define _service_h_

#include "config.h"
#include "trap.h"

#include <string>
#include <vector>

class Service
{
	public:

		typedef std::vector<int> intvector;

	private:

		bool		valid;
		std::string	service;
		intvector	service_field;

		Service();
		Service(const Service &);

	public:
						Service(std::string service);
		bool			is_valid() const;
		std::string		service_string() const;
		intvector		service_vector() const;
};

#endif
