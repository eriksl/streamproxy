#include "config.h"
#include "trap.h"

#include "enigma_settings.h"
#include "util.h"

#include <string>
using std::string;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(__GNUC__) && (__GNUC__ >= 7)
#define FALL_THROUGH __attribute__ ((fallthrough))
#else
#define FALL_THROUGH ((void)0)
#endif /* __GNUC__ >= 7 */

typedef enum
{
	state_skip_nl,
	state_key,
	state_value,
} state_t;

EnigmaSettings::EnigmaSettings()
{
	FILE *fp;
	string key, value;
	int current_char;
	key_value_t::const_iterator it;
	state_t state;
	enum { key_value_size = 64 };

	if(!(fp = fopen("/etc/enigma2/settings", "r")))
	{
		Util::vlog("cannot open enigma2 settings file");
		return;
	}

	state = state_key;

	for(;;)
	{
		if((current_char = fgetc(fp)) == EOF)
			break;

		switch(state)
		{
			case(state_skip_nl):
			{
				if((current_char == '\n') || (current_char == '\r'))
					continue;

				state = state_key;
				FALL_THROUGH;
			}

			case(state_key):
			{
				if(current_char == '=')
				{
					value.clear();
					state = state_value;
					continue;
				}

				if((current_char == '\n') || (current_char == '\r'))
				{
					// line ends but we didn't see an '=' yet
					Util::vlog("line does not contain delimiter: \"%s\"", key.c_str());
					value.clear();
					key.clear();
					// after a CR a NL may follow or vv., skip it and then try again
					state = state_skip_nl;
					continue;
				}

				if(key.length() < key_value_size)
					key.append(1, current_char);

				break;
			}

			case(state_value):
			{
				if((current_char == '\n') || (current_char == '\r'))
				{
					if((key.length() < key_value_size) && (value.length() < key_value_size))
						key_values[key] = value;
					key.clear();
					value.clear();
					// after a CR a NL may follow or vv., skip it and then try again
					state = state_skip_nl;
					continue;
				}

				if(value.length() < key_value_size)
					value.append(1, current_char);

				break;
			}
		}
	}

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
