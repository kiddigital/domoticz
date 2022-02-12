#include "stdafx.h"
#include "RFidTimerTCP.h"
#include "../main/Logger.h"
#include "../main/Helper.h"
#include "../main/mainworker.h"
#include "../hardware/hardwaretypes.h"

#define RETRY_DELAY 30
#define READ_POLL_MSEC 100

#define HEAD_CHAR 0xA0

#define READINTERVAL_DEFAULT 10
#define TAGCLOSEDTIME_DEFAULT 5
#define TAGREADTIME_DEFAULT 5

#define SENSOR_NODE_ID 0
#define SENSOR_CHILD_ID 0
#define SENSOR_NAME "Last seen TAG"
#define SENSOR_LABEL "TAG id"

const unsigned char READCMD[6] = { 0xFF, 0x8B, 0x01, 0x00, 0x01, 0xCE };
namespace
{
	struct reader_packet
	{
		uint8_t head;
		uint8_t payload_size;
		uint16_t readerid;
		uint16_t inputid;
		uint32_t tagid;
		uint8_t checksum;
	};

	struct writer_packet
	{
		uint8_t head;
		uint8_t payload_size;
		char payload[6];
	};
}; // namespace

RFidTimerTCP::RFidTimerTCP(const int ID, const std::string &IPAddress, const unsigned short usIPPort, const int iReadInterval, const int iTagClosedTime, const int iTagReadTime)
	: m_szIPAddress(IPAddress), m_usIPPort(usIPPort), m_iReadInterval(iReadInterval), m_iTagClosedTime(iTagClosedTime), m_iTagReadTime(iTagReadTime)
{
	m_HwdID = ID;
	m_retrycntr = RETRY_DELAY;
	m_PPktLen = 0;
	m_uiPacketCnt = 999;

	if(m_iReadInterval < 1)
		m_iReadInterval = READINTERVAL_DEFAULT;
	if(m_iTagClosedTime < 1)
		m_iTagClosedTime = TAGCLOSEDTIME_DEFAULT;
	if(m_iTagReadTime < 1)
		m_iTagReadTime = TAGREADTIME_DEFAULT;
}

RFidTimerTCP::~RFidTimerTCP()
{
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

	bool bExists = false;
	float fCurValue = GetCustomSensor(SENSOR_NODE_ID, SENSOR_CHILD_ID, bExists);
	if (!bExists)
		SendCustomSensor(SENSOR_NODE_ID, SENSOR_CHILD_ID, 255, 0.0F, SENSOR_NAME, SENSOR_LABEL);

	uint32_t sec_counter = 0;
	uint16_t pass_counter = 0;
	uint8_t noread_counter = 0;
	uint16_t read_counter = m_iReadInterval;	// Start with reading right away

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
				read_counter = 0;
				SendReadPacket();
			}
			if (noread_counter >= 20)
			{
				Log(LOG_STATUS, "No data received after 20 consequtive READS! Disconnecting!");
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

	if (!pPkt)
	{
		return false;
	}

	pPkt->head = HEAD_CHAR;
	pPkt->payload_size = sizeof(READCMD);
	memcpy(pPkt->payload, READCMD, sizeof(READCMD));

	write((unsigned char *)pPkt, sizeof(*pPkt));
	free(pPkt);
	m_uiPacketCnt = 0;
	return true;
}

void RFidTimerTCP::OnData(const unsigned char *pData, size_t length)
{
	int Len = (int)length;

	if(Len >= 12 && pData[0] == HEAD_CHAR)
	{
		if (pData[2] == 0x01 && pData[3] == 0x8B)
		{
			// Received valid packet
			m_uiPacketCnt++;
			SetHeartbeatReceived();

			uint8_t uiMessageCnt = 0;
			while (uiMessageCnt < Len)
			{
				if (pData[0 + uiMessageCnt] == HEAD_CHAR && pData[1 + uiMessageCnt] == 0x13)
				{
					// Seems proper response structure (Header and Lenght indicators match)
					struct reader_packet *pPkt = (struct reader_packet *)malloc(sizeof(*pPkt));

					if (!pPkt)
					{
						return;
					}

					pPkt->head = pData[0 + uiMessageCnt];
					pPkt->payload_size = pData[1 + uiMessageCnt];
					pPkt->readerid = pData[2 + uiMessageCnt];
					pPkt->readerid = (pPkt->readerid << 8) + pData[3 + uiMessageCnt];
					pPkt->inputid = pData[4 + uiMessageCnt];
					pPkt->inputid = (pPkt->inputid << 8) + pData[5 + uiMessageCnt];
					pPkt->tagid = pData[15 + uiMessageCnt];
					pPkt->tagid = (pPkt->tagid << 8) + pData[16 + uiMessageCnt];
					pPkt->tagid = (pPkt->tagid << 8) + pData[17 + uiMessageCnt];
					pPkt->tagid = (pPkt->tagid << 8) + pData[18 + uiMessageCnt];
					pPkt->checksum = pData[20 + uiMessageCnt];
					// To-do: Validate received bytes with received checksum

					uint32_t uiTagID = pPkt->tagid;
					Debug(DEBUG_HARDWARE, "Found TagID (%d) on input (%d) from reader (%d) in Packet (%d) %s", uiTagID, pPkt->inputid, pPkt->readerid, Len, ToHexString(pData, Len).c_str());
					SendCustomSensor(SENSOR_NODE_ID, SENSOR_CHILD_ID, 255, uiTagID, SENSOR_NAME, SENSOR_LABEL);

					free(pPkt);

					uiMessageCnt = uiMessageCnt + 21;
				}
				else if (pData[0 + uiMessageCnt] == HEAD_CHAR && pData[1 + uiMessageCnt] == 0x0A)
				{
					// No Data message
					uiMessageCnt = uiMessageCnt + 12;
#ifdef _DEBUG
					Debug(DEBUG_RECEIVED, "Ignoring received No Data Message (%d) %s", Len, ToHexString(pData, Len).c_str());
#endif
				}
				else
				{
					Debug(DEBUG_RECEIVED, "Packet Received without TagID (%d) %s", Len, ToHexString(pData, Len).c_str());
					while (uiMessageCnt < Len && pData[uiMessageCnt] != HEAD_CHAR)
					{
						uiMessageCnt++;
					}
				}
			}
		}
		else
		{
			Debug(DEBUG_RECEIVED, "Packet does have proper header Byte but does not follow structure. Missing 0x01 0x8B after Length Byte (%d) %s", Len, ToHexString(pData, Len).c_str());
		}
	}
	else
	{
		Debug(DEBUG_RECEIVED, "Unexpected Packet Received (%d) %s", Len, ToHexString(pData, Len).c_str());
	}
}
