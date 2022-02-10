#include "stdafx.h"
#include "RFidTimerTCP.h"
#include "../main/Logger.h"
#include "../main/Helper.h"
#include "../main/mainworker.h"
#include "../hardware/hardwaretypes.h"

#define RETRY_DELAY 30
#define READ_POLL_MSEC 100

const unsigned char READCMD[6] = { 0xFF, 0x8B, 0x01, 0x00, 0x01, 0xCE };

namespace
{
	struct reader_packet
	{
		uint8_t head;
		uint8_t payload_size;
		uint8_t readerid;
		char payload[0];
	};

	struct writer_packet
	{
		uint8_t head;
		uint8_t payload_size;
		char payload[6];
	};

#define IS_HEAD_CHAR(x) ((x) == 0xA0)
}; // namespace

RFidTimerTCP::RFidTimerTCP(const int ID, const std::string &IPAddress, const unsigned short usIPPort, const int iReadInterval, const int iTagClosedTime, const int iTagReadTime)
	: m_szIPAddress(IPAddress), m_usIPPort(usIPPort), m_iReadInterval(iReadInterval), m_iTagClosedTime(iTagClosedTime), m_iTagReadTime(iTagReadTime)
{
	m_HwdID = ID;
	m_retrycntr = RETRY_DELAY;
	m_pPartialPkt = nullptr;
	m_PPktLen = 0;
	m_uiPacketCnt = 0;
}

RFidTimerTCP::~RFidTimerTCP()
{
	free(m_pPartialPkt);
}

bool RFidTimerTCP::StartHardware()
{
	RequestStart();

	// force connect the next first time
	m_retrycntr = RETRY_DELAY;
	m_bIsStarted = true;

	// Start worker thread
	m_thread = std::make_shared<std::thread>([this] { Do_Work(); });
	SetThreadNameInt(m_thread->native_handle());
	return (m_thread != nullptr);
}

bool RFidTimerTCP::StopHardware()
{
	if (m_thread)
	{
		RequestStop();
		m_thread->join();
		m_thread.reset();
	}
	m_bIsStarted = false;
	return true;
}

void RFidTimerTCP::OnConnect()
{
	Log(LOG_STATUS, "connected to: %s:%d", m_szIPAddress.c_str(), m_usIPPort);

	sOnConnected(this);
}

void RFidTimerTCP::OnDisconnect()
{
	Log(LOG_STATUS, "disconnected");
}

void RFidTimerTCP::Do_Work()
{
	Log(LOG_STATUS, "worker started...");
	Debug(DEBUG_HARDWARE, "Parameters: IPaddress (%s) Port (%d) ReadInterval (%d) TagClosedTime (%d) TagReadTime (%d)", m_szIPAddress.c_str(), m_usIPPort, m_iReadInterval, m_iTagClosedTime, m_iTagReadTime);

	uint32_t sec_counter = 0;
	uint16_t pass_counter = 0;
	uint16_t read_counter = 0;
	uint8_t noread_counter = 0;

	connect(m_szIPAddress, m_usIPPort);
	while (!IsStopRequested(READ_POLL_MSEC))
	{
		pass_counter++;

		if (pass_counter >= (1000/READ_POLL_MSEC))
		{
			pass_counter = 0;
			sec_counter++;

			if (sec_counter % 12 == 0)
			{
				m_LastHeartbeat = mytime(nullptr);
			}
		}

		if (isConnected())
		{
			read_counter++;
			if (read_counter >= m_iReadInterval)
			{
				if (m_uiPacketCnt == 0)
				{
					Debug(DEBUG_HARDWARE, "No packets received since last READ command!");
					noread_counter++;
				}
				else
				{
					noread_counter = 0;
				}
				SendReadPacket();
				read_counter = 0;
			}
			if (noread_counter >= 20)
			{
				Log(LOG_STATUS, "No data received after 20 consequtive READS! Reconnecting!");
				disconnect();
				noread_counter = 0;
			}
		}
	}
	terminate();

	Log(LOG_STATUS, "worker stopped...");
}

void RFidTimerTCP::OnError(const boost::system::error_code &error)
{
	if ((error == boost::asio::error::address_in_use) || (error == boost::asio::error::connection_refused) || (error == boost::asio::error::access_denied) ||
	    (error == boost::asio::error::host_unreachable) || (error == boost::asio::error::timed_out))
	{
		Log(LOG_ERROR, "Can not connect to: %s:%d", m_szIPAddress.c_str(), m_usIPPort);
	}
	else if ((error == boost::asio::error::eof) || (error == boost::asio::error::connection_reset))
	{
		Log(LOG_STATUS, "Connection reset!");
	}
	else
		Log(LOG_ERROR, "%s", error.message().c_str());
}

bool RFidTimerTCP::WriteToHardware(const char *pdata, const unsigned char /*length*/)
{
	if (!isConnected() || !pdata)
	{
		return false;
	}

	return true;
}

bool RFidTimerTCP::SendReadPacket()
{
	if (!isConnected())
	{
		return false;
	}

	Debug(DEBUG_RECEIVED, "Send ReadPacket");

	struct writer_packet *pPkt = (struct writer_packet *)malloc(sizeof(*pPkt));

	pPkt->head = 0xA0;
	pPkt->payload_size = sizeof(READCMD);
	memcpy(pPkt->payload, READCMD, sizeof(READCMD));

	if (!pPkt)
	{
		return false;
	}

	write((unsigned char *)pPkt, sizeof(*pPkt));
	free(pPkt);
	m_uiPacketCnt = 0;
	return true;
}

void RFidTimerTCP::OnData(const unsigned char *pData, size_t length)
{
	int Len = (int)length;

	if(Len == 12 || Len == 21)
	{
		// First byte is Header
		if (pData[0] == 0xA0)
		{
			if (pData[2] == 0x01 && pData[3] == 0x8B)
			{
				m_uiPacketCnt++;
				// Second byte is payload length
				if (pData[1] == 0x13 && Len == 21)	// 0x13 + Header + Length = 21 bytes
				{
					// Seems proper response structure (Header and both Lenght indicators match)
					// Byte 15-18 contain the Tag Identifier
					uint32_t uiTagID = 0;
					uiTagID = (uiTagID << 8) + pData[15];
					uiTagID = (uiTagID << 8) + pData[16];
					uiTagID = (uiTagID << 8) + pData[17];
					uiTagID = (uiTagID << 8) + pData[18];
					Debug(DEBUG_RECEIVED, "Found TagID (%d)", uiTagID);
				}
				else if (!(pData[1] == 0x0A && Len == 12))  // Default empty result is 12 bytes (0x0A + Header + Length)
				{
					Debug(DEBUG_RECEIVED, "Packet Received without TagID (%d) %s", Len, ToHexString(pData, Len).c_str());
				}
#ifdef _DEBUG
				else
				{
					Debug(DEBUG_RECEIVED, "Ignoring received no data Packet (%d) %s", Len, ToHexString(pData, Len).c_str());
				}
#endif
			}
			else
			{
				Debug(DEBUG_RECEIVED, "Packet does have proper header Byte 0xA0 but does not follow structure. Missing 0x01 0x8B after Length Byte (%d) %s", Len, ToHexString(pData, Len).c_str());
			}
		}
		else
		{
			Debug(DEBUG_RECEIVED, "Packet does not start with proper header Byte 0xA0 (%d) %s", Len, ToHexString(pData, Len).c_str());
		}
	}
	else
	{
		Debug(DEBUG_RECEIVED, "Unexpected Packet Received (%d) %s", Len, ToHexString(pData, Len).c_str());
	}
}
