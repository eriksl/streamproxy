#include "configmap.h"
#include "config.h"

#include <util.h>

#include <string>
using std::string;

ConfigValue::ConfigValue() throw()
{
	int_value = 0;
}

ConfigValue::ConfigValue(string in) throw()
{
	string_value = in;
	int_value = Util::string_to_int(in);
}

ConfigValue::ConfigValue(int in) throw()
{
	string_value = Util::int_to_string(in);
	int_value = in;
}

ConfigValue::ConfigValue(bool in) throw()
{
	if(!in)
	{
		string_value = "false";
		int_value = 0;
	}
	else
	{
		string_value = "true";
		int_value = 1;
	}
}
