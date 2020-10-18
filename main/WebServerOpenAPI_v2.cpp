/************************************************************************

Implementation of WebServerOpenAPI class
Author: KidDigital (github.com/kiddigital)

15/10/2020 1.0 Creation

License: Public domain

************************************************************************/
#include "stdafx.h"
#include "WebServerOpenAPI_v2.h"
#include "../../main/Logger.h"
#include "../../main/json_helper.h"
#include <sstream>
#include <iomanip>
#include <boost/bind.hpp>

#define TESTDEFINE "testing123"

CWebServerOpenAPI_v2::CWebServerOpenAPI_v2()
{
	gInit();

	// List of the commands available to register with their command name
	// NOTE: make sure the request method (GET, POST, DELETE, etc) is written in CAPS and the rest in lowercase!
	gRegisterCommand("GETcustomdata", boost::bind(&CWebServerOpenAPI_v2::GetCustomData, this, _1, _2));
	gRegisterCommand("POSTcustomdata", boost::bind(&CWebServerOpenAPI_v2::PostCustomData, this, _1, _2));
}

CWebServerOpenAPI_v2::~CWebServerOpenAPI_v2()
{
}

/* *********************
 * The generic function to handle incoming requests
 **********************/

bool CWebServerOpenAPI_v2::gHandleRequest(const std::string method, const std::string uri, std::multimap<std::__cxx11::string, std::__cxx11::string> parameters, Json::Value& root)
{
	Json::Value result;

	_log.Debug(DEBUG_WEBSERVER, "WebServerOpenAPI: Handling request (%s) %s", method.c_str(), uri.c_str());

	gInit();

	if (!gParseURI(uri))
		return false;

	// Add method to command
	m_command = method + m_command;
	m_altcommand = method + m_altcommand;

	// show content:
	for (std::multimap<std::__cxx11::string,std::__cxx11::string>::iterator it=parameters.begin(); it!=parameters.end(); ++it)
		_log.Debug(DEBUG_NORM, "Debugging parameters %s => %s", (*it).first.c_str(), (*it).second.c_str());
	
	// Execute command if found
	if (gFindCommand(m_command))
	{
		if(!gHandleCommand(m_command, m_params, result))
		{
			return false;
		}
	} // Command not found, try the alternative one
	else if (gFindCommand(m_altcommand))
	{
		m_params = "uriparam=" + m_altparams + "&" + m_params;
		if(!gHandleCommand(m_altcommand, m_params, result))
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

void CWebServerOpenAPI_v2::gInit()
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

bool CWebServerOpenAPI_v2::gParseURI(const std::string uri)
{
	std::string path = uri;
	std::string params;
	size_t paramPos = uri.find_first_of('?');

	if (!(uri.find("/api/v2/") == 0))
	{
		return false;
	}
	if (paramPos != std::string::npos)
	{
		path = uri.substr(0, paramPos);
		params = uri.substr(paramPos + 1);
	}

	m_uri = uri;
	m_path = path.substr(8); 		// And strip the first 8 chars (/api/v2/)
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

void CWebServerOpenAPI_v2::gRegisterCommand(const char* command, openapi_command_function CommandFunction)
{
	m_openapicommands.insert(std::pair<std::string, openapi_command_function >(std::string(command), CommandFunction));
}

bool CWebServerOpenAPI_v2::gFindCommand(const std::string& command)
{
	std::map < std::string, openapi_command_function >::iterator pf = m_openapicommands.find(command);
	return (pf != m_openapicommands.end() ? true : false);
}

bool CWebServerOpenAPI_v2::gHandleCommand(const std::string& command, const std::string& params, Json::Value& result)
{
	std::map < std::string, openapi_command_function >::iterator pf = m_openapicommands.find(command);
	if (pf != m_openapicommands.end())
	{
		pf->second(params, result);
		return true;
	}
	return false;
}

/* *********************
 * Below here the implementations of the specific methods supported with Domoticz 2nd version of the API service
 **********************/

bool CWebServerOpenAPI_v2::GetCustomData(const std::string& params, Json::Value& result)
{
	bool bSuccess = false;

	result["data"] = TESTDEFINE;
	result["params"] = params;
	bSuccess = true;

	return bSuccess;
}

bool CWebServerOpenAPI_v2::PostCustomData(const std::string& params, Json::Value& result)
{
	bool bSuccess = false;

	result["post"] = TESTDEFINE;
	result["params"] = params;
	bSuccess = true;

	return bSuccess;
}
