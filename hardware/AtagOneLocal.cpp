#include "stdafx.h"
#include "AtagOneLocal.h"
#include "../main/Helper.h"
#include "hardwaretypes.h"
#include "../main/Logger.h"
#include "../main/WebServerHelper.h"
#include "../main/RFXtrx.h"
#include "../main/SQLHelper.h"
#include "../httpclient/HTTPClient.h"
#include "../main/mainworker.h"
#include "../main/json_helper.h"

extern http::server::CWebServerHelper m_webservers;

//Inspidred by https://github.com/kozmoz/atag-one-api

#define ATAGONE_MAC_ADDRESS "00-00-36-66-84-29" //Dummy MAC address for Atag One (numpad for domoticz = 36668429)
#define ATAGONE_MAX_LOGIN_ATTEMPTS 10
#define ATAGONE_POLL_INTERVAL 60

#define ATAGONE_TEMPERATURE_MIN 4
#define ATAGONE_TEMPERATURE_MAX 27

#define ATAGONE_ACC_STATUS_NA 0
#define ATAGONE_ACC_STATUS_PENDING 1
#define ATAGONE_ACC_STATUS_OK 2
#define ATAGONE_ACC_STATUS_DENIED 3

CAtagOneLocal::CAtagOneLocal(const int ID, const int Mode1, const int Mode2, const int Mode3, const int Mode4, const int Mode5, const int Mode6)
{
	m_HwdID=ID;
	m_bFoundThermostat = false;
	m_bLoggedIn = false;
	m_seqNr = 0;
	m_iLoginAttempts = 0;
	m_IPaddress = "";
	m_DeviceID = "";

	m_LastMinute=-1;
	m_MacAddress = ATAGONE_MAC_ADDRESS;	// To-do: fecth actual MAC address from network using helper function (needs both Windows and Linux implementation)
}

void CAtagOneLocal::Init()
{
	auto result = m_sql.safe_query("SELECT Address, Extra FROM Hardware WHERE (ID==%d)", m_HwdID);
	if (!result.empty() && !result[0][0].empty() && !result[0][1].empty())
	{
		m_IPaddress = result[0][0];
		m_DeviceID = result[0][1];
		m_bFoundThermostat = true;
		m_bLoggedIn = true;
		Debug(DEBUG_HARDWARE, "Found previously detected thermostat in database! (IP: %s, DeviceID: %s)", m_IPaddress.c_str(), m_DeviceID.c_str());
		return;
	}

	Debug(DEBUG_HARDWARE, "No previously detected thermostat found in database...initiate search...");
}

bool CAtagOneLocal::StartHardware()
{
	RequestStart();

	Init();

	m_LastMinute = -1;
	//Start worker thread
	m_thread = std::make_shared<std::thread>([this] { Do_Work(); });
	SetThreadNameInt(m_thread->native_handle());
	m_bIsStarted=true;
	sOnConnected(this);
	return (m_thread != nullptr);
}

bool CAtagOneLocal::StopHardware()
{
	if (m_thread)
	{
		RequestStop();
		m_thread->join();
		m_thread.reset();
	}
    m_bIsStarted=false;
    return true;
}

void CAtagOneLocal::Do_Work()
{
	Log(LOG_STATUS, "Worker started...");
	int sec_counter = ATAGONE_POLL_INTERVAL - 3;
	bool bKeepRunning = true;

	while (bKeepRunning && !IsStopRequested(1000))
	{
		sec_counter++;
		if (sec_counter % 12 == 0) {
			m_LastHeartbeat = mytime(nullptr);
		}

		if (!m_bFoundThermostat)
		{
			if (FindThermostat())
			{
				//Store in database
				m_sql.safe_query("UPDATE Hardware SET Address='%s', Extra='%s' WHERE (ID==%d)", m_IPaddress.c_str(), m_DeviceID.c_str(), m_HwdID);
				Log(LOG_STATUS, "Found Atag One thermostat at IP %s with DeviceID %s", m_IPaddress.c_str(), m_DeviceID.c_str());
				m_bFoundThermostat = true;
			}
			else
			{
				Log(LOG_ERROR, "Could not find Atag One thermostat in the network. Exiting!");
				bKeepRunning = false;
			}
		}
		else if (!m_bLoggedIn)
		{
			if (sec_counter % 5 == 0 && (m_iLoginAttempts < ATAGONE_MAX_LOGIN_ATTEMPTS))
			{
				Log(LOG_STATUS, "Attempting to login to Thermostat at %s (attempt %d of %d)", m_IPaddress.c_str(), m_iLoginAttempts + 1, ATAGONE_MAX_LOGIN_ATTEMPTS);
				if (LoginThermostat())
				{
					Log(LOG_STATUS, "Succesfull login to Thermostat at %s", m_IPaddress.c_str());
					m_bLoggedIn = true;
					sec_counter = ATAGONE_POLL_INTERVAL - 3; // reset counter to minimize delay before first data retrieval
				}
				else
				{
					Log(LOG_ERROR, "Unsuccessful login to Thermostat at %s", m_IPaddress.c_str());
					m_iLoginAttempts++;
					if (m_iLoginAttempts >= ATAGONE_MAX_LOGIN_ATTEMPTS)
					{
						Log(LOG_ERROR, "Failed to login to Thermostat at %s after %d attempts. Exiting!", m_IPaddress.c_str(), ATAGONE_MAX_LOGIN_ATTEMPTS);
						bKeepRunning = false;
					}
				}
			}
		}
		else if (sec_counter % ATAGONE_POLL_INTERVAL == 0)
		{
			//SendOutsideTemperature();
			if (!GetDeviceDetails())
			{
				Log(LOG_ERROR, "Error retrieving data from Atag One thermostat at %s", m_IPaddress.c_str());
			}
		}
	}
	Log(LOG_STATUS, "Worker stopped...");
}

bool CAtagOneLocal::GetDeviceDetails()
{
    std::string sResult;
    std::string sURL;
    std::vector<std::string> ExtraHeaders;
    std::vector<std::string> ResponseHeaders;

	// Bitmask values for retrieve_message "info" field:
	// 1   = Control (control data: setpoint, mode, etc)
	// 2   = Schedules
	// 4   = Configuration
	// 8   = Report (general status, temperatures, pressures)
	// 16  = Status
	// 32  = WiFi Scan
	// 64  = Report Details

    // Retrieve data from the thermostat using HTTP POST at port 10000
    sURL = "http://" + m_IPaddress + ":10000/retrieve";
    
    // Prepare the POST body - request all available data points
	/*
    Json::Value jPostData;
	jPostData["retrieve_message"] = Json::objectValue;
	jPostData["retrieve_message"]["seqnr"] = ++m_seqNr;
	jPostData["retrieve_message"]["device_id"] = m_DeviceID;
	jPostData["retrieve_message"]["info"] = 29;		// requesting bitmask (Control (1), Configuration (4), Report (8), Status (16))
    
	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";     // no pretty-print whitespace
	builder["emitUTF8"] = true;       // optional, keeps UTF-8

	const std::string sPostData = Json::writeString(builder, jPostData);
	*/
	const std::string sPostData = 
	"{\"retrieve_message\": {"
    "\"seqnr\": " + std::to_string(m_seqNr) + ","
    "\"account_auth\": {"
      "\"user_account\": \"\","
      "\"mac_address\": \"" + m_MacAddress + "\""
    "},"
    "\"info\": 29,"
	"}}";

	Debug(DEBUG_HARDWARE, "POST data to thermostat: %s", sPostData.c_str());

    ExtraHeaders.push_back("Content-Type: application/x-www-form-urlencoded");
    
    std::string sHTTPReturn;
    if (!HTTPClient::POST(sURL, sPostData, ExtraHeaders, sHTTPReturn, ResponseHeaders))
    {
        Log(LOG_ERROR, "Error connecting to thermostat at %s", m_IPaddress.c_str());
        return false;
    }

    // Parse the response
    Json::Value root;
    bool ret = ParseJSon(sHTTPReturn, root);
    if (!ret || !root.isObject())
    {
        Log(LOG_ERROR, "Invalid data received from thermostat!");
		Debug(DEBUG_HARDWARE, "Received inavalid data: .%s.", sHTTPReturn.c_str());
        return false;
    }

    // Check if we have valid data structure
    if (!root.isMember("retrieve_reply") || !root["retrieve_reply"].isObject())
    {
        Log(LOG_ERROR, "Invalid response format from thermostat!");
		Debug(DEBUG_HARDWARE, "Received invalid response format: .%s.", root.toStyledString().c_str());
        return false;
    }

    Json::Value retrieveData = root["retrieve_reply"];

	Debug(DEBUG_HARDWARE, "Reply: %s", retrieveData.toStyledString().c_str());

	if (retrieveData.isMember("seqnr") && retrieveData["seqnr"].isInt() && (retrieveData["seqnr"].asUInt64() != m_seqNr))
	{
		Log(LOG_ERROR, "Sequence number mismatch in thermostat response!");
		Debug(DEBUG_HARDWARE, "Expected seqnr: %lu, Received seqnr: %lu", m_seqNr, retrieveData["seqnr"].asUInt64());
		return false;
	}

	if (retrieveData.isMember("acc_status") && retrieveData["acc_status"].isInt() && (retrieveData["acc_status"].asUInt64() != ATAGONE_ACC_STATUS_OK))
	{
		Log(LOG_ERROR, "Thermostat returned not ok access status %lu!", retrieveData["acc_status"].asUInt64());
		m_bLoggedIn = false;
		return false;
	}
	m_seqNr++; // Increment expected sequence number for next request

    if (retrieveData.isMember("report") && retrieveData["report"].isObject())
    {
        Debug(DEBUG_HARDWARE, "Report data received (%s)", retrieveData["report"].toStyledString().c_str());
    }

    if (retrieveData.isMember("control") && retrieveData["control"].isObject())
    {
        Debug(DEBUG_HARDWARE, "Control data received (%s)", retrieveData["control"].toStyledString().c_str());
    }

    if (retrieveData.isMember("configuration") && retrieveData["configuration"].isObject())
    {
        Debug(DEBUG_HARDWARE, "Configuration data received (%s)", retrieveData["configuration"].toStyledString().c_str());
    }

    // Store the complete root object for use by caller
    // The caller can access individual values like:
    // - retrieveData["report"]["room_temp"] (current temperature)
    // - retrieveData["control"]["ch_mode"] (central heating mode)
    // - retrieveData["control"]["temp_set"] (setpoint temperature)
    // etc.

    return true;
}

/*
bool CAtagOneLocal::GetDeviceDetails(const std::string& IPaddress)
{
	std::string sResult;

	std::string sURL;
	std::vector<std::string> ExtraHeaders;
	std::vector<std::string> ResponseHeaders;

	Json::Value root;


	sURL = ATAGONE_URL_DIAGNOSTICS;
	stdreplace(sURL, "{0}", CURLEncode::URLEncode(ThermostatID));
	if (!HTTPClient::GET(sURL, sResult))
	{
		Log(LOG_ERROR, "Error getting thermostat data!");
		m_bDoLogin = true;
		return false;
	}

	sURL = ATAGONE_URL_LATEST_REPORT;
	stdreplace(sURL, "{0}", CURLEncode::URLEncode(ThermostatID));

	if (!HTTPClient::GET(sURL, ExtraHeaders, sResult, ResponseHeaders))
	{
		Log(LOG_ERROR, "Error getting thermostat data! (%s)", ResponseHeaders[0].c_str());
		m_bDoLogin = true;
		return false;
	}

	//Extract all values from the HTML page, and put them in a json array
	Json::Value root;
	std::string sret;
	sret = GetHTMLPageValue(sResult, "Kamertemperatuur|Room temperature|Raumtemperatur", true);
	if (sret.empty())
	{
		Log(LOG_ERROR, "Invalid/no data received (1)...");
		return false;
	}
	root["roomTemperature"] = static_cast<float>(atof(sret.c_str()));
	//root["deviceAlias"] = GetHTMLPageValue(sResult, "Apparaat alias|Device alias", false);
	//root["latestReportTime"] = GetHTMLPageValue(sResult, "Laatste rapportagetijd|Latest report time", false);
	//root["connectedTo"] = GetHTMLPageValue(sResult, "Verbonden met|Connected to", false);
	root["burningHours"] = static_cast<float>(atof(GetHTMLPageValue(sResult, "Branduren|Burning hours", true).c_str()));
	//root["boilerHeatingFor"] = GetHTMLPageValue(sResult, "Ketel in bedrijf voor|Boiler heating for", false);
	sret = GetHTMLPageValue(sResult, "Brander status|Flame status|Brennerstatus", false);
	root["flameStatus"] = ((sret == "Aan") || (sret == "On") || (sret == "An")) ? true : false;
	root["outsideTemperature"] = static_cast<float>(atof(GetHTMLPageValue(sResult, "Buitentemperatuur|Outside temperature|Au&#223;entemperatur", true).c_str()));
	root["dhwSetpoint"] = static_cast<float>(atof(GetHTMLPageValue(sResult, "Setpoint warmwater|DHW setpoint", true).c_str()));
	root["dhwWaterTemperature"] = static_cast<float>(atof(GetHTMLPageValue(sResult, "Warmwatertemperatuur|DHW water temperature|Warmwassertemperatur", true).c_str()));
	root["chSetpoint"] = static_cast<float>(atof(GetHTMLPageValue(sResult, "Setpoint cv|CH setpoint", true).c_str()));
	root["chWaterTemperature"] = static_cast<float>(atof(GetHTMLPageValue(sResult, "CV-aanvoertemperatuur|CH water temperature", true).c_str()));
	root["chWaterPressure"] = static_cast<float>(atof(GetHTMLPageValue(sResult, "CV-waterdruk|CH water pressure|Anlagendruck", true).c_str()));
	root["chReturnTemperature"] = static_cast<float>(atof(GetHTMLPageValue(sResult, "CV retourtemperatuur|CH return temperature|HZ R&#252;cklauftemperatur", true).c_str()));

	// We have to do an extra call to get the target temperature.
	sURL = ATAGONE_URL_UPDATE_DEVICE_CONTROL;
	stdreplace(sURL, "{0}", CURLEncode::URLEncode(ThermostatID));
	if (!HTTPClient::GET(sURL, sResult))
	{
		Log(LOG_ERROR, "Error getting target setpoint data!");
		m_bDoLogin = true;
		return false;
	}

	Json::Value root2;
	bool ret = ParseJSon(sResult, root2);
	if ((!ret) || (!root2.isObject()))
	{
		Log(LOG_ERROR, "Invalid/no data received (2)...");
		return false;
	}
	if (root2["targetTemp"].empty())
	{
		Log(LOG_ERROR, "Invalid/no data received (3)...");
		return false;
	}
	root["targetTemperature"] = static_cast<float>(atof(root2["targetTemp"].asString().c_str()));
	root["currentMode"] = root2["currentMode"].asString();
	root["vacationPlanned"] = root2["vacationPlanned"].asBool();

	//Handle the Values
	float temperature;
	temperature = (float)root["targetTemperature"].asFloat();
	SendSetPointSensor(0, 0, 0, 1, 1, 255, temperature, "Room Setpoint");

	temperature = (float)root["roomTemperature"].asFloat();
	SendTempSensor(2, 255, temperature, "room Temperature");

	if (!root["outsideTemperature"].empty())
	{
		temperature = (float)root["outsideTemperature"].asFloat();
		SendTempSensor(3, 255, temperature, "outside Temperature");
	}

	//DHW
	if (!root["dhwSetpoint"].empty())
	{
		temperature = (float)root["dhwSetpoint"].asFloat();
		SendSetPointSensor(0, 0, 0, 2, 1, 255, temperature, "DHW Setpoint");
	}
	if (!root["dhwWaterTemperature"].empty())
	{
		temperature = (float)root["dhwWaterTemperature"].asFloat();
		SendTempSensor(4, 255, temperature, "DHW Temperature");
	}
	//CH
	if (!root["chSetpoint"].empty())
	{
		temperature = (float)root["chSetpoint"].asFloat();
		SendSetPointSensor(0, 0, 0, 3, 1, 255, temperature, "CH Setpoint");
	}
	if (!root["chWaterTemperature"].empty())
	{
		temperature = (float)root["chWaterTemperature"].asFloat();
		SendTempSensor(5, 255, temperature, "CH Temperature");
	}
	if (!root["chWaterPressure"].empty())
	{
		float pressure = (float)root["chWaterPressure"].asFloat();
		SendPressureSensor(1, 1, 255, pressure, "Pressure");
	}
	if (!root["chReturnTemperature"].empty())
	{
		temperature = (float)root["chReturnTemperature"].asFloat();
		SendTempSensor(6, 255, temperature, "CH Return Temperature");
	}

	if (!root["currentMode"].empty())
	{
		std::string actSource = root["currentMode"].asString();
		bool bIsScheduleMode = (actSource == "schedule_active");
		SendSwitch(1, 1, 255, bIsScheduleMode, 0, "Thermostat Schedule Mode", m_Name);
	}
	if (!root["flameStatus"].empty())
	{
		SendSwitch(2, 1, 255, root["flameStatus"].asBool(), 0, "Flame Status", m_Name);
	}
	return true;
}
*/

bool CAtagOneLocal::LoginThermostat()
{
	std::string sResult;
	std::string sURL;
	std::string sDeviceName = m_Name + " atag-one API";
	std::vector<std::string> ExtraHeaders;
	std::vector<std::string> ResponseHeaders;

	// Login to the thermostat using HTTP POST at port 10000
	sURL = "http://" + m_IPaddress + ":10000/pair_message";
	
	// Prepare the POST body with device ID, username and password
	/*
	Json::Value jPostData;
	jPostData["pair_message"] = Json::objectValue;
	jPostData["pair_message"]["account_auth"] = Json::objectValue;
	jPostData["pair_message"]["account_auth"]["user_account"] = "";		// We leave it empty for local login
	jPostData["pair_message"]["account_auth"]["mac_address"] = m_MacAddress;
	jPostData["pair_message"]["accounts"] = Json::objectValue;
	jPostData["pair_message"]["accounts"]["entries"] = Json::arrayValue;
	Json::Value jPostEntry = Json::objectValue;
	jPostEntry["user_account"] = ""; // We leave it empty for local login
	jPostEntry["mac_address"] = sMacAddress;
	jPostEntry["device_name"] = sDeviceName;
	jPostEntry["account_type"] = 0;	//
	jPostData["pair_message"]["accounts"]["entries"].append(jPostEntry);
	jPostData["pair_message"]["seqnr"] = 0;
	
	Json::StreamWriterBuilder jswBuilder;
	jswBuilder["indentation"] = "";     // no pretty-print whitespace
	jswBuilder["emitUTF8"] = true;       // optional, keeps UTF-8
	//jswBuilder["enableYAMLCompatibility"] = true; // optional, stable ordering in some builds

	const std::string sPostData = Json::writeString(jswBuilder, jPostData);
	*/
	// We build the JSON string manually to ensure order of elements as expected by the thermostat
	const std::string sPostData =
    "{\"pair_message\":{"
    "\"seqnr\":0,"
    "\"account_auth\":{"
        "\"user_account\":\"\","
        "\"mac_address\":\"" + m_MacAddress + "\""
    "},"
    "\"accounts\":{"
        "\"entries\":[{"
            "\"user_account\":\"\","
            "\"mac_address\":\"" + m_MacAddress + "\","
            "\"device_name\":\"" + sDeviceName + "\","
            "\"account_type\":0"
        "}]"
    "}}}";

	Debug(DEBUG_HARDWARE, "POST data to thermostat: %s", sPostData.c_str());

	ExtraHeaders.push_back("Content-Type: application/x-www-form-urlencoded");
	//ExtraHeaders.push_back("User-Agent: Mozilla/5.0 (compatible; AtagOneAPI/1.0.0(2026-01-01); https://atag.one/)");
	//ExtraHeaders.push_back("X-OneApp-Version: 1.0.0(2026-01-01)");
	//ExtraHeaders.push_back("Accept-Charset: UTF-8");
	//ExtraHeaders.push_back("Accept: */*");

	std::string sHTTPReturn;
	if (!HTTPClient::POST(sURL, sPostData, ExtraHeaders, sHTTPReturn, ResponseHeaders))
	{
		Log(LOG_ERROR, "Error connecting to thermostat at %s!", m_IPaddress.c_str());
		return false;
	}

	// Parse the response
    Json::Value root;
    bool ret = ParseJSon(sHTTPReturn, root);
    if (!ret || !root.isObject())
    {
        Log(LOG_ERROR, "Invalid data received from thermostat!");
        return false;
    }

    // Check if we have valid data structure
    if (!root.isMember("pair_reply") || !root["pair_reply"].isObject())
    {
        Log(LOG_ERROR, "Invalid response format from thermostat (%s)!", root.toStyledString().c_str());
        return false;
    }
    Json::Value retrieveData = root["pair_reply"];

	Debug(DEBUG_HARDWARE, "Reply: %s", retrieveData.toStyledString().c_str());

	if (retrieveData.isMember("acc_status") && retrieveData["acc_status"].isInt() && (retrieveData["acc_status"].asUInt64() != ATAGONE_ACC_STATUS_OK))
	{
		Log(LOG_ERROR, "Thermostat returned access status %lu!", retrieveData["acc_status"].asUInt64());
		return false;
	}
	return true;
}

bool CAtagOneLocal::FindThermostat()
{
	m_DeviceID = "";
	// Search for the thermostat on the given IP address
	// To-Do: implement actual discovery mechanism by listening for UDP message on port 11000
	// For now, we just assume the thermostat is at a fixed IP address
	m_IPaddress = "172.16.0.253";
	m_DeviceID = "6808-1500-1808_17-41-001-295";
	Debug(DEBUG_HARDWARE, "Thermostat %sfound %s", (m_DeviceID.empty() ? "not " : ""), (m_DeviceID.empty() ? "" : "(" + m_DeviceID + ")").c_str());

	return (!m_DeviceID.empty());
}

bool CAtagOneLocal::WriteToHardware(const char *pdata, const unsigned char /*length*/)
{
	const tRBUF *pCmd = reinterpret_cast<const tRBUF *>(pdata);
	if (pCmd->LIGHTING2.packettype == pTypeLighting2)
	{
		//Light command

		int node_id = pCmd->LIGHTING2.id4;
		bool bIsOn = (pCmd->LIGHTING2.cmnd == light2_sOn);
		if (node_id == 1)
		{
			//Pause Switch
			//SetPauseStatus(bIsOn);
			return true;
		}
	}
	return false;
}

void CAtagOneLocal::SetSetpoint(const int idx, const float temp)
{
	if (idx != 1)
	{
		Log(LOG_ERROR, "Currently only Room Temperature Setpoint allowed!");
		return;
	}

	int rtemp = int(temp * 2.0F);
	float dtemp = float(rtemp) / 2.0F;
	if (
		(dtemp<ATAGONE_TEMPERATURE_MIN) ||
		(dtemp>ATAGONE_TEMPERATURE_MAX)
		)
	{
		Log(LOG_ERROR, "Temperature should be between %d and %d!", ATAGONE_TEMPERATURE_MIN, ATAGONE_TEMPERATURE_MAX);
		return;
	}
	char szTemp[20];
	sprintf(szTemp, "%.1f", dtemp);
	std::string sTemp = szTemp;

	SendSetPointSensor(0, 0, 0, (const uint8_t)idx, 1, 255, dtemp, "");
}

bool CAtagOneLocal::SetCentralHeatingSetpoint(float temperature)
{
    if (temperature < ATAGONE_TEMPERATURE_MIN || temperature > ATAGONE_TEMPERATURE_MAX)
    {
        Log(LOG_ERROR, "Temperature out of range [%d, %d]", ATAGONE_TEMPERATURE_MIN, ATAGONE_TEMPERATURE_MAX);
        return false;
    }

    std::string sURL = "http://" + m_IPaddress + ":10000/update";
    
    // Round to half degree (0.5 precision)
    float rounded = std::round(temperature * 2.0f) / 2.0f;
    
    // Build compact JSON payload
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << rounded;
    
    std::string sPostData = 
        "{\"update_message\":{"
        "\"seqnr\":0,"
        "\"account_auth\":{"
            "\"user_account\":\"\","
            "\"mac_address\":\"" + m_MacAddress + "\""
        "},"
        "\"control\":{"
            "\"ch_mode_temp\":" + oss.str() +
        "}"
        "}}";

    std::vector<std::string> ExtraHeaders;
    ExtraHeaders.push_back("Content-Type: application/x-www-form-urlencoded; UTF-8");
    ExtraHeaders.push_back("User-Agent: Mozilla/5.0 (compatible; AtagOneAPI/1.0.0; https://atag.one/)");
    ExtraHeaders.push_back("Accept-Charset: UTF-8");

    std::string sHTTPReturn;
    std::vector<std::string> ResponseHeaders;
    
    if (!HTTPClient::POST(sURL, sPostData, ExtraHeaders, sHTTPReturn, ResponseHeaders))
    {
        Log(LOG_ERROR, "Error sending setpoint to thermostat");
        return false;
    }

    // Parse response
    Json::Value root;
    if (!ParseJSon(sHTTPReturn, root) || !root.isMember("update_reply"))
    {
        Log(LOG_ERROR, "Invalid response from setpoint update");
        return false;
    }

    Json::Value reply = root["update_reply"];
    if (reply.isMember("acc_status") && reply["acc_status"].asInt() != ATAGONE_ACC_STATUS_OK)
    {
        Log(LOG_ERROR, "Thermostat rejected setpoint update (acc_status=%d)", reply["acc_status"].asInt());
        return false;
    }

    Log(LOG_STATUS, "Central heating setpoint set to %.1f°C", rounded);
    return true;
}
