#include "stdafx.h"
#include "RFidTimerTCP.h"
#include "../main/Logger.h"
#include "../main/Helper.h"
#include "../main/SQLHelper.h"
#include <iostream>
#include "../main/localtime_r.h"
#include "../main/mainworker.h"
#include "../hardware/hardwaretypes.h"
#include <json/json.h>
#include "../tinyxpath/tinyxml.h"
#include "../main/WebServer.h"

#include <sstream>

#define RETRY_DELAY 30
#define READ_POLL_MSEC 1000

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

RFidTimerTCP::RFidTimerTCP(const int ID, const std::string &IPAddress, const unsigned short usIPPort)
	: m_szIPAddress(IPAddress)
{
	m_HwdID = ID;
	m_usIPPort = usIPPort;
	m_retrycntr = RETRY_DELAY;
	m_pPartialPkt = nullptr;
	m_PPktLen = 0;
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
	m_bIsStarted = true;

	//SendPacket(READCMD);
	sOnConnected(this);
}

void RFidTimerTCP::OnDisconnect()
{
	Log(LOG_STATUS, "disconnected");
}

void RFidTimerTCP::Do_Work()
{
	Log(LOG_STATUS, "worker started...");

	uint32_t sec_counter = 0;
	uint16_t pass_counter = 0;
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
			SendReadPacket();
		}
	}
	terminate();

	Log(LOG_STATUS, "worker stopped...");
}

void RFidTimerTCP::OnData(const unsigned char *pData, size_t length)
{
	ParseData(pData, length);
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

	Debug(DEBUG_HARDWARE, "Send ReadPacket");

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
	return true;
}

void RFidTimerTCP::ReceiveMessage(const char *pData, int Len)
{
	while (Len && (pData[Len - 1] == '\r' || pData[Len - 1] == '\n'))
		Len--;

	if (Len < 4 || !IS_HEAD_CHAR(pData[Len - 1]))
		return;

	Log(LOG_NORM, "Packet received: %d %.*s", Len, Len - 1, pData);
}

void RFidTimerTCP::ParseData(const unsigned char *pData, int Len)
{
	Debug(DEBUG_RECEIVED, "Packet Received (%d) %s", Len, ToHexString(pData, Len).c_str());
	/*
	if (m_pPartialPkt)
	{
		unsigned char *new_data = (unsigned char *)realloc(m_pPartialPkt, m_PPktLen + Len);
		if (!new_data)
		{
			free(m_pPartialPkt);
			m_pPartialPkt = nullptr;
			m_PPktLen = 0;
			Log(LOG_ERROR, "Failed to prepend previous data");
			// We'll attempt to resync
		}
		else
		{
			memcpy(new_data + m_PPktLen, pData, Len);
			m_pPartialPkt = new_data;
			Len += m_PPktLen;
			pData = new_data;
		}
	}
	while (Len >= 18)
	{
		const struct reader_packet *pkt = (const struct reader_packet *)pData;
		if (pkt->head != 0x0A)
		{
			Len--;
			pData++;
			continue;
		}
		int data_size = static_cast<int>(ntohl(pkt->data_size));
		if (Len < 16 + data_size)
			break;

		ReceiveMessage(pkt->payload, data_size - 2);
		Len -= 16 + data_size;
		pData += 16 + data_size;
	}
	unsigned char *new_partial = nullptr;
	if (Len)
	{
		if (pData == m_pPartialPkt)
		{
			m_PPktLen = Len;
			return;
		}
		new_partial = (unsigned char *)malloc(Len);
		if (new_partial)
			memcpy(new_partial, pData, Len);
		else
			Len = 0;
	}
	free(m_pPartialPkt);
	m_PPktLen = Len;
	m_pPartialPkt = new_partial;
	*/
}

bool RFidTimerTCP::CustomCommand(const uint64_t /*idx*/, const std::string &sCommand)
{
	return SendReadPacket();
}

// Webserver helpers (TO-DO: if needed?)
/*
namespace http
{
	namespace server
	{
		void CWebServer::Cmd_RFidTimerTCPCommand(WebEmSession &session, const request &req, Json::Value &root)
		{
			if (session.rights != 2)
			{
				session.reply_status = reply::forbidden;
				return; // Only admin user allowed
			}

			std::string sIdx = request::findValue(&req, "idx");
			std::string sAction = request::findValue(&req, "action");
			if (sIdx.empty())
				return;
			// int idx = atoi(sIdx.c_str());

			std::vector<std::vector<std::string>> result;
			result = m_sql.safe_query("SELECT DS.SwitchType, DS.DeviceID, H.Type, H.ID FROM DeviceStatus DS, Hardware H WHERE (DS.ID=='%q') AND (DS.HardwareID == H.ID)", sIdx.c_str());

			root["status"] = "ERR";
			if (result.size() == 1)
			{
				_eHardwareTypes hType = (_eHardwareTypes)atoi(result[0][2].c_str());
				switch (hType)
				{
					// We allow raw EISCP commands to be sent on *any* of the logical devices
					// associated with the hardware.
					case HTYPE_RFidTimerTCP:
						CDomoticzHardwareBase *pBaseHardware = m_mainworker.GetHardwareByIDType(result[0][3], HTYPE_RFidTimerTCP);
						if (pBaseHardware == nullptr)
							return;
						RFidTimerTCP *pRFidTimerTCP = reinterpret_cast<RFidTimerTCP *>(pBaseHardware);

						pRFidTimerTCP->SendPacket(sAction.c_str());
						root["status"] = "OK";
						root["title"] = "RFidTimerTCPCommand";
						break;
				}
			}
		}

	} // namespace server
} // namespace http
*/
