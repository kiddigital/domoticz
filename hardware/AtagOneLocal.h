#pragma once

#include "DomoticzHardware.h"

class CAtagOneLocal : public CDomoticzHardwareBase
{
public:
	CAtagOneLocal(int ID, int Mode1, int Mode2, int Mode3, int Mode4, int Mode5, int Mode6);
	~CAtagOneLocal() override = default;
	bool WriteToHardware(const char *pdata, unsigned char length) override;
	void SetSetpoint(int idx, float temp);

private:
	void Init();
	bool StartHardware() override;
	bool StopHardware() override;
	void Do_Work();

	bool GetDeviceDetails();
	bool LoginThermostat();
	bool FindThermostat();
	bool ProcessDeviceDetails(const Json::Value &retrievedData);

	int m_LastMinute;
	bool m_bFoundThermostat;
	bool m_bLoggedIn;
	uint64_t m_seqNr;
	uint8_t m_iLoginAttempts;
	std::string m_IPaddress;
	std::string m_DeviceID;
	std::string m_MacAddress;

	std::shared_ptr<std::thread> m_thread;
};
