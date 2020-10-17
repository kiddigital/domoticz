/************************************************************************

WebServer OpenAPI class
Author: kiddigital (github.com/kiddigital)

15/10/2020 1.0 Creation

License: Public domain

************************************************************************/
#pragma once
#include <string>
#include <boost/function.hpp>

class CWebServerOpenAPI
{
	typedef boost::function< void(const std::string& params, Json::Value &result) > openapi_command_function;
public:
	CWebServerOpenAPI();
	~CWebServerOpenAPI(void);

	bool HandleRequest(const std::string method, const std::string uri, Json::Value& root);
private:
	void Init();
	bool ParseURI(const std::string uri);
	void RegisterCommand(const char* command, openapi_command_function CommandFunction);
	bool FindCommand(const std::string& command);
	bool HandleCommand(const std::string& command, const std::string& params, Json::Value& result);

	std::map < std::string, openapi_command_function > m_openapicommands;

	uint16_t m_httpcode;
	std::string m_uri;
	std::string m_path;
	std::string m_params;
	std::string m_command;
	std::string m_altparams;
	std::string m_altcommand;
	Json::Value m_resultjson;

	// List of OpenAPI supported commands
	// Make sure they also get registered in the class constructor! 
	bool GetCustomData(const std::string& params, Json::Value& result);
};
