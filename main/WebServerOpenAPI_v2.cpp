/************************************************************************

Implementation of WebServerOpenAPI class
Author: KidDigital (github.com/kiddigital)

15/10/2020 1.0 Creation

License: Public domain

************************************************************************/
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
//#include <iomanip>

//using namespace boost::placeholders;

#define TESTDEFINE "testing123"

extern std::string szStartupFolder;
extern std::string szUserDataFolder;
extern std::string szWWWFolder;

extern std::string szAppVersion;
extern int iAppRevision;
extern std::string szAppHash;
extern std::string szAppDate;
extern std::string szPyVersion;

extern bool g_bUseUpdater;

CWebServerOpenAPI_v2::CWebServerOpenAPI_v2()
{
	gInit();

	// List of the commands available to register with their command name
	// NOTE: make sure the request method (GET, POST, DELETE, etc) is written in CAPS and the rest in lowercase!
	gRegisterCommand("GETcustomdata", [this](auto&& input, auto&& result) { GetCustomData(input, result); }, http::server::URIGHTS_NONE);
	gRegisterCommand("POSTcustomdata", [this](auto&& input, auto&& result) { PostCustomData(input, result); }, http::server::URIGHTS_ADMIN);
	gRegisterCommand("GETdevice", [this](auto&& input, auto&& result) { GetDevice(input, result); }, http::server::URIGHTS_VIEWER);
	/* Services */
	gRegisterCommand("GETservicesstatus", [this](auto&& input, auto&& result) { GetServicesStatus(input, result); }, http::server::URIGHTS_NONE);
	gRegisterCommand("GETservicesversion", [this](auto&& input, auto&& result) { GetServicesVersion(input, result); }, http::server::URIGHTS_NONE);
}

CWebServerOpenAPI_v2::~CWebServerOpenAPI_v2()
{
}

/* *********************
 * The generic (helper) functions to handle incoming requests
 **********************/

bool CWebServerOpenAPI_v2::gHandleRequest(const std::string method, const std::string uri, std::multimap<std::string, std::string> parameters, const int8_t userrights)
{
	Json::Value result;
	Json::Value input;
	bool bAltExec = false;

	_log.Debug(DEBUG_WEBSERVER, "WebServerOpenAPI: Handling request (%s) %s (rights %d)", method.c_str(), uri.c_str(), userrights);

	gInit();

	if (!gParseURI(uri))
		return false;

	m_userrights = userrights;
	if (userrights < 0)
		m_userrights = (int8_t)http::server::URIGHTS_NONE;

	// Add method to command
	m_command = method + m_command;
	m_altcommand = method + m_altcommand;

	// Get all parameters, both query string AND (POST, PUT, etc.) body
	for (auto &parameter: parameters)
	{
		_log.Debug(DEBUG_WEBSERVER, "Debugging parameters %s => %s", parameter.first.c_str(), parameter.second.c_str());
		input[parameter.first] = parameter.second;
	}
	
	// Execute command if found
	if (gFindCommand(m_command))
	{
		if(!gHandleCommand(m_command, input, result))
		{
			return false;
		}
	} // Command not found, try the alternative one
	else if (gFindCommand(m_altcommand))
	{
		bAltExec = true;
		input["_uriparam"] = m_altparams;
		m_params = "_uriparam=" + m_altparams + "&" + m_params;
		if(!gHandleCommand(m_altcommand, input, result))
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
		m_resultjson = result;
	}

	return true;
}

http::server::reply::status_type CWebServerOpenAPI_v2::gGetResultCode()
{
	return m_httpresult.status;
}

Json::Value CWebServerOpenAPI_v2::gGetResultJSON()
{
	return m_resultjson;
}

void CWebServerOpenAPI_v2::gInit()
{
	m_httpresult.status = http::server::reply::not_found;
	m_uri = "";
	m_path = "";
	m_params = "";
	m_command = "";
	m_altparams = "";
	m_altcommand = "";
	m_resultjson.clear();
	m_userrights = http::server::URIGHTS_NONE;
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

void CWebServerOpenAPI_v2::gRegisterCommand(const char* command, openapi_command_function CommandFunction, const uint8_t rights)
{
	m_openapicommands.insert(std::pair<std::string, openapi_command_function >(std::string(command), CommandFunction));
	m_openapicommandrights.insert(std::pair<std::string, int8_t >(std::string(command), (int8_t)rights));
}

bool CWebServerOpenAPI_v2::gFindCommand(const std::string& command)
{
	std::map < std::string, openapi_command_function >::iterator pf = m_openapicommands.find(command);
	return (pf != m_openapicommands.end() ? true : false);
}

bool CWebServerOpenAPI_v2::gHandleCommand(const std::string& command, const Json::Value& input, Json::Value& result)
{
	if (gFindCommand(command))
	{
		std::map < std::string, int8_t >::iterator pr = m_openapicommandrights.find(command);
		if (pr != m_openapicommandrights.end())
		{
			if (m_userrights >= pr->second)
			{
				std::map < std::string, openapi_command_function >::iterator pf = m_openapicommands.find(command);
				m_httpresult.status = http::server::reply::ok;
				pf->second(input, result);
				if (m_httpresult.status == http::server::reply::ok && result.empty())
					m_httpresult.status = http::server::reply::no_content;
			}
			else
			{
				_log.Debug(DEBUG_AUTH ,"WebServerOpenAPI: Command %s requires rights %d, but user has only rights %d", command.c_str(), pr->second, m_userrights);
				m_httpresult.status = http::server::reply::forbidden;
			}
			return true;
		}
	}
	return false;
}

/* *********************
 * Below here the implementations of the specific methods supported with Domoticz 2nd version of the API service
 **********************/

void CWebServerOpenAPI_v2::GetCustomData(const Json::Value& input, Json::Value& result)
{
	result["data"] = TESTDEFINE;
	result["params"] = input;
}

void CWebServerOpenAPI_v2::PostCustomData(const Json::Value& input, Json::Value& result)
{
	result["post"] = TESTDEFINE;
	result["params"] = input;
}

void CWebServerOpenAPI_v2::GetDevice(const Json::Value& input, Json::Value& result)
{
	int iDeviceIDX;

	iDeviceIDX = atoi(input["_uriparam"].asString().c_str());
	if (iDeviceIDX > 0)
	{
		std::vector<std::vector<std::string> > qresult;
		qresult = m_sql.safe_query("SELECT ID, Name, HardwareID, DeviceID, Unit, Type, SubType, nValue, sValue, LastUpdate FROM DeviceStatus WHERE (ID=%d)", iDeviceIDX);
		if (!qresult.empty())
		{
			result["ID"] = std::strtoul(qresult[0].at(0).c_str(),nullptr,0);
			result["Name"] = qresult[0].at(1);
			result["HardwareID"] = std::strtoul(qresult[0].at(2).c_str(),nullptr,0);
			result["DeviceID"] = qresult[0].at(3);
			result["Unit"] = std::strtoul(qresult[0].at(4).c_str(),nullptr,0);
			result["Type"] = std::strtoul(qresult[0].at(5).c_str(),nullptr,0);
			result["SubType"] = std::strtoul(qresult[0].at(6).c_str(),nullptr,0);
			result["nValue"] = std::strtol(qresult[0].at(7).c_str(),nullptr,0);
			result["sValue"] = qresult[0].at(8);
			result["LastUpdate"] = qresult[0].at(9);
		}
		else
		{
			m_httpresult.status = http::server::reply::not_found;
		}
	}
}

// Here we include all methods for all Services calls
#include "WebServerOpenAPI_v2_Services.cpp"
