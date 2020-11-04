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
#include "../webserver/reply.hpp"
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
	gRegisterCommand("GETdevice", boost::bind(&CWebServerOpenAPI_v2::GetDevice, this, _1, _2));
	gRegisterCommand("GETweatherforecastdata", boost::bind(&CWebServerOpenAPI_v2::GetWeatherForecastdata, this, _1, _2));
}

CWebServerOpenAPI_v2::~CWebServerOpenAPI_v2()
{
}

/* *********************
 * The generic (helper) functions to handle incoming requests
 **********************/

bool CWebServerOpenAPI_v2::gHandleRequest(const std::string method, const std::string uri, std::multimap<std::__cxx11::string, std::__cxx11::string> parameters, Json::Value& root)
{
	Json::Value result;
	Json::Value input;
	bool bAltExec = false;

	_log.Debug(DEBUG_WEBSERVER, "WebServerOpenAPI: Handling request (%s) %s", method.c_str(), uri.c_str());

	gInit();

	if (!gParseURI(uri))
		return false;

	// Add method to command
	m_command = method + m_command;
	m_altcommand = method + m_altcommand;

	// Get all parameters, noth query string AND (POST, PUT, etc.) body
	for (std::multimap<std::__cxx11::string,std::__cxx11::string>::iterator it=parameters.begin(); it!=parameters.end(); ++it)
	{
		_log.Debug(DEBUG_WEBSERVER, "Debugging parameters %s => %s", (*it).first.c_str(), (*it).second.c_str());
		input[(*it).first] = (*it).second;
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
		/*
		root["command"] = m_command;
		if (bAltExec)
			root["command"] = m_altcommand;
		root["result"] = result;
		*/
		root = result;
	}

	m_resultjson = root;

	return true;
}

http::server::reply::status_type CWebServerOpenAPI_v2::gGetResultCode()
{
	return m_httpresult.status;
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

bool CWebServerOpenAPI_v2::gHandleCommand(const std::string& command, const Json::Value& input, Json::Value& result)
{
	std::map < std::string, openapi_command_function >::iterator pf = m_openapicommands.find(command);
	if (pf != m_openapicommands.end())
	{
		pf->second(input, result);
		return true;
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

void CWebServerOpenAPI_v2::GetWeatherForecastdata(const Json::Value& input, Json::Value& result)
{
	result["function"] = "GetWeatherForecastdata";
	result["params"] = input;

	std::string Latitude = "1";
	std::string Longitude = "1";
	std::string sValue;
	if (m_sql.GetPreferencesVar("Location", sValue))
	{
		std::vector<std::string> strarray;
		StringSplit(sValue, ";", strarray);

		if (strarray.size() == 2)
		{
			Latitude = strarray[0];
			Longitude = strarray[1];
		}
	}
	result["Latitude"] = Latitude;
	result["Longitude"] = Longitude;

}
