#pragma once

#include "ASyncTCP.h"
#include "DomoticzHardware.h"

class RFidTimerTCP : public CDomoticzHardwareBase, ASyncTCP
{
      public:
	RFidTimerTCP(int ID, const std::string &IPAddress, unsigned short usIPPort);
	~RFidTimerTCP() override;
	bool WriteToHardware(const char *pdata, unsigned char length) override;
	boost::signals2::signal<void()> sDisconnected;
	bool SendReadPacket();

      private:
	bool StartHardware() override;
	bool StopHardware() override;
	bool CustomCommand(uint64_t idx, const std::string &sCommand) override;
	void ReceiveMessage(const char *pData, int Len);
	void Do_Work();

	void OnConnect() override;
	void OnDisconnect() override;
	void OnData(const unsigned char *pData, size_t length) override;
	void OnError(const boost::system::error_code &error) override;

	void ParseData(const unsigned char *pData, int Len);

      private:
	int m_retrycntr;
	unsigned char *m_pPartialPkt;
	int m_PPktLen;
	std::string m_szIPAddress;
	unsigned short m_usIPPort;
	std::shared_ptr<std::thread> m_thread;
};
