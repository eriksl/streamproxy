#include "config.h"
#include "trap.h"

#include "enigma_settings.h"
#include "util.h"

#include <string>
using std::string;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

EnigmaSettings::EnigmaSettings()
{
	FILE *fp;
	char buffer[1024];
	char *value;
	char *eol;
	key_value_t::const_iterator it;

	if(!(fp = fopen("/etc/enigma2/settings", "r")))
	{
		Util::vlog("cannot open enigma2 settings file");
		return;
	}

	while(fgets(buffer, sizeof(buffer), fp))
	{
		if(!(value = strchr(buffer, '=')))
		{
			Util::vlog("line does not contain delimiter: \"%s\"", buffer);
			continue;
		}

		*value++ = '\0';

		if((eol = strchr(value, '\r')))
			*eol = '\0';

		if((eol = strchr(value, '\n')))
			*eol = '\0';

		key_values[buffer] = value;
	}

	//for(it = key_values.begin(); it != key_values.end(); it++)
		//Util::vlog("> \"%s\"=\"%s\"", it->first.c_str(), it->second.c_str());

	fclose(fp);
}

bool EnigmaSettings::exists(string key) const
{
	return(!!key_values.count(key));
}

string EnigmaSettings::as_string(string key) const
{
	if(!exists(key))
		throw(trap("key does not exist"));

	return(key_values.at(key));
}

int EnigmaSettings::as_int(string key) const
{
	string value = as_string(key);

	return(strtol(value.c_str(), 0, 0));
}
