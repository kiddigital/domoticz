#include "stdafx.h"
#include "AtagOneLocal.h"
#include "../main/Helper.h"
#include "hardwaretypes.h"
#include "../main/Logger.h"
#include "../main/WebServerHelper.h"
#include "../main/RFXtrx.h"
#include "../main/SQLHelper.h"
#include "../httpclient/HTTPClient.h"
#include "../httpclient/sock_port.h"
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
	const std::string sPostData = 
	"{\"retrieve_message\": {"
    "\"seqnr\": " + std::to_string(m_seqNr) + ","
    "\"account_auth\": {"
      "\"user_account\": \"\","
      "\"mac_address\": \"" + m_MacAddress + "\""
    "},"
    "\"info\": 93,"
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

    if (!(retrieveData.isMember("report") && retrieveData["report"].isObject()))
    {
        Debug(DEBUG_HARDWARE, "Report data missing!");
		return false;
    }

	if (!(retrieveData["report"].isMember("details") && retrieveData["report"]["details"].isObject()))
    {
        Debug(DEBUG_HARDWARE, "Report details data missing!");
		return false;
    }

    if (!(retrieveData.isMember("control") && retrieveData["control"].isObject()))
	{
		Debug(DEBUG_HARDWARE, "Control data missing!");
		return false;
    }

    if (!(retrieveData.isMember("configuration") && retrieveData["configuration"].isObject()))
    {
        Debug(DEBUG_HARDWARE, "Configuration data missing!");
		return false;
    }

    return ProcessDeviceDetails(retrieveData);
}

bool CAtagOneLocal::ProcessDeviceDetails(const Json::Value &retrievedData)
{
	//Handle the Values
	float temperature;
	temperature = (float)retrievedData["report"]["details"]["target_temp"].asFloat();
	SendSetPointSensor(0, 0, 0, 1, 1, 255, temperature, "Room Setpoint");

	temperature = (float)retrievedData["report"]["room_temp"].asFloat();
	SendTempSensor(2, 255, temperature, "Room Temperature");

	if (!retrievedData["report"]["outside_temp"].empty())
	{
		temperature = (float)retrievedData["report"]["outside_temp"].asFloat();
		SendTempSensor(3, 255, temperature, "Outside Temperature");
	}

	//DHW
	if (!retrievedData["control"]["dhw_temp_setp"].empty())
	{
		temperature = (float)retrievedData["control"]["dhw_temp_setp"].asFloat();
		SendSetPointSensor(0, 0, 0, 2, 1, 255, temperature, "DHW Setpoint");
	}
	if (!retrievedData["report"]["dhw_water_temp"].empty())
	{
		temperature = (float)retrievedData["report"]["dhw_water_temp"].asFloat();
		SendTempSensor(4, 255, temperature, "DHW Temperature");
	}
	//CH
	if (!retrievedData["report"]["ch_setpoint"].empty())
	{
		temperature = (float)retrievedData["report"]["ch_setpoint"].asFloat();
		SendSetPointSensor(0, 0, 0, 3, 1, 255, temperature, "CH Setpoint");
	}
	if (!retrievedData["report"]["ch_water_temp"].empty())
	{
		temperature = (float)retrievedData["report"]["ch_water_temp"].asFloat();
		SendTempSensor(5, 255, temperature, "CH Temperature");
	}
	if (!retrievedData["report"]["ch_water_pres"].empty())
	{
		float pressure = (float)retrievedData["report"]["ch_water_pres"].asFloat();
		SendPressureSensor(1, 1, 255, pressure, "CH Water Pressure");
	}
	if (!retrievedData["report"]["ch_return_temp"].empty())
	{
		temperature = (float)retrievedData["report"]["ch_return_temp"].asFloat();
		SendTempSensor(6, 255, temperature, "CH Return Temperature");
	}
	if (!retrievedData["report"]["details"]["rel_mod_level"].empty())
	{
		float relModLevel = (float)retrievedData["report"]["details"]["rel_mod_level"].asFloat();
		SendPercentageSensor(1, 1, 255, relModLevel, "Relative Modulation Level");
	}
	/*
	if (!retrievedData["currentMode"].empty())
	{
		std::string actSource = retrievedData["currentMode"].asString();
		bool bIsScheduleMode = (actSource == "schedule_active");
		SendSwitch(1, 1, 255, bIsScheduleMode, 0, "Thermostat Schedule Mode", m_Name);
	}
	if (!retrievedData["flameStatus"].empty())
	{
		SendSwitch(2, 1, 255, retrievedData["flameStatus"].asBool(), 0, "Flame Status", m_Name);
	}
	*/	
	return true;
}

bool CAtagOneLocal::LoginThermostat()
{
	std::string sResult;
	std::string sURL;
	std::string sDeviceName = m_Name + " atag-one API";
	std::vector<std::string> ExtraHeaders;
	std::vector<std::string> ResponseHeaders;

	// Login to the thermostat using HTTP POST at port 10000
	sURL = "http://" + m_IPaddress + ":10000/pair_message";
	
	// Build the JSON string manually to ensure order of elements as expected by the thermostat
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
	// m_IPaddress = "172.16.0.253";
	// m_DeviceID = "6808-1500-1808_17-41-001-295";
	constexpr unsigned short atagDiscoveryPort = 11000;
	constexpr int discoveryTimeoutSeconds = 15;
	constexpr size_t discoveryMessageSize = 37;
	constexpr size_t deviceIdOffset = 4;
	constexpr size_t deviceIdLength = 33;

	m_IPaddress.clear();
	m_DeviceID.clear();

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET)
	{
		Log(LOG_ERROR, "Atag One: unable to create UDP discovery socket (error=%d)", SOCKET_ERRNO);
		return false;
	}

	int reuseAddr = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuseAddr), sizeof(reuseAddr)) == SOCKET_ERROR)
	{
		Debug(DEBUG_HARDWARE, "Atag One: failed to enable SO_REUSEADDR on discovery socket (error=%d)", SOCKET_ERRNO);
	}

	sockaddr_in listenAddr = {};
	listenAddr.sin_family = AF_INET;
	listenAddr.sin_port = htons(atagDiscoveryPort);
	listenAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock, reinterpret_cast<sockaddr*>(&listenAddr), sizeof(listenAddr)) == SOCKET_ERROR)
	{
		Log(LOG_ERROR, "Atag One: unable to bind UDP discovery socket to port %u (error=%d)", atagDiscoveryPort, SOCKET_ERRNO);
		closesocket(sock);
		return false;
	}

	time_t deadline = mytime(nullptr) + discoveryTimeoutSeconds;
	while (!IsStopRequested(0) && mytime(nullptr) < deadline)
	{
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(sock, &readSet);

		timeval timeout = {};
		timeout.tv_sec = 1;

		int selectResult = select(static_cast<int>(sock) + 1, &readSet, nullptr, nullptr, &timeout);
		if (selectResult == 0)
		{
			continue;
		}
		if (selectResult == SOCKET_ERROR)
		{
			Log(LOG_ERROR, "Atag One: discovery socket wait failed (error=%d)", SOCKET_ERRNO);
			break;
		}

		sockaddr_in senderAddr = {};
		socklen_t senderAddrLen = sizeof(senderAddr);
		char buffer[64];
		int received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr*>(&senderAddr), &senderAddrLen);
		if (received <= 0)
		{
			Debug(DEBUG_HARDWARE, "Atag One: empty UDP discovery packet received");
			continue;
		}

		buffer[received] = '\0';
		if ((static_cast<size_t>(received) < discoveryMessageSize) || (strncmp(buffer, "ONE ", deviceIdOffset) != 0))
		{
			Debug(DEBUG_HARDWARE, "Atag One: ignoring UDP packet on discovery port with unexpected payload '%s'", buffer);
			continue;
		}

		char senderIp[INET_ADDRSTRLEN] = {};
		if (inet_ntop(AF_INET, &senderAddr.sin_addr, senderIp, sizeof(senderIp)) == nullptr)
		{
			Log(LOG_ERROR, "Atag One: failed to decode thermostat IP address from discovery packet");
			continue;
		}

		m_DeviceID.assign(buffer + deviceIdOffset, deviceIdLength);
		m_IPaddress = senderIp;
		Debug(DEBUG_HARDWARE, "Atag One thermostat discovered at %s with DeviceID %s", m_IPaddress.c_str(), m_DeviceID.c_str());
		closesocket(sock);
		return true;
	}

	closesocket(sock);
	Debug(DEBUG_HARDWARE, "Atag One thermostat not found via UDP discovery on port %u", atagDiscoveryPort);
	return false;
}

bool CAtagOneLocal::WriteToHardware(const char *pdata, const unsigned char /*length*/)
{
	return false;
}

void CAtagOneLocal::SetSetpoint(const int idx, const float temp)
{
	if (idx != 1)
	{
		Log(LOG_ERROR, "Currently only Room Temperature Setpoint allowed!");
		return;
	}

    if (temp < ATAGONE_TEMPERATURE_MIN || temp > ATAGONE_TEMPERATURE_MAX)
    {
        Log(LOG_ERROR, "Setpoint temperature out of range [%d, %d]!", ATAGONE_TEMPERATURE_MIN, ATAGONE_TEMPERATURE_MAX);
        return;
    }

	std::string sResult;
	std::string sURL;
 	std::vector<std::string> ExtraHeaders;
	std::vector<std::string> ResponseHeaders;

	sURL = "http://" + m_IPaddress + ":10000/update";

	// Round to half degree (0.5 precision)
    float rounded = std::round(temp * 2.0f) / 2.0f;
    
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

	Debug(DEBUG_HARDWARE, "POST data to thermostat: %s", sPostData.c_str());

	ExtraHeaders.push_back("Content-Type: application/x-www-form-urlencoded");

	std::string sHTTPReturn;
	if (!HTTPClient::POST(sURL, sPostData, ExtraHeaders, sHTTPReturn, ResponseHeaders))
	{
		Log(LOG_ERROR, "Error connecting to thermostat at %s!", m_IPaddress.c_str());
		return;
	}

	// Parse response
    Json::Value root;
    if (!ParseJSon(sHTTPReturn, root) || !root.isMember("update_reply"))
    {
        Log(LOG_ERROR, "Invalid response from setpoint update!");
		Debug(DEBUG_HARDWARE, "Received invalid response: .%s.", sHTTPReturn.c_str());
        return;
    }

    Json::Value reply = root["update_reply"];
    if (reply.isMember("acc_status") && reply["acc_status"].asInt() != ATAGONE_ACC_STATUS_OK)
    {
        Log(LOG_ERROR, "Thermostat rejected setpoint update (acc_status=%d)", reply["acc_status"].asInt());
        return;
    }

    Debug(DEBUG_HARDWARE, "Central heating setpoint set to %.1f°C", rounded);

	SendSetPointSensor(0, 0, 0, (const uint8_t)idx, 1, 255, rounded, "");
}
