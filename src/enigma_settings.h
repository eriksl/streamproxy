#ifndef _enigma_settings_h_
#define _enigma_settings_h_

#include "config.h"
#include "trap.h"

#include <string>
#include <map>

class EnigmaSettings
{
	private:

		typedef std::map<std::string, std::string> key_value_t;

		EnigmaSettings(const EnigmaSettings &);

		key_value_t	key_values;

	public:

		EnigmaSettings();
		bool exists(std::string key) const;
		std::string as_string(std::string key) const;
		int as_int(std::string key) const;
};

#endif
