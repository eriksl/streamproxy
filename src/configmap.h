#ifndef _configmap_h_
#define _configmap_h_

#include "config.h"

#include <string>
#include <map>

class ConfigValue
{
	public:

		std::string	string_value;
		int			int_value;

		ConfigValue()					throw();
		ConfigValue(std::string, int)	throw();
		ConfigValue(std::string)		throw();
		ConfigValue(int)				throw();
		ConfigValue(bool)				throw();
};

typedef std::map<std::string, ConfigValue> ConfigMap;

#endif
