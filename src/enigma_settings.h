#ifndef _enigma_settings_h_
#define _enigma_settings_h_

#include <string>
#include <map>

class EnigmaSettings
{
	private:

		typedef std::map<std::string, std::string> key_value_t;

		EnigmaSettings(const EnigmaSettings &);

		key_value_t	key_values;

	public:

		EnigmaSettings() throw(std::string);
		bool exists(std::string key) const throw();
		std::string as_string(std::string key) const throw(std::string);
		int as_int(std::string key) const throw(std::string);
};

#endif
