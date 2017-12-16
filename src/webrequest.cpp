#include "config.h"

#include "webrequest.h"
#include "types.h"
#include "configmap.h"
#include "util.h"
#include "stbtraits.h"

#include <string>
using std::string;

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <string.h>

WebRequest::WebRequest(const ConfigMap &config_map_in, const HeaderMap &headers_in,
		const CookieMap &cookies_in, const UrlParameterMap &parameters_in,
		const stb_traits_t &stb_traits_in)
	:
		config_map(config_map_in), headers(headers_in), cookies(cookies_in), parameters(parameters_in),
		stb_traits(stb_traits_in)
{
}

string WebRequest::page_test_cookie(string &mimetype) const
{
	mimetype = "text/html";

	string data =
		"<script LANGUAGE=\"JavaScript\">\n"
		"	cookie_name = \"testcookie\";\n"
		"	var entered;\n"
		"\n"
		"	function putCookie()\n"
		"	{\n"
		"		if(document.cookie != document.cookie)\n"
		"		{\n"
		"			index = document.cookie.indexOf(cookie_name);\n"
		"		}\n"
		"		else\n"
		"		{\n"
		"			index = -1;\n"
		"		}\n"
		"\n"
		" 		if (index == -1)\n"
		"		{\n"
		"			entered = document.cf.cfd.value;\n"
		"			document.cookie = cookie_name + \" = \" + entered;\n"
		"			document.cookie = \"another_test=test\";\n"
		"		}\n"
		"	}\n"
		"</script>\n"
		"<form name=\"cf\">\n"
		"	Enter a word: <input type=\"text\" name=\"cfd\" size=\"20\">\n"
		"	<input type=\"button\" value=\"set to cookie\" onClick=\"putCookie()\">\n"
		"</form>\n";

	return(data);
}

string WebRequest::page_info(string &mimetype) const
{
	ConfigMap::const_iterator		itm;
	HeaderMap::const_iterator		ithm;
	CookieMap::const_iterator		itcm;
	UrlParameterMap::const_iterator	itpm;
	bool							xml;
	string							data;

	xml = !!parameters.count("xml") && (parameters.at("xml") == "1");

	if(xml)
	{
		mimetype = "text/xml";

		data += "<streamproxy>\n";
		data += "	<default_settings>\n";

		for(itm = config_map.begin(); itm != config_map.end(); itm++)
		{
			data += "		<setting_string id=\"" + itm->first + "\">" +
				itm->second.string_value + "</setting_string>\n";
			data += "		<setting_int id=\"" + itm->first + "\">" +
				Util::int_to_string(itm->second.int_value) + "</setting_int>\n";
		}

		data += "	</default_settings>\n";
		data += "	<headers>\n";

		for(ithm = headers.begin(); ithm != headers.end(); ithm++)
			data += "		<header id=\"" + ithm->first + "\">" + ithm->second + "</header>\n";

		data += "	</headers>\n";
		data += "	<cookies>\n";

		for(itcm = cookies.begin(); itcm != cookies.end(); itcm++)
			data += "<cookie id=\"" + itcm->first + "\">" + itcm->second + "</cookie>\n";

		data += "	</cookies>\n";
		data += "	<urlparameters>\n";

		for(itpm = parameters.begin(); itpm != parameters.end(); itpm++)
			data += "		<urlparameter id=\"" + itpm->first + "\">" + itpm->second + "</urlparameter>\n";

		data += "	</urlparameters>\n";
		data += "   <traits>\n";
		data += string("        <manufacturer>") + stb_traits.manufacturer + "</manufacturer>\n";
		data += string("        <model>") + stb_traits.model + "</model>\n";
		data += string("        <transcoding>") + ((stb_traits.transcoding_type == stb_transcoding_vuplus) ? "vuplus" : "enigma") + "</transcoding>\n";
		data += string("        <chipset>") + stb_traits.chipset + "</chipset>\n";
		data += string("        <encoders>") + Util::int_to_string(stb_traits.encoders) + "</encoders>\n";
		data += "        <features>\n";

		for(unsigned int feature_index = 0; feature_index < stb_traits.num_features; feature_index++)
		{
			const stb_feature_t *feature = &stb_traits.features[feature_index];

			data += string("            <feature_entry id=\"") + feature->id + "\">\n";
			data += string("<settable>") + (feature->settable ? "yes" : "no") + "</settable>\n";
			data += string("<description>") + feature->description + "</description>\n";
			data += string("<proc_entry>") + (feature->api_data ? feature->api_data : "") + "</proc_entry>\n";

			switch(feature->type)
			{
				case(stb_traits_type_bool):
				{
					data += string("<default>") + (feature->value.bool_type.default_value ? "yes" : "no") + "</default>";
					break;
				}

				case(stb_traits_type_int):
				{
					data += "<type>integer</type>";
					data += string("<default>") + Util::int_to_string(feature->value.int_type.default_value) + "</default>";
					data += string("<min>") + Util::int_to_string(feature->value.int_type.min_value) + "</min>";
					data += string("<max>") + Util::int_to_string(feature->value.int_type.max_value) + "</max>";
					break;
				}

				case(stb_traits_type_string):
				{
					data += "<type>string</type>";
					data += string("<default>") + feature->value.string_type.default_value + "</default>";
					data += string("<min>") + Util::int_to_string(feature->value.string_type.min_length) + "</min>";
					data += string("<max>") + Util::int_to_string(feature->value.string_type.max_length) + "</max>";
					break;
				}

				case(stb_traits_type_string_enum):
				{
					const char * const * enum_values = feature->value.string_enum_type.enum_values;

					data += string("<type>enum</type>");
					data += string("<default>") + feature->value.string_enum_type.default_value + "</default>";
					data += "<values>";

					for(int enum_index = 0; (enum_index < 16) && enum_values[enum_index]; enum_index++)
						data += "<value id=\"" + string(enum_values[enum_index]) + "\"/>";

					data += "</values>";

					break;
				}

				default:
				{
					data += "<type>unknown</type>";

					break;
				}
			}

			data += "</feature_entry>\n";
		}

		data += "        </features>\n";
		data += "    </traits>\n";
		data += "</streamproxy>\n";
	}
	else
	{
		mimetype = "text/html";

		data += "<div style=\"margin: 12px 0px 0px 0px;\">\n";
		data += "	<table border=\"1\">\n";
		data += "		<tr><th colspan=\"3\">default settings</th></tr>\n";

		for(itm = config_map.begin(); itm != config_map.end(); itm++)
			data += string("		<tr>") +
				"<td>" + itm->first + "</td>" +
				"<td>" + itm->second.string_value + "</td>" +
				"<td>" + Util::int_to_string(itm->second.int_value) + "</td>" +
				"</tr>\n";

		data += "	</table>\n";
		data += "</div>\n";

		data += "<div style=\"margin: 12px 0px 0px 0px;\">\n";
		data += "	<table border=\"1\">\n";
		data += "		<tr><th colspan=\"2\">headers</th></tr>\n";

		for(ithm = headers.begin(); ithm != headers.end(); ithm++)
			data += "		<tr><td>" + ithm->first + "</td><td>" + ithm->second + "</td></tr>\n";

		data += "	</table>\n";
		data += "</div>\n";

		data += "<div style=\"margin: 12px 0px 0px 0px;\">\n";
		data += "	<table border=\"1\">\n";
		data += "		<tr><th colspan=\"2\">cookies</th></tr>\n";

		for(itcm = cookies.begin(); itcm != cookies.end(); itcm++)
			data += "		<tr><td>" + itcm->first + "</td><td>" + itcm->second + "</td></tr>\n";

		data += "	</table>\n";
		data += "</div>\n";

		data += "<div style=\"margin: 12px 0px 0px 0px;\">\n";
		data += "	<table border=\"1\">\n";
		data += "		<tr><th colspan=\"2\">url parameters</th></tr>\n";

		for(itpm = parameters.begin(); itpm != parameters.end(); itpm++)
			data += "		<tr><td>" + itpm->first + "</td><td>" + itpm->second + "</td></tr>\n";

		data += "	</table>\n";
		data += "</div>\n";

		data += "<div style=\"margin: 12px 0px 0px 0px;\">\n";
		data += "	<table border=\"1\">\n";
		data += "		<tr><th colspan=\"9\">stb features</th></tr>\n";
		data += "       <tr><th colspan=\"2\">manufacturer</th><th>model</th><th>transcoding</th><th colspan=\"2\">chipset</th><th colspan=\"2\">encoders</th>\n";
		data += "       ";
		data += string("<tr><td colspan=\"2\">") + stb_traits.manufacturer + "</td><td>" + stb_traits.model + "</td><td>" + ((stb_traits.transcoding_type == stb_transcoding_vuplus) ? "vuplus" : "enigma") + "</td>\n" ;
		data += string("<td colspan=\"2\">") + stb_traits.chipset + "</td><td colspan=\"2\">" + Util::int_to_string(stb_traits.encoders) + "</td></tr>";
		data += "		<tr><th>name</th><th>settable</th><th>description</th><th>proc entry</th>\n";
		data += "		    <th>type</th><th>default</th><th>min-max/choices</th></tr>\n";

		for(unsigned int feature_index = 0; feature_index < stb_traits.num_features; feature_index++)
		{
			const stb_feature_t *feature = &stb_traits.features[feature_index];
			data += string("		<tr><td>") + feature->id + "</td><td>" + (feature->settable ? "yes" : "no") + "</td>";
			data += string("<td>") + feature->description + "</td><td>" + (feature->api_data ? feature->api_data : "") + "</td>";

			switch(feature->type)
			{
				case(stb_traits_type_bool):
				{
					data += string("<td>bool</td><td>") + (feature->value.bool_type.default_value ? "yes" : "no") + "</td>";
					break;
				}

				case(stb_traits_type_int):
				{
					data += string("<td>integer</td><td>") + Util::int_to_string(feature->value.int_type.default_value) + "</td>";
					data += string("<td>") + Util::int_to_string(feature->value.int_type.min_value);
					data += string("-") + Util::int_to_string(feature->value.int_type.max_value) + "</td>";
					break;
				}

				case(stb_traits_type_string):
				{
					data += string("<td>string</td><td>") + feature->value.string_type.default_value + "</td>";
					data += string("<td>") + Util::int_to_string(feature->value.string_type.min_length);
					data += string("-") + Util::int_to_string(feature->value.string_type.max_length) + "</td>";
					break;
				}

				case(stb_traits_type_string_enum):
				{
					const char * const * enum_values = feature->value.string_enum_type.enum_values;

					data += string("<td>enum</td><td>") + feature->value.string_enum_type.default_value + "</td>";
					data += "<td>";

					for(int enum_index = 0; (enum_index < 16) && enum_values[enum_index]; enum_index++)
						data += string(enum_values[enum_index]) + "&nbsp;";

					data += "</td>";

					break;
				}

				default:
				{
					data += "<td>unknown!</td>";

					break;
				}
			}

			data += "</tr>\n";
		}

		data += "	</table>\n";
		data += "</div>\n";
	}

	return(data);
}

string WebRequest::get(string &mimetype) const
{
	string request, html, data;

	if(parameters.count("request"))
		request = parameters.at("request");

	if((request == "") || (request == "info"))
		data = page_info(mimetype);

	if(request == "cookie")
		data = page_test_cookie(mimetype);

	if(!data.length())
		return("");

	html = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";

	if((mimetype == "text/html") || (mimetype == "text/xhtml"))
	{
		html += "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n";
		html += "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
		html += "<head>\n";
		html += "	<title>Streamproxy - " + request + "</title>\n";
		html += "</head>\n";
		html += "<body>\n";
	}

	html += data;

	if((mimetype == "text/html") || (mimetype == "text/xhtml"))
	{
		html += "</body>\n";
		html += "</html>\n";
	}

	return(html);
}
