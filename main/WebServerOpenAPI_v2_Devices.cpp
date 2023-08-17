/******************************************************************************

  All '/devices' calls of OpenAPI_v2 Webserver are here

******************************************************************************/

#include "WebServerOpenAPI_v2.h"

namespace http
{
	namespace server
	{

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
	else
	{
		m_httpresult.status = http::server::reply::bad_request;
	}
}

	} // namespace server
} // namespace http