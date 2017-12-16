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

		ConfigValue();
		ConfigValue(std::string, int);
		ConfigValue(std::string);
		ConfigValue(int);
		ConfigValue(bool);
};

typedef std::map<std::string, ConfigValue> ConfigMap;

#endif
