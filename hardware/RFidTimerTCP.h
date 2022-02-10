#pragma once

#include "ASyncTCP.h"
#include "DomoticzHardware.h"

class RFidTimerTCP : public CDomoticzHardwareBase, ASyncTCP
{
      public:
	RFidTimerTCP(int ID, const std::string &IPAddress, unsigned short usIPPort, const int iReadInterval, const int iTagClosedTime, const int iTagReadTime);
	~RFidTimerTCP() override;
	bool WriteToHardware(const char *pdata, unsigned char length) override;
	boost::signals2::signal<void()> sDisconnected;
	bool SendReadPacket();

      private:
	bool StartHardware() override;
	bool StopHardware() override;
	void Do_Work();

	void OnConnect() override;
	void OnDisconnect() override;
	void OnData(const unsigned char *pData, size_t length) override;
	void OnError(const boost::system::error_code &error) override;

      private:
	int m_retrycntr;
	unsigned char *m_pPartialPkt;
	int m_PPktLen;
	std::string m_szIPAddress;
	unsigned short m_usIPPort;
	int m_iReadInterval;
	int m_iTagClosedTime;
	int m_iTagReadTime;
	uint16_t m_uiPacketCnt;
	std::shared_ptr<std::thread> m_thread;
};
