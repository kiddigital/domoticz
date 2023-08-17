/************************************************************************

WebServer OpenAPI class
Author: kiddigital (github.com/kiddigital)

15/10/2020 1.0 Creation

************************************************************************/
#pragma once
#include "stdafx.h"
#include "WebServerOpenAPI_v2.h"
#include "Logger.h"
#include "json_helper.h"
#include "Helper.h"
#include "SQLHelper.h"
#include "mainworker.h"
#include "../webserver/cWebem.h"
#include "../webserver/reply.hpp"
#include <sstream>
#include <string>

namespace http
{
	namespace server
	{

class CWebServerOpenAPI_v2
{
	typedef std::function< void(const Json::Value &input, Json::Value &result) > openapi_command_function;
public:
	CWebServerOpenAPI_v2();
	~CWebServerOpenAPI_v2(void);

	bool gHandleRequest(const std::string method, const std::string uri, std::multimap<std::string, std::string> parameters, const int8_t userrights);
	http::server::reply::status_type gGetResultCode();
	Json::Value gGetResultJSON();
private:
	void gInit();
	bool gParseURI(const std::string uri);
	void gRegisterCommand(const char* command, openapi_command_function CommandFunction, const uint8_t rights);
	bool gFindCommand(const std::string& command);
	bool gHandleCommand(const std::string& command, const Json::Value& input, Json::Value& result);

	std::map < std::string, openapi_command_function > m_openapicommands;
	std::map < std::string, int8_t > m_openapicommandrights;

	http::server::reply m_httpresult;
	std::string m_uri;
	std::string m_path;
	std::string m_params;
	std::string m_command;
	std::string m_altparams;
	std::string m_altcommand;
	Json::Value m_resultjson;
	int8_t m_userrights;

	// List of OpenAPI supported commands
	// Make sure they also get registered in the class constructor! 

	/* Devices */
	void GetDevice(const Json::Value& input, Json::Value& result);
};

	} // namespace server
} // namespace http