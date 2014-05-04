#include "config.h"

#include "webrequest.h"
#include "types.h"
#include "configmap.h"
#include "util.h"

#include <string>
using std::string;

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <string.h>

WebRequest::WebRequest(const ConfigMap &config_map_in, const HeaderMap &headers_in,
		const CookieMap &cookies_in, const UrlParameterMap &parameters_in) throw()
	:
		config_map(config_map_in), headers(headers_in), cookies(cookies_in), parameters(parameters_in)
{
}

string WebRequest::page_test_cookie(string &mimetype) const throw()
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

string WebRequest::page_info(string &mimetype) const throw()
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
	}

	return(data);
}

string WebRequest::get(string &mimetype) const throw()
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
