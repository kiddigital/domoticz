/************************************************************************

Implementation of WebServerOpenAPI class
Author: KidDigital (github.com/kiddigital)

15/10/2020 1.0 Creation

License: Public domain

************************************************************************/
#include "stdafx.h"
#include "WebServerOpenAPI.h"
#include "../../main/Logger.h"
#include "../../main/json_helper.h"
//#include "../../main/Helper.h"
//#include "../../httpclient/UrlEncode.h"
//#include "../../httpclient/HTTPClient.h"
//#include "../../main/SQLHelper.h"
//#include "../../webserver/Base64.h"
#include <sstream>
#include <iomanip>
#include <boost/bind.hpp>

#define TESTDEFINE "testing123"

CWebServerOpenAPI::CWebServerOpenAPI()
{
	Init();

	// List of the commands available to register with their command name
	// NOTE: make sure the request method (GET, POST, DELETE, etc) is written in CAPS and the rest in lowercase!
	RegisterCommand("GETcustomdata", boost::bind(&CWebServerOpenAPI::GetCustomData, this, _1, _2));
}

CWebServerOpenAPI::~CWebServerOpenAPI()
{
}

bool CWebServerOpenAPI::HandleRequest(const std::string method, const std::string uri, Json::Value& root)
{
	Json::Value result;

	_log.Debug(DEBUG_WEBSERVER, "WebServerOpenAPI: Handling request (%s) %s", method.c_str(), uri.c_str());

	Init();

	if (!ParseURI(uri))
		return false;

	// Add method to command
	m_command = method + m_command;
	m_altcommand = method + m_altcommand;

	// Execute command if found
	if (FindCommand(m_command))
	{
		if(!HandleCommand(m_command, m_params, result))
		{
			return false;
		}
	} // Command not found, try the alternative one
	else if (FindCommand(m_altcommand))
	{
		m_params = "uriparam=" + m_altparams + "&" + m_params;
		if(!HandleCommand(m_altcommand, m_params, result))
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	if (!result.empty())
	{
		root["result"] = result;
	}

	root["uri"] = m_uri;
	root["path"] = m_path;
	root["params"] = m_params;
	root["command"] = m_command;
	root["altparams"] = m_altparams;
	root["altcommand"] = m_altcommand;

	return true;
}

void CWebServerOpenAPI::Init()
{
	m_httpcode = 0;
	m_uri = "";
	m_path = "";
	m_params = "";
	m_command = "";
	m_altparams = "";
	m_altcommand = "";
	m_resultjson.clear();
}

bool CWebServerOpenAPI::ParseURI(const std::string uri)
{
	std::string path = uri;
	std::string params;
	size_t paramPos = uri.find_first_of('?');

	if (!(uri.find("/api/") == 0))
	{
		return false;
	}
	if (paramPos != std::string::npos)
	{
		path = uri.substr(0, paramPos);
		params = uri.substr(paramPos + 1);
	}

	m_uri = uri;
	m_path = path.substr(5); 		// And strip the first 5 chars
	m_params = params;

	// Build command
	std::string tmp = m_path;
	while(tmp.find_first_of("/") != std::string::npos)
	{
		tmp.replace(tmp.find_first_of("/"),1,"");
	}
	m_command = tmp;

	// Build command alternative
	tmp = m_path;
	if(tmp.find_last_of("/") != std::string::npos)
	{
		tmp.replace(tmp.find_last_of("/"),1,"#");
		while(tmp.find_first_of("/") != std::string::npos)
		{
			tmp.replace(tmp.find_first_of("/"),1,"");
		}
		paramPos = tmp.find_first_of("#");
		if (paramPos != std::string::npos)
		{
			m_altcommand = tmp.substr(0, paramPos);
			m_altparams = tmp.substr(paramPos + 1);
		}
	}

	return true;
}

void CWebServerOpenAPI::RegisterCommand(const char* command, openapi_command_function CommandFunction)
{
	m_openapicommands.insert(std::pair<std::string, openapi_command_function >(std::string(command), CommandFunction));
}

bool CWebServerOpenAPI::FindCommand(const std::string& command)
{
	std::map < std::string, openapi_command_function >::iterator pf = m_openapicommands.find(command);
	return (pf != m_openapicommands.end() ? true : false);
}

bool CWebServerOpenAPI::HandleCommand(const std::string& command, const std::string& params, Json::Value& result)
{
	std::map < std::string, openapi_command_function >::iterator pf = m_openapicommands.find(command);
	if (pf != m_openapicommands.end())
	{
		pf->second(params, result);
		return true;
	}
	return false;
}

bool CWebServerOpenAPI::GetCustomData(const std::string& params, Json::Value& result)
{
	bool bSuccess = false;

	result["data"] = TESTDEFINE;
	result["params"] = params;
	bSuccess = true;

	return bSuccess;
}
