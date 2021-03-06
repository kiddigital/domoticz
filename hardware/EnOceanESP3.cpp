#include "stdafx.h"

#include <string>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <ctime>

#include <boost/exception/diagnostic_information.hpp>

#include "../main/Logger.h"
#include "../main/Helper.h"
#include "../main/RFXtrx.h"
#include "../main/SQLHelper.h"
#include "../main/localtime_r.h"

#include "hardwaretypes.h"
#include "EnOceanESP3.h"

// ESP3 Packet types
enum ESP3_PACKET_TYPE : uint8_t
{
	PACKET_RADIO_ERP1 = 0x01,	  // Radio telegram
	PACKET_RESPONSE = 0x02,		  // Response to any packet
	PACKET_RADIO_SUB_TEL = 0x03,	  // Radio subtelegram
	PACKET_EVENT = 0x04,		  // Event message
	PACKET_COMMON_COMMAND = 0x05,	  // Common command"
	PACKET_SMART_ACK_COMMAND = 0x06,  // Smart Acknowledge command
	PACKET_REMOTE_MAN_COMMAND = 0x07, // Remote management command
	PACKET_RADIO_MESSAGE = 0x09,	  // Radio message
	PACKET_RADIO_ERP2 = 0x0A,	  // ERP2 protocol radio telegram
	PACKET_CONFIG_COMMAND = 0x0B,	  // RESERVED
	PACKET_COMMAND_ACCEPTED = 0x0C,	  // For long operations, informs the host the command is accepted
	PACKET_RADIO_802_15_4 = 0x10,	  // 802_15_4 Raw Packet
	PACKET_COMMAND_2_4 = 0x11,	  // 2.4 GHz Command
};

// ESP3 Return codes
enum ESP3_RETURN_CODE : uint8_t
{
	RET_OK = 0x00,			// OK ... command is understood and triggered
	RET_ERROR = 0x01,		// There is an error occured
	RET_NOT_SUPPORTED = 0x02,	// The functionality is not supported by that implementation
	RET_WRONG_PARAM = 0x03,		// There was a wrong parameter in the command
	RET_OPERATION_DENIED = 0x04,	// Example: memory access denied (code-protected)
	RET_LOCK_SET = 0x05,		// Duty cycle lock
	RET_BUFFER_TO_SMALL = 0x06,	// The internal ESP3 buffer of the device is too small, to handle this telegram
	RET_NO_FREE_BUFFER = 0x07,	// Currently all internal buffers are used
	RET_MEMORY_ERROR = 0x82,	// The memory write process failed
	RET_BASEID_OUT_OF_RANGE = 0x90, // BaseID out of range
	RET_BASEID_MAX_REACHED = 0x91,	// BaseID has already been changed 10 times, no more changes are allowed
};

// ESP3 Event codes
enum ESP3_EVENT_CODE : uint8_t
{
	SA_RECLAIM_NOT_SUCCESSFUL = 0x01, // Informs the external host about an unsuccessful reclaim by a Smart Ack. client
	SA_CONFIRM_LEARN = 0x02,	  // Request to the external host about how to handle a received learn-in / learn-out of a Smart Ack. client
	SA_LEARN_ACK = 0x03,		  // Response to the Smart Ack. client about the result of its Smart Acknowledge learn request
	CO_READY = 0x04,		  // Inform the external about the readiness for operation
	CO_EVENT_SECUREDEVICES = 0x05,	  // Informs the external host about an event in relation to security processing
	CO_DUTYCYCLE_LIMIT = 0x06,	  // Informs the external host about reaching the duty cycle limit
	CO_TRANSMIT_FAILED = 0x07,	  // Informs the external host about not being able to send a telegram
	CO_TX_DONE = 0x08,		  // Informs that all TX operations are done
	CO_LRN_MODE_DISABLED = 0x09	  // Informs that the learn mode has time-out
};

// ESP3 Common commands
enum ESP3_COMMON_COMMAND : uint8_t
{
	CO_WR_SLEEP = 0x01,			  // Enter energy saving mode
	CO_WR_RESET = 0x02,			  // Reset the device
	CO_RD_VERSION = 0x03,			  // Read the device version information
	CO_RD_SYS_LOG = 0x04,			  // Read system log
	CO_WR_SYS_LOG = 0x05,			  // Reset system log
	CO_WR_BIST = 0x06,			  // Perform Self Test
	CO_WR_IDBASE = 0x07,			  // Set ID range base address
	CO_RD_IDBASE = 0x08,			  // Read ID range base address
	CO_WR_REPEATER = 0x09,			  // Set Repeater Level
	CO_RD_REPEATER = 0x10,			  // Read Repeater Level
	CO_WR_FILTER_ADD = 0x11,		  // Add filter to filter list
	CO_WR_FILTER_DEL = 0x12,		  // Delete a specific filter from filter list
	CO_WR_FILTER_DEL_ALL = 0x13,		  // Delete all filters from filter list
	CO_WR_FILTER_ENABLE = 0x14,		  // Enable / disable filter list
	CO_RD_FILTER = 0x15,			  // Read filters from filter list
	CO_WR_WAIT_MATURITY = 0x16,		  // Wait until the end of telegram maturity time before received radio telegrams will be forwarded to the external host
	CO_WR_SUBTEL = 0x17,			  // Enable / Disable transmission of additional subtelegram info to the external host
	CO_WR_MEM = 0x18,			  // Write data to device memory
	CO_RD_MEM = 0x19,			  // Read data from device memory
	CO_RD_MEM_ADDRESS = 0x20,		  // Read address and length of the configuration area and the Smart Ack Table
	CO_RD_SECURITY = 0x21,			  // DEPRECATED Read own security information (level, // key)
	CO_WR_SECURITY = 0x22,			  // DEPRECATED Write own security information (level, // key)
	CO_WR_LEARNMODE = 0x23,			  // Enable / disable learn mode
	CO_RD_LEARNMODE = 0x24,			  // ead learn mode status
	CO_WR_SECUREDEVICE_ADD = 0x25,		  // DEPRECATED Add a secure device
	CO_WR_SECUREDEVICE_DEL = 0x26,		  // Delete a secure device from the link table
	CO_RD_SECUREDEVICE_BY_INDEX = 0x27,	  // DEPRECATED Read secure device by index
	CO_WR_MODE = 0x28,			  // Set the gateway transceiver mode
	CO_RD_NUMSECUREDEVICES = 0x29,		  // Read number of secure devices in the secure link table
	CO_RD_SECUREDEVICE_BY_ID = 0x30,	  // Read information about a specific secure device from the secure link table using the device ID
	CO_WR_SECUREDEVICE_ADD_PSK = 0x31,	  // Add Pre-shared key for inbound secure device
	CO_WR_SECUREDEVICE_ENDTEACHIN = 0x32,	  // Send Secure teach-In message
	CO_WR_TEMPORARY_RLC_WINDOW = 0x33,	  // Set a temporary rolling-code window for every taught-in device
	CO_RD_SECUREDEVICE_PSK = 0x34,		  // Read PSK
	CO_RD_DUTYCYCLE_LIMIT = 0x35,		  // Read the status of the duty cycle limit monitor
	CO_SET_BAUDRATE = 0x36,			  // Set the baud rate used to communicate with the external host
	CO_GET_FREQUENCY_INFO = 0x37,		  // Read the radio frequency and protocol supported by the device
	CO_38T_STEPCODE = 0x38,			  // Read Hardware Step code and Revision of the Device
	CO_40_RESERVED = 0x40,			  // Reserved
	CO_41_RESERVED = 0x41,			  // Reserved
	CO_42_RESERVED = 0x42,			  // Reserved
	CO_43_RESERVED = 0x43,			  // Reserved
	CO_44_RESERVED = 0x44,			  // Reserved
	CO_45_RESERVED = 0x45,			  // Reserved
	CO_WR_REMAN_CODE = 0x46,		  // Set the security code to unlock Remote Management functionality via radio
	CO_WR_STARTUP_DELAY = 0x47,		  // Set the startup delay (time from power up until start of operation)
	CO_WR_REMAN_REPEATING = 0x48,		  // Select if REMAN telegrams originating from this module can be repeated
	CO_RD_REMAN_REPEATING = 0x49,		  // Check if REMAN telegrams originating from this module can be repeated
	CO_SET_NOISETHRESHOLD = 0x50,		  // Set the RSSI noise threshold level for telegram reception
	CO_GET_NOISETHRESHOLD = 0x51,		  // Read the RSSI noise threshold level for telegram reception
	CO_52_RESERVED = 0x52,			  // Reserved
	CO_53_RESERVED = 0x53,			  // Reserved
	CO_WR_RLC_SAVE_PERIOD = 0x54,		  // Set the period in which outgoing RLCs are saved to the EEPROM
	CO_WR_RLC_LEGACY_MODE = 0x55,		  // Activate the legacy RLC security mode allowing roll-over and using the RLC acceptance window for 24bit explicit RLC
	CO_WR_SECUREDEVICEV2_ADD = 0x56,	  // Add secure device to secure link table
	CO_RD_SECUREDEVICEV2_BY_INDEX = 0x57,	  // Read secure device from secure link table using the table index
	CO_WR_RSSITEST_MODE = 0x58,		  // Control the state of the RSSI-Test mode
	CO_RD_RSSITEST_MODE = 0x59,		  // Read the state of the RSSI-Test Mode
	CO_WR_SECUREDEVICE_MAINTENANCEKEY = 0x60, // Add the maintenance key information into the secure link table
	CO_RD_SECUREDEVICE_MAINTENANCEKEY = 0x61, // Read by index the maintenance key information from the secure link table
	CO_WR_TRANSPARENT_MODE = 0x62,		  // Control the state of the transparent mode
	CO_RD_TRANSPARENT_MODE = 0x63,		  // Read the state of the transparent mode
	CO_WR_TX_ONLY_MODE = 0x64,		  // Control the state of the TX only mode
	CO_RD_TX_ONLY_MODE = 0x65		  // Read the state of the TX only mode} COMMON_COMMAND;
};

// ESP3 Smart Ack codes
enum ESP3_SMART_ACK_CODE : uint8_t
{
	SA_WR_LEARNMODE = 0x01,	     // Set/Reset Smart Ack learn mode
	SA_RD_LEARNMODE = 0x02,	     // Get Smart Ack learn mode state
	SA_WR_LEARNCONFIRM = 0x03,   // Used for Smart Ack to add or delete a mailbox of a client
	SA_WR_CLIENTLEARNRQ = 0x04,  // Send Smart Ack Learn request (Client)
	SA_WR_RESET = 0x05,	     // Send reset command to a Smart Ack client
	SA_RD_LEARNEDCLIENTS = 0x06, // Get Smart Ack learned sensors / mailboxes
	SA_WR_RECLAIMS = 0x07,	     // Set number of reclaim attempts
	SA_WR_POSTMASTER = 0x08	     // Activate/Deactivate Post master functionality
};

// ESP3 Function return codes
enum ESP3_FUNC_RETURN_CODE : uint8_t
{
	RC_OK = 0,		    // Action performed. No problem detected
	RC_EXIT,		    // Action not performed. No problem detected
	RC_KO,			    // Action not performed. Problem detected
	RC_TIME_OUT,		    // Action couldn't be carried out within a certain time.
	RC_FLASH_HW_ERROR,	    // The write/erase/verify process failed, the flash page seems to be corrupted
	RC_NEW_RX_BYTE,		    // A new UART/SPI byte received
	RC_NO_RX_BYTE,		    // No new UART/SPI byte received
	RC_NEW_RX_TEL,		    // New telegram received
	RC_NO_RX_TEL,		    // No new telegram received
	RC_NOT_VALID_CHKSUM,	    // Checksum not valid
	RC_NOT_VALID_TEL,	    // Telegram not valid
	RC_BUFF_FULL,		    // Buffer full, no space in Tx or Rx buffer
	RC_ADDR_OUT_OF_MEM,	    // Address is out of memory
	RC_NOT_VALID_PARAM,	    // Invalid function parameter
	RC_BIST_FAILED,		    // Built in self test failed
	RC_ST_TIMEOUT_BEFORE_SLEEP, // Before entering power down, the short term timer had timed out.
	RC_MAX_FILTER_REACHED,	    // Maximum number of filters reached, no more filter possible
	RC_FILTER_NOT_FOUND,	    // Filter to delete not found
	RC_BASEID_OUT_OF_RANGE,	    // BaseID out of range
	RC_BASEID_MAX_REACHED,	    // BaseID was changed 10 times, no more changes are allowed
	RC_XTAL_NOT_STABLE,	    // XTAL is not stable
	RC_NO_TX_TEL,		    // No telegram for transmission in queue
	RC_ELEGRAM_WAIT,	    //	Waiting before sending broadcast message
	RC_OUT_OF_RANGE,	    //	Generic out of range return code
	RC_LOCK_SET,		    //	Function was not executed due to sending lock
	RC_NEW_TX_TEL		    // New telegram transmitted
};

#if _DEBUG
	#define ENOCEAN_BUTTON_DEBUG
#endif

#define ENABLE_LOGGING

#define ENOCEAN_RETRY_DELAY 30

//Write/Read has to be done in sync with ESP3

#define ESP3_SYNC 0x55
#define ESP3_HEADER_LENGTH 0x4

#define round(a) ( int ) ( a + .5 )

// the following lines are taken from EO300I API header file

//polynomial G(x) = x8 + x2 + x1 + x0 is used to generate the CRC8 table
const unsigned char crc8table[256] = {
	0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15,
	0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d,
	0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65,
	0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
	0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5,
	0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
	0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85,
	0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
	0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2,
	0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
	0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2,
	0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
	0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32,
	0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
	0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42,
	0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
	0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c,
	0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
	0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec,
	0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
	0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c,
	0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
	0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c,
	0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
	0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b,
	0x76, 0x71, 0x78, 0x7f, 0x6A, 0x6d, 0x64, 0x63,
	0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b,
	0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
	0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb,
	0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8D, 0x84, 0x83,
	0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb,
	0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3
};

#define proc_crc8(crc, data) (crc8table[crc ^ data])

#define SER_SYNCH_CODE 0x55
#define SER_HEADER_NR_BYTES 0x04

// end of lines from EO300I API header file

/**
 * @defgroup bitmasks Bitmasks for various fields.
 * There are two definitions for every bit mask. First, the bit mask itself
 * and also the number of necessary shifts.
 * @{
 */
/**
 * @defgroup status_rps Status of telegram (for RPS telegrams)
 * Bitmasks for the status-field, if ORG = RPS.
 * @{
 */
#define S_RPS_T21 0x20
#define S_RPS_T21_SHIFT 5
#define S_RPS_NU  0x10
#define S_RPS_NU_SHIFT 4
#define S_RPS_RPC 0x0F
#define S_RPS_RPC_SHIFT 0
/*@}*/

#define F60201_R1_MASK 0xE0
#define F60201_R1_SHIFT 5
#define F60201_EB_MASK 0x10
#define F60201_EB_SHIFT 4
#define F60201_R2_MASK 0x0E
#define F60201_R2_SHIFT 1
#define F60201_SA_MASK 0x01
#define F60201_SA_SHIFT 0

#define F60201_BUTTON_A1 0
#define F60201_BUTTON_A0 1
#define F60201_BUTTON_B1 2
#define F60201_BUTTON_B0 3

/**
 * @defgroup status_rpc Status of telegram (for 1BS, 4BS, HRC or 6DT telegrams)
 * Bitmasks for the status-field, if ORG = 1BS, 4BS, HRC or 6DT.
 * @{
 */
#define S_RPC 0x0F
#define S_RPC_SHIFT 0
/*@}*/

/**
 * @defgroup data3 Meaning of data_byte 3 (for RPS telegrams, NU = 1)
 * Bitmasks for the data_byte3-field, if ORG = RPS and NU = 1.
 * Specification can be found at:
 *      https://www.enocean.com/fileadmin/redaktion/enocean_alliance/pdf/EnOcean_Equipment_Profiles_EEP_V2.6.3_public.pdf
 * @{
 */

	//!	Rocker ID Mask
#define DB3_RPS_NU_RID 0xC0
#define DB3_RPS_NU_RID_SHIFT 6

//!	Button ID Mask
#define DB3_RPS_NU_BID 0xE0
#define DB3_RPS_NU_BID_SHIFT 5

//!	Up Down Mask
#define DB3_RPS_NU_UD  0x20
#define DB3_RPS_NU_UD_SHIFT  5

//!	Pressed Mask
#define DB3_RPS_NU_PR  0x10
#define DB3_RPS_NU_PR_SHIFT 4

//!	Second Rocker ID Mask
#define DB3_RPS_NU_SRID 0x0C
#define DB3_RPS_NU_SRID_SHIFT 2

//!	Second Button ID Mask
#define DB3_RPS_NU_SBID 0x0E
#define DB3_RPS_NU_SBID_SHIFT 1

//!	Second UpDown Mask
#define DB3_RPS_NU_SUD 0x02
#define DB3_RPS_NU_SUD_SHIFT 1

//!	Second Action Mask
#define DB3_RPS_NU_SA 0x01
#define DB3_RPS_NU_SA_SHIFT 0

/*@}*/

/**
 * @defgroup data3_1 Meaning of data_byte 3 (for RPS telegrams, NU = 0)
 * Bitmasks for the data_byte3-field, if ORG = RPS and NU = 0.
 * @{
 */
#define DB3_RPS_BUTTONS 0xE0
#define DB3_RPS_BUTTONS_SHIFT 5
#define DB3_RPS_PR 0x10
#define DB3_RPS_PR_SHIFT 4
/*@}*/

/**
 * @defgroup data0 Meaning of data_byte 0 (for 4BS telegrams)
 * Bitmasks for the data_byte0-field, if ORG = 4BS.
 * @{
 */
#define DB0_4BS_DI_3 0x08
#define DB0_4BS_DI_3_SHIFT 3
#define DB0_4BS_DI_2 0x04
#define DB0_4BS_DI_2_SHIFT 2
#define DB0_4BS_DI_1 0x02
#define DB0_4BS_DI_1_SHIFT 1
#define DB0_4BS_DI_0 0x01
#define DB0_4BS_DI_0_SHIFT 0
/*@}*/

/**
 * @defgroup data3_hrc Meaning of data_byte 3 (for HRC telegrams)
 * Bitmasks for the data_byte3-field, if ORG = HRC.
 * @{
 */
#define DB3_HRC_RID 0xC0
#define DB3_HRC_RID_SHIFT 6
#define DB3_HRC_UD  0x20
#define DB3_HRC_UD_SHIFT 5
#define DB3_HRC_PR  0x10
#define DB3_HRC_PR_SHIFT 4
#define DB3_HRC_SR  0x08
#define DB3_HRC_SR_SHIFT 3

// 2016-01-31 Stéphane Guillard : added 4BS learn bit definitions below
#define RORG_4BS_TEACHIN_LRN_BIT (1 << 3)
#define RORG_4BS_TEACHIN_EEP_BIT (1 << 7)

bool CEnOceanESP3::sendFrame(unsigned char frametype, unsigned char *databuf, unsigned short datalen, unsigned char *optdata, unsigned char optdatalen)
{
	unsigned char crc=0;
	unsigned char buf[1024];
	int len=0;

	buf[len++]=SER_SYNCH_CODE;
	buf[len++]=(datalen >> 8) & 0xFF; // len
	buf[len++]=datalen & 0xFF;
	buf[len++]=optdatalen;
	buf[len++]=frametype;
	crc = proc_crc8(crc, buf[1]);
	crc = proc_crc8(crc, buf[2]);
	crc = proc_crc8(crc, buf[3]);
	crc = proc_crc8(crc, buf[4]);
	buf[len++]=crc;
	crc = 0;
	for (int i=0;i<datalen;i++) {
		buf[len]=databuf[i];
		crc=proc_crc8(crc, buf[len++]);
	}
	for (int i=0;i<optdatalen;i++) {
		buf[len]=optdata[i];
		crc=proc_crc8(crc, buf[len++]);
	}
	buf[len++]=crc;
	write((const char*)&buf,len);
	return true;
}

bool CEnOceanESP3::sendFrameQueue(unsigned char frametype, unsigned char *databuf, unsigned short datalen, unsigned char *optdata, unsigned char optdatalen)
{
	unsigned char crc=0;
	unsigned char buf[1024];
	int len=0;

	buf[len++]=SER_SYNCH_CODE;
	buf[len++]=(datalen >> 8) & 0xff; // len
	buf[len++]=datalen & 0xff;
	buf[len++]=optdatalen;
	buf[len++]=frametype;
	crc = proc_crc8(crc, buf[1]);
	crc = proc_crc8(crc, buf[2]);
	crc = proc_crc8(crc, buf[3]);
	crc = proc_crc8(crc, buf[4]);
	buf[len++]=crc;
	crc = 0;
	for (int i=0;i<datalen;i++) {
		buf[len]=databuf[i];
		crc=proc_crc8(crc, buf[len++]);
	}
	for (int i=0;i<optdatalen;i++) {
		buf[len]=optdata[i];
		crc=proc_crc8(crc, buf[len++]);
	}
	buf[len++]=crc;
	Add2SendQueue((const char*)&buf,len);
	return true;
}

#ifndef _DEBUG
//#define USE_TEST
#endif

#ifdef USE_TEST
struct _tEnocean3TestStruct
{
	uint8_t PacketType;
	uint8_t DataSize;
	uint8_t OptionalDataSize;
	uint8_t pData[40];
};

const std::vector<uint8_t> TestArray[] = { { 0x01, 0x07, 0x07, 0xF6, 0x50, 0x00, 0x32, 0x9C, 0xE3, 0x30, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x50, 0x00 }, // 3 button rockker
					   { 0x01, 0x07, 0x07, 0xF6, 0x00, 0x00, 0x32, 0x9C, 0xE3, 0x20, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x53, 0x00 },

					   { 0x01, 0x07, 0x07, 0xF6, 0x10, 0x00, 0x32, 0x9C, 0xE3, 0x30, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x58, 0x00 },
					   { 0x01, 0x07, 0x07, 0xF6, 0x00, 0x00, 0x32, 0x9C, 0xE3, 0x20, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x56, 0x00 } };
#endif


CEnOceanESP3::CEnOceanESP3(const int ID, const std::string& devname, const int type)
{
	m_HwdID=ID;
	m_szSerialPort=devname;
    m_Type = type;
	m_bufferpos=0;
	memset(&m_buffer,0,sizeof(m_buffer));
	m_id_base=0;
	m_receivestate=ERS_SYNCBYTE;
#ifdef USE_TEST
	// Test
	for (const auto &itt : TestArray)
	{
		_tEnocean3TestStruct *pTest = (_tEnocean3TestStruct *)&itt.at(0);

		m_ReceivedPacketType = pTest->PacketType;
		m_DataSize = pTest->DataSize;
		m_OptionalDataSize = pTest->OptionalDataSize;
		m_bufferpos = 0;
		for (int ii = 0; ii < m_DataSize + m_OptionalDataSize; ii++)
			m_buffer[m_bufferpos++] = pTest->pData[ii];
		ParseData();
	}
#endif
}

bool CEnOceanESP3::StartHardware()
{
	RequestStart();

	Init();

	m_retrycntr=ENOCEAN_RETRY_DELAY*5; //will force reconnect first thing

	//Start worker thread
	m_thread = std::make_shared<std::thread>([this] { Do_Work(); });
	SetThreadNameInt(m_thread->native_handle());

	return (m_thread != nullptr);
}

bool CEnOceanESP3::StopHardware()
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

void CEnOceanESP3::Init()
{
	ReloadVLDNodes();
}

void CEnOceanESP3::ReloadVLDNodes()
{
	m_VLDNodes.clear();
	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT ID, DeviceID, Manufacturer, Profile, [Type] FROM EnoceanSensors WHERE (HardwareID==%d)", m_HwdID);
	if (!result.empty())
	{
		for (const auto &sd : result)
		{
			_tVLDNode node;
			node.idx = atoi(sd[0].c_str());
			node.manufacturer = atoi(sd[2].c_str());
			node.profile = (uint8_t)atoi(sd[3].c_str());
			node.type = (uint8_t)atoi(sd[4].c_str());

			//convert to hex, and we have our ID
			std::stringstream s_strid;
			s_strid << std::hex << std::uppercase << sd[1];
			uint32_t devid;
			s_strid >> devid;
			m_VLDNodes[devid] = node;
		}
	}
}

void CEnOceanESP3::Do_Work()
{
	int msec_counter=0;
	int sec_counter = 0;

	Log(LOG_STATUS, "Worker started...");

	while (!IsStopRequested(200))
	{
		msec_counter++;
		if (msec_counter == 5)
		{
			msec_counter = 0;
			sec_counter++;
			if (sec_counter % 12 == 0)
			{
				m_LastHeartbeat = mytime(nullptr);
			}
		}

		if (!isOpen())
		{
			if (m_retrycntr==0)
			{
				Log(LOG_STATUS,"serial retrying in %d seconds...", ENOCEAN_RETRY_DELAY);
			}
			m_retrycntr++;
			if (m_retrycntr/5>=ENOCEAN_RETRY_DELAY)
			{
				m_retrycntr=0;
				m_bufferpos=0;
				OpenSerialDevice();
			}
		}
		if (!m_sendqueue.empty())
		{
			std::lock_guard<std::mutex> l(m_sendMutex);

			std::vector<std::string>::iterator itt=m_sendqueue.begin();
			if (itt!=m_sendqueue.end())
			{
				std::string sBytes=*itt;
				write(sBytes.c_str(),sBytes.size());
				m_sendqueue.erase(itt);
			}
		}
	}
	terminate();

	Log(LOG_STATUS,"Worker stopped...");
}

void CEnOceanESP3::Add2SendQueue(const char* pData, const size_t length)
{
#ifdef ENABLE_LOGGING
	std::stringstream sstr;

	for (size_t idx = 0; idx < length; idx++)
	{
		sstr << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (((unsigned int)pData[idx]) & 0xFF);
		if (idx != length - 1)
			sstr << " ";
	}
	Log(LOG_STATUS,"EnOcean Send: %s",sstr.str().c_str());
#endif

	std::string sBytes;
	sBytes.insert(0,pData,length);
	std::lock_guard<std::mutex> l(m_sendMutex);
	m_sendqueue.push_back(sBytes);
}


bool CEnOceanESP3::OpenSerialDevice()
{
	//Try to open the Serial Port
	try
	{
		open(m_szSerialPort, 57600); //ECP3 open with 57600
		Log(LOG_STATUS,"Using serial port: %s", m_szSerialPort.c_str());
	}
	catch (boost::exception & e)
	{
		Log(LOG_ERROR,"Error opening serial port!");
#ifdef _DEBUG
		Log(LOG_ERROR,"-----------------\n%s\n----------------", boost::diagnostic_information(e).c_str());
#else
		(void)e;
#endif
		return false;
	}
	catch ( ... )
	{
		Log(LOG_ERROR,"Error opening serial port!!!");
		return false;
	}
	m_bIsStarted=true;

	m_receivestate=ERS_SYNCBYTE;
	setReadCallback([this](auto d, auto l) { readCallback(d, l); });
	sOnConnected(this);

	uint8_t buf[100];

	//Request BASE_ID
	m_bBaseIDRequested=true;
	buf[0] = CO_RD_IDBASE;
	sendFrameQueue(PACKET_COMMON_COMMAND, buf, 1, nullptr, 0);

	//Request Version
	buf[0] = CO_RD_VERSION;
	sendFrameQueue(PACKET_COMMON_COMMAND, buf, 1, nullptr, 0);

	return true;
}

void CEnOceanESP3::readCallback(const char *data, size_t len)
{
	size_t ii=0;

	while (ii<len)
	{
		const unsigned char c = data[ii];

		switch (m_receivestate)
		{
		case ERS_SYNCBYTE:
			if (c!=0x55)
				return;
			m_receivestate = ERS_HEADER;
			m_bufferpos=0;
			break;
		case ERS_HEADER:
			m_buffer[m_bufferpos++]=c;
			if (m_bufferpos==5)
			{
				m_DataSize=(m_buffer[0]<<8)|m_buffer[1];
				m_OptionalDataSize=m_buffer[2];
				m_ReceivedPacketType=m_buffer[3];
				unsigned char CRCH=m_buffer[4];

				unsigned char crc=0;
				crc = proc_crc8(crc, m_buffer[0]);
				crc = proc_crc8(crc, m_buffer[1]);
				crc = proc_crc8(crc, m_buffer[2]);
				crc = proc_crc8(crc, m_buffer[3]);

				if (CRCH==crc)
				{
					m_bufferpos=0;
					m_wantedlength=m_DataSize+m_OptionalDataSize;
					m_receivestate = ERS_DATA;
				}
				else
				{
					Log(LOG_ERROR,"Frame Checksum Error!...");
					m_receivestate = ERS_SYNCBYTE;
				}
			}
			break;
		case ERS_DATA:
			m_buffer[m_bufferpos++] = c;
			if (m_bufferpos>=m_wantedlength)
			{
				m_receivestate = ERS_CHECKSUM;
			}
			break;
		case ERS_CHECKSUM:
			{
				unsigned char CRCD=c;
				unsigned char crc=0;
				for (int iCheck=0; iCheck<m_wantedlength; iCheck++)
					crc = proc_crc8(crc, m_buffer[iCheck]);
				if (CRCD==crc)
				{
					ParseData();
				}
				m_receivestate = ERS_SYNCBYTE;
			}
			break;
		}
		ii++;
	}

}

bool CEnOceanESP3::WriteToHardware(const char *pdata, const unsigned char /*length*/)
{
	if (m_id_base==0)
		return false;
	if (!isOpen())
		return false;
	RBUF *tsen=(RBUF*)pdata;
	if (tsen->LIGHTING2.packettype!=pTypeLighting2)
		return false; //only allowed to control switches

	uint32_t iNodeID = GetINodeID(tsen->LIGHTING2.id1, tsen->LIGHTING2.id2, tsen->LIGHTING2.id3, tsen->LIGHTING2.id4);
	if ((iNodeID <= m_id_base) || (iNodeID > m_id_base + 128))
	{
		std::string nodeID = GetNodeID(iNodeID);
		std::string baseID = GetNodeID(m_id_base);
		Log(LOG_ERROR,"EnOcean: Can not switch with ID %s, use a switch created with base ID %s!...", nodeID.c_str(), baseID.c_str());
		return false;
	}

	unsigned char RockerID=0;
	unsigned char Pressed=1;

	if (tsen->LIGHTING2.unitcode < 10)
	{
		RockerID = tsen->LIGHTING2.unitcode - 1;
	}
	else
		return false;//double not supported yet!


	//First we need to find out if this is a Dimmer switch,
	//because they are threaded differently
	bool bIsDimmer=false;
	uint8_t LastLevel=0;
	std::string nodeID = GetNodeID(iNodeID);
	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT SwitchType,LastLevel FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q') AND (Unit==%d)", m_HwdID, nodeID.c_str(), int(tsen->LIGHTING2.unitcode));
	if (!result.empty())
	{
		_eSwitchType switchtype=(_eSwitchType)atoi(result[0][0].c_str());
		if (switchtype==STYPE_Dimmer)
			bIsDimmer=true;
		LastLevel=(uint8_t)atoi(result[0][1].c_str());
	}

	uint8_t iLevel=tsen->LIGHTING2.level;
	int cmnd=tsen->LIGHTING2.cmnd;
	int orgcmd=cmnd;
	if ((tsen->LIGHTING2.level==0)&&(!bIsDimmer))
		cmnd=light2_sOff;
	else
	{
		if (cmnd==light2_sOn)
		{
			iLevel=LastLevel;
		}
		else
		{
			//scale to 0 - 100 %
			iLevel=tsen->LIGHTING2.level;
			if (iLevel>15)
				iLevel=15;
			float fLevel = (100.0F / 15.0F) * float(iLevel);
			if (fLevel > 99.0F)
				fLevel = 100.0F;
			iLevel=(uint8_t)(fLevel);
		}
		cmnd=light2_sSetLevel;
	}

	//char buff[512];
	//sprintf(buff,"cmnd: %d, level: %d, orgcmd: %d",cmnd, iLevel, orgcmd);
	//Log(LOG_ERROR,buff);
	unsigned char buf[100];
	//unsigned char optbuf[100];

	if(!bIsDimmer)
	{
		// on/off switch without dimming capability: Profile F6-02-01
		// cf. EnOcean Equipment Profiles v2.6.5 page 11 (RPS format) & 14
		unsigned char UpDown = 1;

		buf[0] = RORG_RPS;

		UpDown = ((orgcmd != light2_sOff) && (orgcmd != light2_sGroupOff));

		switch(RockerID)
		{
			case 0:	// Button A
						if(UpDown)
							buf[1] = F60201_BUTTON_A1 << F60201_R1_SHIFT;
						else
							buf[1] = F60201_BUTTON_A0 << F60201_R1_SHIFT;
						break;

			case 1:	// Button B
						if(UpDown)
							buf[1] = F60201_BUTTON_B1 << F60201_R1_SHIFT;
						else
							buf[1] = F60201_BUTTON_B0 << F60201_R1_SHIFT;
						break;

			default:
						return false;	// not supported
		}

		buf[1] |= F60201_EB_MASK;		// button is pressed

		buf[2]=(iNodeID >> 24) & 0xFF;		// Sender ID
		buf[3]=(iNodeID >> 16) & 0xFF;
		buf[4]=(iNodeID >> 8) & 0xFF;
		buf[5]=iNodeID & 0xFF;

		buf[6] = S_RPS_T21|S_RPS_NU;	// press button			// b5=T21, b4=NU, b3-b0= RepeaterCount

		//char buff[512];
		//sprintf(buff,"%02X %02X %02X %02X %02X %02X %02X",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6]);
		//Log(LOG_ERROR,buff);

		sendFrameQueue(PACKET_RADIO_ERP1, buf, 7, nullptr, 0);

		//Next command is send a bit later (button release)
		buf[1] = 0;				// no button press
		buf[6] = S_RPS_T21;	// release button			// b5=T21, b4=NU, b3-b0= RepeaterCount
		//sprintf(buff,"%02X %02X %02X %02X %02X %02X %02X",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6]);
		//Log(LOG_ERROR,buff);

		sendFrameQueue(PACKET_RADIO_ERP1, buf, 7, nullptr, 0);
	}
	else
	{
		// on/off switch with dimming capability: Profile A5-38-02
		// cf. EnOcean Equipment Profiles v2.6.5 page 12 (4BS format) & 103
		buf[0]=0xa5;
		buf[1]=0x2;
		buf[2]=100;	//level
		buf[3]=1;	//speed
		buf[4]=0x09; // Dim Off

		buf[5]=(iNodeID >> 24) & 0xFF;
		buf[6]=(iNodeID >> 16) & 0xFF;
		buf[7]=(iNodeID >> 8) & 0xFF;
		buf[8]=iNodeID & 0xFF;

		buf[9]=0x30; // status

		if (cmnd!=light2_sSetLevel)
		{
			//On/Off
			unsigned char UpDown = 1;
			UpDown = ((cmnd != light2_sOff) && (cmnd != light2_sGroupOff));

			buf[1] = (RockerID<<DB3_RPS_NU_RID_SHIFT) | (UpDown<<DB3_RPS_NU_UD_SHIFT) | (Pressed<<DB3_RPS_NU_PR_SHIFT);//0x30;
			buf[9] = 0x30;

			sendFrameQueue(PACKET_RADIO_ERP1, buf, 10, nullptr, 0);

			//char buff[512];
			//sprintf(buff,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9]);
			//Log(LOG_ERROR,buff);

			//Next command is send a bit later (button release)
			buf[1] = 0;
			buf[9] = 0x20;
			//sprintf(buff,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9]);
			//Log(LOG_ERROR,buff);
			sendFrameQueue(PACKET_RADIO_ERP1, buf, 10, nullptr, 0);
		}
		else
		{
			//Send dim value

			//Dim On DATA_BYTE0 = 0x09
			//Dim Off DATA_BYTE0 = 0x08

			buf[1]=2;
			buf[2]=iLevel;
			buf[3]=1;//very fast dimming

			if ((iLevel==0)||(orgcmd==light2_sOff))
				buf[4]=0x08; //Dim Off
			else
				buf[4]=0x09;//Dim On

			sendFrameQueue(PACKET_RADIO_ERP1, buf, 10, nullptr, 0);
		}
	}

	return true;
}

void CEnOceanESP3::SendDimmerTeachIn(const char *pdata, const unsigned char /*length*/)
{
	if (m_id_base==0)
		return;
	if (isOpen()) {
		RBUF *tsen = (RBUF*)pdata;
		if (tsen->LIGHTING2.packettype != pTypeLighting2)
			return; //only allowed to control switches
		uint32_t iNodeID = GetINodeID(tsen->LIGHTING2.id1, tsen->LIGHTING2.id2, tsen->LIGHTING2.id3, tsen->LIGHTING2.id4);
		if ((iNodeID <= m_id_base) || (iNodeID > m_id_base + 128))
		{
			std::string nodeID = GetNodeID(iNodeID);
			std::string baseID = GetNodeID(m_id_base);
			Log(LOG_ERROR,"EnOcean: Can not switch with ID %s, use a switch created with base ID %s!...", nodeID.c_str(), baseID.c_str());
			return;
		}

		unsigned char buf[100];
		buf[0] = 0xa5;
		buf[1] = 0x2;
		buf[2] = 0;
		buf[3] = 0;
		buf[4] = 0x0; // DB0.3=0 -> teach in

		buf[5] = (iNodeID >> 24) & 0xFF;
		buf[6] = (iNodeID >> 16) & 0xFF;
		buf[7] = (iNodeID >> 8) & 0xFF;
		buf[8] = iNodeID & 0xFF;

		buf[9] = 0x30; // status

		if (tsen->LIGHTING2.unitcode < 10)
		{
			unsigned char RockerID = 0;
			//unsigned char UpDown = 1;
			//unsigned char Pressed = 1;
			RockerID = tsen->LIGHTING2.unitcode - 1;
		}
		else
		{
			return;//double not supported yet!
		}
		sendFrame(PACKET_RADIO_ERP1, buf, 10, nullptr, 0);
	}
}

bool CEnOceanESP3::ParseData()
{
#ifdef ENABLE_LOGGING
	std::stringstream sstr;

	sstr << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (unsigned int)m_ReceivedPacketType << " (";
	sstr << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (unsigned int)m_DataSize << "/";
	sstr << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (unsigned int)m_OptionalDataSize << ") ";

	for (int idx=0;idx<m_bufferpos;idx++)
	{
		sstr << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (unsigned int)m_buffer[idx];
		if (idx!=m_bufferpos-1)
			sstr << " ";
	}
	Log(LOG_STATUS,"%s",sstr.str().c_str());
#endif

	if (m_ReceivedPacketType==PACKET_RESPONSE)
	{
		//Response
		unsigned char ResponseCode=m_buffer[0];
		if (ResponseCode!=0)
		{
			std::string szError="Unknown?";
			switch (ResponseCode)
			{
			case RET_ERROR:
				szError="RET_ERROR";
				break;
			case RET_NOT_SUPPORTED:
				szError="RET_NOT_SUPPORTED";
				break;
			case RET_WRONG_PARAM:
				szError="RET_WRONG_PARAM";
				break;
			case RET_OPERATION_DENIED:
				szError="RET_OPERATION_DENIED";
				break;
			}
			Log(LOG_ERROR,"Response Error (Code: %d, %s)",ResponseCode,szError.c_str());
			return false;
		}
		if ((m_bBaseIDRequested)&&(m_bufferpos==6))
		{
			m_bBaseIDRequested=false;
			m_id_base = GetINodeID(m_buffer[1], m_buffer[2], m_buffer[3], m_buffer[4]);
			//unsigned char changes_left=m_buffer[5];
			Log(LOG_STATUS,"Transceiver ID_Base: %08X",m_id_base);
		}
		if (m_bufferpos==33)
		{
			//Version Information
			Log(LOG_STATUS,"Version_Info, App: %02X.%02X.%02X.%02X, API: %02X.%02X.%02X.%02X, ChipID: %02X.%02X.%02X.%02X, ChipVersion: %02X.%02X.%02X.%02X, Description: %s",
				m_buffer[1],m_buffer[2],m_buffer[3],m_buffer[4],
				m_buffer[5],m_buffer[6],m_buffer[7],m_buffer[8],
				m_buffer[9],m_buffer[10],m_buffer[11],m_buffer[12],
				m_buffer[13],m_buffer[14],m_buffer[15],m_buffer[16],
				(const char*)&m_buffer+17
				);
		}
		return true;
	}
	if (m_ReceivedPacketType == PACKET_RADIO_ERP1)
		ParseRadioDatagram();
	else
	{
		char szTmp[100];
		sprintf(szTmp,"Unhandled Packet Type (%02X)",m_ReceivedPacketType);
		Log(LOG_STATUS, "%s", szTmp);
	}
	/*
		enocean_data_structure *pFrame=(enocean_data_structure*)&m_buffer;
		unsigned char Checksum=enocean_calc_checksum(pFrame);
		if (Checksum!=pFrame->CHECKSUM)
			return false; //checksum Mismatch!

		uint32_t iNodeID = GetINodeID(pFrame->ID_BYTE3, pFrame->ID_BYTE2, pFrame->ID_BYTE1, pFrame->ID_BYTE0);

		//Handle possible OK/Errors
		bool bStopProcessing=false;
		if (pFrame->H_SEQ_LENGTH==0x8B)
		{
			switch (pFrame->ORG)
			{
			case 0x58:
				//OK
	#ifdef _DEBUG
				Log(LOG_NORM,"OK");
	#endif
				bStopProcessing=true;
				break;
			case 0x28:
				Log(LOG_ERROR,"ERR_MODEM_NOTWANTEDACK");
				bStopProcessing=true;
				break;
			case 0x29:
				Log(LOG_ERROR,"ERR_MODEM_NOTACK");
				bStopProcessing=true;
				break;
			case 0x0C:
				Log(LOG_ERROR,"ERR_MODEM_DUP_ID");
				bStopProcessing=true;
				break;
			case 0x08:
				Log(LOG_ERROR,"Error in H_SEQ");
				bStopProcessing=true;
				break;
			case 0x09:
				Log(LOG_ERROR,"Error in LENGTH");
				bStopProcessing=true;
				break;
			case 0x0A:
				Log(LOG_ERROR,"Error in CHECKSUM");
				bStopProcessing=true;
				break;
			case 0x0B:
				Log(LOG_ERROR,"Error in ORG");
				bStopProcessing=true;
				break;
			case 0x22:
				Log(LOG_ERROR,"ERR_TX_IDRANGE");
				bStopProcessing=true;
				break;
			case 0x1A:
				Log(LOG_ERROR,"ERR_ IDRANGE");
				bStopProcessing=true;
				break;
			}
		}
		if (bStopProcessing)
			return true;

		switch (pFrame->ORG)
		{
		case C_ORG_INF_IDBASE:
			m_id_base = (pFrame->DATA_BYTE3 << 24) + (pFrame->DATA_BYTE2 << 16) + (pFrame->DATA_BYTE1 << 8) + pFrame->DATA_BYTE0;
			Log(LOG_STATUS,"Transceiver ID_Base: %08X",m_id_base);
			break;
		
		case C_ORG_RPS:
			if (pFrame->STATUS & S_RPS_NU) {
				//Rocker
				// NU == 1, N-Message
				unsigned char RockerID=(pFrame->DATA_BYTE3 & DB3_RPS_NU_RID) >> DB3_RPS_NU_RID_SHIFT;
				unsigned char UpDown=(pFrame->DATA_BYTE3 & DB3_RPS_NU_UD) >> DB3_RPS_NU_UD_SHIFT;
				unsigned char Pressed=(pFrame->DATA_BYTE3 & DB3_RPS_NU_PR)>>DB3_RPS_NU_PR_SHIFT;
				unsigned char SecondRockerID=(pFrame->DATA_BYTE3 & DB3_RPS_NU_SRID)>>DB3_RPS_NU_SRID_SHIFT;
				unsigned char SecondUpDown=(pFrame->DATA_BYTE3 & DB3_RPS_NU_SUD)>>DB3_RPS_NU_SUD_SHIFT;
				unsigned char SecondAction=(pFrame->DATA_BYTE3 & DB3_RPS_NU_SA)>>DB3_RPS_NU_SA_SHIFT;
	#ifdef _DEBUG
				Log(LOG_NORM,"Received RPS N-Message Node %08x Rocker ID: %i UD: %i Pressed: %i Second Rocker ID: %i SUD: %i Second Action: %i",
					iNodeID, RockerID, UpDown, Pressed, SecondRockerID, SecondUpDown, SecondAction);
	#endif
				//We distinguish 3 types of buttons from a switch: Left/Right/Left+Right
				if (Pressed==1)
				{
					RBUF tsen;
					memset(&tsen,0,sizeof(RBUF));
					tsen.LIGHTING2.packetlength=sizeof(tsen.LIGHTING2)-1;
					tsen.LIGHTING2.packettype=pTypeLighting2;
					tsen.LIGHTING2.subtype=sTypeAC;
					tsen.LIGHTING2.seqnbr=0;
					tsen.LIGHTING2.id1=(BYTE)pFrame->ID_BYTE3;
					tsen.LIGHTING2.id2=(BYTE)pFrame->ID_BYTE2;
					tsen.LIGHTING2.id3=(BYTE)pFrame->ID_BYTE1;
					tsen.LIGHTING2.id4=(BYTE)pFrame->ID_BYTE0;
					tsen.LIGHTING2.level=0;
					tsen.LIGHTING2.rssi=12;

					if (SecondAction==0)
					{
						//Left/Right Up/Down
						tsen.LIGHTING2.unitcode=RockerID+1;
						tsen.LIGHTING2.cmnd=(UpDown==1)?light2_sOn:light2_sOff;
					}
					else
					{
						//Left+Right Up/Down
						tsen.LIGHTING2.unitcode=SecondRockerID+10;
						tsen.LIGHTING2.cmnd=(SecondUpDown==1)?light2_sOn:light2_sOff;
					}
					sDecodeRXMessage(this, (const unsigned char *)&tsen.LIGHTING2, nullptr, 255, m_Name.c_str());
				}
			}
			break;
		case C_ORG_4BS:
			break;
		default:
			{
				char *pszHumenTxt=enocean_hexToHuman(pFrame);
				if (pszHumenTxt)
				{
					Log(LOG_NORM, "%s", pszHumenTxt);
					free(pszHumenTxt);
				}
			}
			break;
		}
	*/
	return true;
}

void CEnOceanESP3::ParseRadioDatagram()
{
	char szTmp[100];
	int rssi = 12;  // RSSI for Domoticz
	// normal value is between 0 (very weak) and 11 (strong)
	// 12 = no RSSI value in device list
	if (m_OptionalDataSize == 7)
	{
		int rssi_dbm = m_buffer[m_DataSize+5] * -1;  // RSSI reported by Enocean Dongle in dBm
		// convert RSSI dBm to RSSI Domoticz
		// this is not the best conversion algo
		// but, according to my tests, it's a good start
		if (rssi_dbm > -50) {
			rssi = 11;
		}
		else if (rssi_dbm < -100) {
			rssi = 0;
		}
		else {
			rssi = static_cast<int>((rssi_dbm + 100) / 5);
		}
		sprintf(szTmp,"destination: %02X%02X%02X%02X RSSI: %i dBm (%i/11)",
			m_buffer[m_DataSize+1],m_buffer[m_DataSize+2],m_buffer[m_DataSize+3],m_buffer[m_DataSize+4],rssi_dbm,rssi
			);

	}
	else {
		sprintf(szTmp, "Optional data size: %i",m_OptionalDataSize);
	}
	Log(LOG_NORM, "%s", szTmp);
	switch (m_buffer[0])
	{
		case RORG_1BS: // 1 byte communication (Contacts/Switches)
			{
				sprintf(szTmp,"1BS data: Sender id: %02X%02X%02X%02X Data: %02X",
					m_buffer[2],m_buffer[3],m_buffer[4],m_buffer[5],
					m_buffer[0]
				);

				Log(LOG_NORM, "%s", szTmp);

				unsigned char DATA_BYTE0 = m_buffer[1];

				unsigned char ID_BYTE3  = m_buffer[2];
				unsigned char ID_BYTE2  = m_buffer[3];
				unsigned char ID_BYTE1  = m_buffer[4];
				unsigned char ID_BYTE0  = m_buffer[5];

				int UpDown=(DATA_BYTE0&1)==0;

				RBUF tsen;
				memset(&tsen,0,sizeof(RBUF));
				tsen.LIGHTING2.packetlength=sizeof(tsen.LIGHTING2)-1;
				tsen.LIGHTING2.packettype=pTypeLighting2;
				tsen.LIGHTING2.subtype=sTypeAC;
				tsen.LIGHTING2.seqnbr=0;
				tsen.LIGHTING2.id1=(BYTE)ID_BYTE3;
				tsen.LIGHTING2.id2=(BYTE)ID_BYTE2;
				tsen.LIGHTING2.id3=(BYTE)ID_BYTE1;
				tsen.LIGHTING2.id4=(BYTE)ID_BYTE0;
				tsen.LIGHTING2.level=0;
				tsen.LIGHTING2.rssi=rssi;
				tsen.LIGHTING2.unitcode=1;
				tsen.LIGHTING2.cmnd=(UpDown==1)?light2_sOn:light2_sOff;
				sDecodeRXMessage(this, (const unsigned char *)&tsen.LIGHTING2, nullptr, 255, m_Name.c_str());
			}
			break;
		case RORG_4BS: // 4 byte communication
			{
				sprintf(szTmp,"4BS data: Sender id: %02X%02X%02X%02X Status: %02X Data: %02X",
					m_buffer[5],m_buffer[6],m_buffer[7],m_buffer[8],
					m_buffer[9],
					m_buffer[3]
				);
				Log(LOG_NORM, "%s", szTmp);

				unsigned char DATA_BYTE3 = m_buffer[1];
				unsigned char DATA_BYTE2 = m_buffer[2];
				unsigned char DATA_BYTE1 = m_buffer[3];
				unsigned char DATA_BYTE0 = m_buffer[4];

				unsigned char ID_BYTE3  = m_buffer[5];
				unsigned char ID_BYTE2  = m_buffer[6];
				unsigned char ID_BYTE1  = m_buffer[7];
				unsigned char ID_BYTE0  = m_buffer[8];

				uint32_t iNodeID = GetINodeID(ID_BYTE3, ID_BYTE2, ID_BYTE1, ID_BYTE0);
				std::string nodeID = GetNodeID(iNodeID);

				if ((DATA_BYTE0 & RORG_4BS_TEACHIN_LRN_BIT) == 0)	// LRN_BIT is 0 -> Teach-in datagram
				{
					int manufacturer;
					int profile;
					int ttype;

					// 2016-01-31 Stéphane Guillard : added handling of this case:
					if ((DATA_BYTE0 & RORG_4BS_TEACHIN_EEP_BIT) == 0)
					{
						// RORG_4BS_TEACHIN_EEP_BIT is 0 -> Teach-in Variant 1 : data doesn't contain EEP and Manufacturer ID
						// An EEP profile must be manually allocated per sender ID (see EEP 2.6.2 specification §3.3 p173/197)
						Log(LOG_NORM, "4BS, Variant 1 Teach-in diagram: Sender_ID: %08X", iNodeID);
						Log(LOG_NORM, "Teach-in data contains no EEP profile. Created generic A5-02-05 profile (0/40°C temp sensor); please adjust by hand using Setup button on EnOcean adapter in Setup/Hardware menu");

						manufacturer = 0x7FF;			// Generic
						profile = 0x02;					// == T4BSTable[4].Func: Temperature Sensor Range 0C to +40C
						ttype = 0x05;						// == T4BSTable[4].Type
					}
					else
					{
						// RORG_4BS_TEACHIN_EEP_BIT is 1 -> Teach-in Variant 2 : data contains EEP and Manufacturer ID

						//DB3		DB3/2	DB2/1			DB0
						//Profile	Type	Manufacturer-ID	RORG_4BS_TEACHIN_EEP_BIT		RE2		RE1				RORG_4BS_TEACHIN_LRN_BIT
						//6 Bit		7 Bit	11 Bit			1Bit		1Bit	1Bit	1Bit	1Bit	1Bit	1Bit	1Bit

						// Extract manufacturer, profile and type from data
						manufacturer = ((DATA_BYTE2 & 7) << 8) | DATA_BYTE1;
						profile = DATA_BYTE3 >> 2;
						ttype = ((DATA_BYTE3 & 0x03) << 5) | (DATA_BYTE2 >> 3);

						Log(LOG_NORM,"4BS, Variant 2 Teach-in diagram: Sender_ID: %08X\nManufacturer: %03X (%s)\nProfile: %02X\nType: %02X (%s)",
							iNodeID, manufacturer,GetManufacturerName(manufacturer),
							profile, ttype, GetEEPLabel(RORG_4BS, profile, ttype));
 					}

					// Search the sensor in database
					std::vector<std::vector<std::string> > result;
					result = m_sql.safe_query("SELECT ID FROM EnoceanSensors WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, nodeID.c_str());
					if (result.empty())
					{
						// If not found, add it to the database
						m_sql.safe_query("INSERT INTO EnoceanSensors (HardwareID, DeviceID, Manufacturer, Profile, [Type]) VALUES (%d,'%q',%d,%d,%d)", m_HwdID, nodeID.c_str(), manufacturer, profile, ttype);
						Log(LOG_NORM, "Sender_ID %08X inserted in the database", iNodeID);
					}
					else
						Log(LOG_NORM, "Sender_ID %08X already in the database", iNodeID);
					ReloadVLDNodes();
				}
				else	// RORG_4BS_TEACHIN_LRN_BIT is 1 -> Data datagram
				{
					//Following sensors need to have had a teach-in
					std::vector<std::vector<std::string> > result;
					result = m_sql.safe_query("SELECT ID, Manufacturer, Profile, [Type] FROM EnoceanSensors WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, nodeID.c_str());
					if (result.empty())
					{
						Log(LOG_NORM, "Need Teach-In for %s", nodeID.c_str());
						return;
					}
					int Manufacturer=atoi(result[0][1].c_str());
					int Profile=atoi(result[0][2].c_str());
					int iType=atoi(result[0][3].c_str());

					if (Profile == 0x12 && iType == 0x00)
					{ // A5-12-00, Automated Meter Reading, Counter
						unsigned long cvalue=(DATA_BYTE3<<16)|(DATA_BYTE2<<8)|(DATA_BYTE1);
						RBUF tsen;
						memset(&tsen,0,sizeof(RBUF));
						tsen.RFXMETER.packetlength=sizeof(tsen.RFXMETER)-1;
						tsen.RFXMETER.packettype=pTypeRFXMeter;
						tsen.RFXMETER.subtype=sTypeRFXMeterCount;
						tsen.RFXMETER.rssi=rssi;
						tsen.RFXMETER.id1=ID_BYTE2;
						tsen.RFXMETER.id2=ID_BYTE1;
						tsen.RFXMETER.count1 = (BYTE)((cvalue & 0xFF000000) >> 24);
						tsen.RFXMETER.count2 = (BYTE)((cvalue & 0x00FF0000) >> 16);
						tsen.RFXMETER.count3 = (BYTE)((cvalue & 0x0000FF00) >> 8);
						tsen.RFXMETER.count4 = (BYTE)(cvalue & 0x000000FF);
						sDecodeRXMessage(this, (const unsigned char *)&tsen.RFXMETER, nullptr, 255, nullptr);
					}
					else if (Profile == 0x12 && iType == 0x01)
					{ // A5-12-01, Automated Meter Reading, Electricity
						int cvalue=(DATA_BYTE3<<16)|(DATA_BYTE2<<8)|(DATA_BYTE1);
						_tUsageMeter umeter;
						umeter.id1=(BYTE)ID_BYTE3;
						umeter.id2=(BYTE)ID_BYTE2;
						umeter.id3=(BYTE)ID_BYTE1;
						umeter.id4=(BYTE)ID_BYTE0;
						umeter.dunit=1;
						umeter.fusage=(float)cvalue;
						sDecodeRXMessage(this, (const unsigned char *)&umeter, nullptr, 255, nullptr);
					}
					else if (Profile == 0x12 && iType == 0x02)
					{ // A5-12-02, Automated Meter Reading, Gas
						unsigned long cvalue=(DATA_BYTE3<<16)|(DATA_BYTE2<<8)|(DATA_BYTE1);
						RBUF tsen;
						memset(&tsen,0,sizeof(RBUF));
						tsen.RFXMETER.packetlength=sizeof(tsen.RFXMETER)-1;
						tsen.RFXMETER.packettype=pTypeRFXMeter;
						tsen.RFXMETER.subtype=sTypeRFXMeterCount;
						tsen.RFXMETER.rssi=rssi;
						tsen.RFXMETER.id1=ID_BYTE2;
						tsen.RFXMETER.id2=ID_BYTE1;
						tsen.RFXMETER.count1 = (BYTE)((cvalue & 0xFF000000) >> 24);
						tsen.RFXMETER.count2 = (BYTE)((cvalue & 0x00FF0000) >> 16);
						tsen.RFXMETER.count3 = (BYTE)((cvalue & 0x0000FF00) >> 8);
						tsen.RFXMETER.count4 = (BYTE)(cvalue & 0x000000FF);
						sDecodeRXMessage(this, (const unsigned char *)&tsen.RFXMETER, nullptr, 255, nullptr);
					}
					else if (Profile == 0x12 && iType == 0x03)
					{ // A5-12-03, Automated Meter Reading, Water
						unsigned long cvalue=(DATA_BYTE3<<16)|(DATA_BYTE2<<8)|(DATA_BYTE1);
						RBUF tsen;
						memset(&tsen,0,sizeof(RBUF));
						tsen.RFXMETER.packetlength=sizeof(tsen.RFXMETER)-1;
						tsen.RFXMETER.packettype=pTypeRFXMeter;
						tsen.RFXMETER.subtype=sTypeRFXMeterCount;
						tsen.RFXMETER.rssi=rssi;
						tsen.RFXMETER.id1=ID_BYTE2;
						tsen.RFXMETER.id2=ID_BYTE1;
						tsen.RFXMETER.count1 = (BYTE)((cvalue & 0xFF000000) >> 24);
						tsen.RFXMETER.count2 = (BYTE)((cvalue & 0x00FF0000) >> 16);
						tsen.RFXMETER.count3 = (BYTE)((cvalue & 0x0000FF00) >> 8);
						tsen.RFXMETER.count4 = (BYTE)(cvalue & 0x000000FF);
						sDecodeRXMessage(this, (const unsigned char *)&tsen.RFXMETER, nullptr, 255, nullptr);
					}
					else if (Profile == 0x10 && iType <= 0x0D)
					{ // A5-10-01..OD, RoomOperatingPanel
						// Room Sensor and Control Unit (EEP A5-10-01 ... A5-10-0D)
						// [Eltako FTR55D, FTR55H, Thermokon SR04 *, Thanos SR *, untested]
						// DATA_BYTE3 is the fan speed or night reduction for Eltako
						// DATA_BYTE2 is the setpoint where 0x00 = min ... 0xFF = max or
						// reference temperature for Eltako where 0x00 = 0°C ... 0xFF = 40°C
						// DATA_BYTE1 is the temperature where 0x00 = +40°C ... 0xFF = 0°C
						// DATA_BYTE0_bit_0 is the occupy button, pushbutton or slide switch
						float temp = GetDeviceValue(DATA_BYTE1, 0, 255, 40, 0);
						if (Manufacturer == ELTAKO)
						{
							int nightReduction = 0;
							if (DATA_BYTE3 == 0x06)
								nightReduction = 1;
							else if (DATA_BYTE3 == 0x0C)
								nightReduction = 2;
							else if (DATA_BYTE3 == 0x13)
								nightReduction = 3;
							else if (DATA_BYTE3 == 0x19)
								nightReduction = 4;
							else if (DATA_BYTE3 == 0x1F)
								nightReduction = 5;
							//float setpointTemp=GetDeviceValue(DATA_BYTE2, 0, 255, 0, 40);
						}
						else
						{
							int fspeed = 3;
							if (DATA_BYTE3 >= 145)
								fspeed = 2;
							else if (DATA_BYTE3 >= 165)
								fspeed = 1;
							else if (DATA_BYTE3 >= 190)
								fspeed = 0;
							else if (DATA_BYTE3 >= 210)
								fspeed = -1; //auto
							//int iswitch = DATA_BYTE0 & 1;
						}
						RBUF tsen;
						memset(&tsen,0,sizeof(RBUF));
						tsen.TEMP.packetlength=sizeof(tsen.TEMP)-1;
						tsen.TEMP.packettype=pTypeTEMP;
						tsen.TEMP.subtype=sTypeTEMP10;
						tsen.TEMP.id1=ID_BYTE2;
						tsen.TEMP.id2=ID_BYTE1;
						// WARNING
						// battery_level & rssi fields are used here to transmit ID_BYTE0 value to decode_Temp in mainworker.cpp
						// decode_Temp assumes battery_level = 255 (Unknown) & rssi = 12 (Not available)
						tsen.TEMP.battery_level=ID_BYTE0&0x0F;
						tsen.TEMP.rssi=(ID_BYTE0&0xF0)>>4;
						tsen.TEMP.tempsign=(temp>=0)?0:1;
						int at10 = round(std::abs(temp * 10.0F));
						tsen.TEMP.temperatureh=(BYTE)(at10/256);
						at10-=(tsen.TEMP.temperatureh*256);
						tsen.TEMP.temperaturel=(BYTE)(at10);
						sDecodeRXMessage(this, (const unsigned char *)&tsen.TEMP, nullptr, -1, nullptr);
					}
					else if (Profile == 0x06 && iType == 0x01)
					{ // A5-06-01, Light Sensor
						// [Eltako FAH60, FAH63, FIH63, Thermokon SR65 LI, untested]
						// DATA_BYTE3 is the voltage where 0x00 = 0 V ... 0xFF = 5.1 V
						// DATA_BYTE3 is the low illuminance for Eltako devices where
						// min 0x00 = 0 lx, max 0xFF = 100 lx, if DATA_BYTE2 = 0
						// DATA_BYTE2 is the illuminance (ILL2) where min 0x00 = 300 lx, max 0xFF = 30000 lx
						// DATA_BYTE1 is the illuminance (ILL1) where min 0x00 = 600 lx, max 0xFF = 60000 lx
						// DATA_BYTE0_bit_0 is Range select where 0 = ILL1, 1 = ILL2
						float lux =0;
						if (Manufacturer == ELTAKO)
						{
							if(DATA_BYTE2 == 0)
								lux = GetDeviceValue(DATA_BYTE3, 0, 255, 0, 100);
							else
								lux = GetDeviceValue(DATA_BYTE2, 0, 255, 300, 30000);
						}
						else
						{
							float voltage = GetDeviceValue(DATA_BYTE3, 0, 255, 0, 5100); // need to convert value from V to mV
							if(DATA_BYTE0 & 1)
								lux = GetDeviceValue(DATA_BYTE2, 0, 255, 300, 30000);
							else
								lux = GetDeviceValue(DATA_BYTE1, 0, 255, 600, 60000);

							RBUF tsen;
							memset(&tsen,0,sizeof(RBUF));
							tsen.RFXSENSOR.packetlength=sizeof(tsen.RFXSENSOR)-1;
							tsen.RFXSENSOR.packettype=pTypeRFXSensor;
							tsen.RFXSENSOR.subtype=sTypeRFXSensorVolt;
							tsen.RFXSENSOR.id=ID_BYTE1;
							// WARNING
							// filler & rssi fields are used here to transmit ID_BYTE0 value to decode_RFXSensor in mainworker.cpp
							// decode_RFXSensor sets BatteryLevel to 255 (Unknown) and rssi to 12 (Not available)
							tsen.RFXSENSOR.filler=ID_BYTE0&0x0F;
							tsen.RFXSENSOR.rssi=(ID_BYTE0&0xF0)>>4;
							tsen.RFXSENSOR.msg1 = (BYTE)(voltage/256);
							tsen.RFXSENSOR.msg2 = (BYTE)(voltage-(tsen.RFXSENSOR.msg1*256));
							sDecodeRXMessage(this, (const unsigned char *)&tsen.RFXSENSOR, nullptr, 255, nullptr);
						}
						_tLightMeter lmeter;
						lmeter.id1=(BYTE)ID_BYTE3;
						lmeter.id2=(BYTE)ID_BYTE2;
						lmeter.id3=(BYTE)ID_BYTE1;
						lmeter.id4=(BYTE)ID_BYTE0;
						lmeter.dunit=1;
						lmeter.fLux=lux;
						sDecodeRXMessage(this, (const unsigned char *)&lmeter, nullptr, 255, nullptr);
					}
					else if (Profile == 0x02)
					{ // A5-02-01..30, Temperature sensor
						float ScaleMax=0;
						float ScaleMin=0;
						if (iType==0x01) { ScaleMin=-40; ScaleMax=0; }
						else if (iType==0x02) { ScaleMin=-30; ScaleMax=10; }
						else if (iType==0x03) { ScaleMin=-20; ScaleMax=20; }
						else if (iType==0x04) { ScaleMin=-10; ScaleMax=30; }
						else if (iType==0x05) { ScaleMin=0; ScaleMax=40; }
						else if (iType==0x06) { ScaleMin=10; ScaleMax=50; }
						else if (iType==0x07) { ScaleMin=20; ScaleMax=60; }
						else if (iType==0x08) { ScaleMin=30; ScaleMax=70; }
						else if (iType==0x09) { ScaleMin=40; ScaleMax=80; }
						else if (iType==0x0A) { ScaleMin=50; ScaleMax=90; }
						else if (iType==0x0B) { ScaleMin=60; ScaleMax=100; }
						else if (iType==0x10) { ScaleMin=-60; ScaleMax=20; }
						else if (iType==0x11) { ScaleMin=-50; ScaleMax=30; }
						else if (iType==0x12) { ScaleMin=-40; ScaleMax=40; }
						else if (iType==0x13) { ScaleMin=-30; ScaleMax=50; }
						else if (iType==0x14) { ScaleMin=-20; ScaleMax=60; }
						else if (iType==0x15) { ScaleMin=-10; ScaleMax=70; }
						else if (iType==0x16) { ScaleMin=0; ScaleMax=80; }
						else if (iType==0x17) { ScaleMin=10; ScaleMax=90; }
						else if (iType==0x18) { ScaleMin=20; ScaleMax=100; }
						else if (iType==0x19) { ScaleMin=30; ScaleMax=110; }
						else if (iType==0x1A) { ScaleMin=40; ScaleMax=120; }
						else if (iType==0x1B) { ScaleMin=50; ScaleMax=130; }
						else if (iType == 0x20) { ScaleMin = -10; ScaleMax = 41.2F; }
						else if (iType == 0x30) { ScaleMin = -40; ScaleMax = 62.3F; }

						float temp;
						if (iType<0x20)
							temp = GetDeviceValue(DATA_BYTE1, 255, 0, ScaleMin, ScaleMax);
						else
							temp = GetDeviceValue(((DATA_BYTE2 & 3) << 8) | DATA_BYTE1, 0, 255, ScaleMin, ScaleMax); // 10bit
						RBUF tsen;
						memset(&tsen,0,sizeof(RBUF));
						tsen.TEMP.packetlength=sizeof(tsen.TEMP)-1;
						tsen.TEMP.packettype=pTypeTEMP;
						tsen.TEMP.subtype=sTypeTEMP10;
						tsen.TEMP.id1=ID_BYTE2;
						tsen.TEMP.id2=ID_BYTE1;
						// WARNING
						// battery_level & rssi fields are used here to transmit ID_BYTE0 value to decode_Temp in mainworker.cpp
						// decode_Temp assumes battery_level = 255 (Unknown) & rssi = 12 (Not available)
						tsen.TEMP.battery_level = ID_BYTE0 & 0x0F;
						tsen.TEMP.rssi=(ID_BYTE0&0xF0)>>4;
						tsen.TEMP.tempsign=(temp>=0)?0:1;
						int at10 = round(std::abs(temp * 10.0F));
						tsen.TEMP.temperatureh=(BYTE)(at10/256);
						at10-=(tsen.TEMP.temperatureh*256);
						tsen.TEMP.temperaturel=(BYTE)(at10);
						sDecodeRXMessage(this, (const unsigned char *)&tsen.TEMP, nullptr, -1, nullptr);
					}
					else if (Profile == 0x04)
					{ // A5-04-01..04, Temperature and Humidity Sensor
						float ScaleMax = 0;
						float ScaleMin = 0;
						if (iType == 0x01) { ScaleMin = 0; ScaleMax = 40; }
						else if (iType == 0x02) { ScaleMin = -20; ScaleMax = 60; }
						else if (iType == 0x03) { ScaleMin = -20; ScaleMax = 60; } //10bit?

						float temp = GetDeviceValue(DATA_BYTE1, 0, 250, ScaleMin, ScaleMax);
						float hum = GetDeviceValue(DATA_BYTE2, 0, 255, 0, 100);
						RBUF tsen;
						memset(&tsen,0,sizeof(RBUF));
						tsen.TEMP_HUM.packetlength=sizeof(tsen.TEMP_HUM)-1;
						tsen.TEMP_HUM.packettype=pTypeTEMP_HUM;
						tsen.TEMP_HUM.subtype=sTypeTH5;
						tsen.TEMP_HUM.rssi=rssi;
						tsen.TEMP_HUM.id1=ID_BYTE2;
						tsen.TEMP_HUM.id2=ID_BYTE1;
						tsen.TEMP_HUM.battery_level=9;
						tsen.TEMP_HUM.tempsign=(temp>=0)?0:1;
						int at10 = round(std::abs(temp * 10.0F));
						tsen.TEMP_HUM.temperatureh=(BYTE)(at10/256);
						at10-=(tsen.TEMP_HUM.temperatureh*256);
						tsen.TEMP_HUM.temperaturel=(BYTE)(at10);
						tsen.TEMP_HUM.humidity=(BYTE)hum;
						tsen.TEMP_HUM.humidity_status=Get_Humidity_Level(tsen.TEMP_HUM.humidity);
						sDecodeRXMessage(this, (const unsigned char *)&tsen.TEMP_HUM, nullptr, -1, nullptr);
					}
					else if (Profile == 0x07 && iType == 0x01)
					{ // A5-07-01, Occupancy sensor with Supply voltage monitor
						if (DATA_BYTE3 < 251)
						{
							RBUF tsen;

							if (DATA_BYTE0 & 1)
							{
								//Voltage supported
								float voltage = GetDeviceValue(DATA_BYTE3, 0, 250, 0, 5000.0F); // need to convert value from V to mV
								memset(&tsen, 0, sizeof(RBUF));
								tsen.RFXSENSOR.packetlength = sizeof(tsen.RFXSENSOR) - 1;
								tsen.RFXSENSOR.packettype = pTypeRFXSensor;
								tsen.RFXSENSOR.subtype = sTypeRFXSensorVolt;
								tsen.RFXSENSOR.id = ID_BYTE1;
								// WARNING
								// filler & rssi fields are used here to transmit ID_BYTE0 value to decode_RFXSensor in mainworker.cpp
								// decode_RFXSensor sets BatteryLevel to 255 (Unknown) and rssi to 12 (Not available)
								tsen.RFXSENSOR.filler = ID_BYTE0 & 0x0F;
								tsen.RFXSENSOR.rssi = (ID_BYTE0 & 0xF0) >> 4;
								tsen.RFXSENSOR.msg1 = (BYTE)(voltage / 256);
								tsen.RFXSENSOR.msg2 = (BYTE)(voltage - (tsen.RFXSENSOR.msg1 * 256));
								sDecodeRXMessage(this, (const unsigned char *)&tsen.RFXSENSOR, nullptr, 255, nullptr);
							}

							bool bPIROn = (DATA_BYTE1 > 127);
							memset(&tsen, 0, sizeof(RBUF));
							tsen.LIGHTING2.packetlength = sizeof(tsen.LIGHTING2) - 1;
							tsen.LIGHTING2.packettype = pTypeLighting2;
							tsen.LIGHTING2.subtype = sTypeAC;
							tsen.LIGHTING2.seqnbr = 0;
							tsen.LIGHTING2.id1 = (BYTE)ID_BYTE3;
							tsen.LIGHTING2.id2 = (BYTE)ID_BYTE2;
							tsen.LIGHTING2.id3 = (BYTE)ID_BYTE1;
							tsen.LIGHTING2.id4 = (BYTE)ID_BYTE0;
							tsen.LIGHTING2.level = 0;
							tsen.LIGHTING2.rssi = rssi;
							tsen.LIGHTING2.unitcode = 1;
							tsen.LIGHTING2.cmnd = (bPIROn) ? light2_sOn : light2_sOff;
							sDecodeRXMessage(this, (const unsigned char *)&tsen.LIGHTING2, nullptr, 255, m_Name.c_str());
						}
						else {
							//Error code
						}
					}
					else if (iType == 0x07 && iType == 0x02)
					{ // A5-07-02, , Occupancy sensor with Supply voltage monitor
						if (DATA_BYTE3 < 251)
						{
							RBUF tsen;

							float voltage = GetDeviceValue(DATA_BYTE3, 0, 250, 0, 5000.0F); // need to convert value from V to mV
							memset(&tsen, 0, sizeof(RBUF));
							tsen.RFXSENSOR.packetlength = sizeof(tsen.RFXSENSOR) - 1;
							tsen.RFXSENSOR.packettype = pTypeRFXSensor;
							tsen.RFXSENSOR.subtype = sTypeRFXSensorVolt;
							tsen.RFXSENSOR.id = ID_BYTE1;
							// WARNING
							// filler & rssi fields are used here to transmit ID_BYTE0 value to decode_RFXSensor in mainworker.cpp
							// decode_RFXSensor sets BatteryLevel to 255 (Unknown) and rssi to 12 (Not available)
							tsen.RFXSENSOR.filler = ID_BYTE0 & 0x0F;
							tsen.RFXSENSOR.rssi = (ID_BYTE0 & 0xF0) >> 4;
							tsen.RFXSENSOR.msg1 = (BYTE)(voltage / 256);
							tsen.RFXSENSOR.msg2 = (BYTE)(voltage - (tsen.RFXSENSOR.msg1 * 256));
							sDecodeRXMessage(this, (const unsigned char *)&tsen.RFXSENSOR, nullptr, 255, nullptr);

							bool bPIROn = (DATA_BYTE0 & 0x80)!=0;
							memset(&tsen, 0, sizeof(RBUF));
							tsen.LIGHTING2.packetlength = sizeof(tsen.LIGHTING2) - 1;
							tsen.LIGHTING2.packettype = pTypeLighting2;
							tsen.LIGHTING2.subtype = sTypeAC;
							tsen.LIGHTING2.seqnbr = 0;
							tsen.LIGHTING2.id1 = (BYTE)ID_BYTE3;
							tsen.LIGHTING2.id2 = (BYTE)ID_BYTE2;
							tsen.LIGHTING2.id3 = (BYTE)ID_BYTE1;
							tsen.LIGHTING2.id4 = (BYTE)ID_BYTE0;
							tsen.LIGHTING2.level = 0;
							tsen.LIGHTING2.rssi = rssi;
							tsen.LIGHTING2.unitcode = 1;
							tsen.LIGHTING2.cmnd = (bPIROn) ? light2_sOn : light2_sOff;
							sDecodeRXMessage(this, (const unsigned char *)&tsen.LIGHTING2, nullptr, 255, m_Name.c_str());
						}
						else {
							//Error code
						}
					}
					if (iType == 0x07 && iType == 0x03)
					{ // A5-07-03, Occupancy sensor with Supply voltage monitor and 10-bit illumination measurement
						//(EPP A5-07-03)
						if (DATA_BYTE3 < 251)
						{
							RBUF tsen;

							float voltage = GetDeviceValue(DATA_BYTE3, 0, 250, 0, 5000.0F); // need to convert value from V to mV
							memset(&tsen, 0, sizeof(RBUF));
							tsen.RFXSENSOR.packetlength = sizeof(tsen.RFXSENSOR) - 1;
							tsen.RFXSENSOR.packettype = pTypeRFXSensor;
							tsen.RFXSENSOR.subtype = sTypeRFXSensorVolt;
							tsen.RFXSENSOR.id = ID_BYTE1;
							// WARNING
							// filler & rssi fields are used here to transmit ID_BYTE0 value to decode_RFXSensor in mainworker.cpp
							// decode_RFXSensor sets BatteryLevel to 255 (Unknown) and rssi to 12 (Not available)
							tsen.RFXSENSOR.filler = ID_BYTE0 & 0x0F;
							tsen.RFXSENSOR.rssi = (ID_BYTE0 & 0xF0) >> 4;
							tsen.RFXSENSOR.msg1 = (BYTE)(voltage / 256);
							tsen.RFXSENSOR.msg2 = (BYTE)(voltage - (tsen.RFXSENSOR.msg1 * 256));
							sDecodeRXMessage(this, (const unsigned char *)&tsen.RFXSENSOR, nullptr, 255, nullptr);

							int lux = (DATA_BYTE2 << 2) | (DATA_BYTE1>>6);
							if (lux > 1000)
								lux = 1000;
							_tLightMeter lmeter;
							lmeter.id1 = (BYTE)ID_BYTE3;
							lmeter.id2 = (BYTE)ID_BYTE2;
							lmeter.id3 = (BYTE)ID_BYTE1;
							lmeter.id4 = (BYTE)ID_BYTE0;
							lmeter.dunit = 1;
							lmeter.fLux = (float)lux;
							sDecodeRXMessage(this, (const unsigned char *)&lmeter, nullptr, 255, nullptr);

							bool bPIROn = (DATA_BYTE0 & 0x80)!=0;
							memset(&tsen, 0, sizeof(RBUF));
							tsen.LIGHTING2.packetlength = sizeof(tsen.LIGHTING2) - 1;
							tsen.LIGHTING2.packettype = pTypeLighting2;
							tsen.LIGHTING2.subtype = sTypeAC;
							tsen.LIGHTING2.seqnbr = 0;
							tsen.LIGHTING2.id1 = (BYTE)ID_BYTE3;
							tsen.LIGHTING2.id2 = (BYTE)ID_BYTE2;
							tsen.LIGHTING2.id3 = (BYTE)ID_BYTE1;
							tsen.LIGHTING2.id4 = (BYTE)ID_BYTE0;
							tsen.LIGHTING2.level = 0;
							tsen.LIGHTING2.rssi = rssi;
							tsen.LIGHTING2.unitcode = 1;
							tsen.LIGHTING2.cmnd = (bPIROn) ? light2_sOn : light2_sOff;
							sDecodeRXMessage(this, (const unsigned char *)&tsen.LIGHTING2, nullptr, 255, m_Name.c_str());
						}
						else {
							//Error code
						}
					}
					else if (iType == 0x09 && iType == 0x04)
					{	// A5-09-04, CO2 Gas Sensor with Temp and Humidity
						// DB3 = Humidity in 0.5% steps, 0...200 -> 0...100% RH (0x51 = 40%)
						// DB2 = CO2 concentration in 10 ppm steps, 0...255 -> 0...2550 ppm (0x39 = 570 ppm)
						// DB1 = Temperature in 0.2C steps, 0...255 -> 0...51 C (0x7B = 24.6 C)
						// DB0 = flags (DB0.3: 1=data, 0=teach-in; DB0.2: 1=Hum Sensor available, 0=no Hum; DB0.1: 1=Temp sensor available, 0=No Temp; DB0.0 not used)
						// mBuffer[15] is RSSI as -dBm (ie value of 100 means "-100 dBm"), but RssiLevel is in the range 0...11 (or reported as 12 if not known)
						// Battery level is not reported by device, so use fixed value of 9 as per other sensor functions

						// TODO: Check sensor availability flags and only report humidity and/or temp if available.

						float temp = GetDeviceValue(DATA_BYTE1, 0, 255, 0, 51);
						float hum = GetDeviceValue(DATA_BYTE3, 0, 200, 0, 100);
						int co2 = (int)GetDeviceValue(DATA_BYTE2, 0, 255, 0, 2550);
						int NodeID = (ID_BYTE2 << 8) + ID_BYTE1;

						// Report battery level as 9
						SendTempHumSensor(NodeID, 9, temp, round(hum), "GasSensor.04", rssi);
						SendAirQualitySensor((NodeID & 0xFF00) >> 8, NodeID & 0xFF, 9, co2, "GasSensor.04");
					}
				}
			}
			break;
		case RORG_RPS: // repeated switch communication
			{
#ifdef ENOCEAN_BUTTON_DEBUG
				sprintf(szTmp, "RPS data: Sender id: %02X%02X%02X%02X Status: %02X Data: %02X",
					m_buffer[2],m_buffer[3],m_buffer[4],m_buffer[5],
					m_buffer[6],
					m_buffer[1]
				);
				Log(LOG_NORM, "%s", szTmp);
				if (m_buffer[6] & (1 << 2))
				{
					Log(LOG_NORM, "T21");
				}
#endif // ENOCEAN_BUTTON_DEBUG

				unsigned char STATUS=m_buffer[6];

				unsigned char T21 = (m_buffer[6] & S_RPS_T21) >> S_RPS_T21_SHIFT;
				unsigned char NU = (m_buffer[6] & S_RPS_NU) >> S_RPS_NU_SHIFT;

				unsigned char ID_BYTE3=m_buffer[2];
				unsigned char ID_BYTE2=m_buffer[3];
				unsigned char ID_BYTE1=m_buffer[4];
				unsigned char ID_BYTE0=m_buffer[5];
				uint32_t iNodeID = GetINodeID(ID_BYTE3, ID_BYTE2, ID_BYTE1, ID_BYTE0);
				std::string nodeID = GetNodeID(iNodeID);
				int Profile;
				int iType;

				// if a button is attached to a module, we should ignore it else its datagram will conflict with status reported by the module using VLD datagram
				std::vector<std::vector<std::string> > result;
				result = m_sql.safe_query("SELECT ID, Profile, [Type] FROM EnoceanSensors WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, nodeID.c_str());
				if (result.empty())
				{
					// If SELECT returns nothing, add Enocean sensor to the database
					// with a default profile and type. The profile and type
					// will have to be updated manually by the user.
					int manufacturer = 0x7FF;  // generic manufacturer
					Profile = 0x02;
					iType = 0x01;
					m_sql.safe_query("INSERT INTO EnoceanSensors (HardwareID, DeviceID, Manufacturer, Profile, [Type]) VALUES (%d, '%q', %d, %d, %d)", m_HwdID, nodeID.c_str(), manufacturer, Profile, iType);
					Log(LOG_NORM, "Sender_ID %08X inserted in the database with default profile F6-%02X-%02X", iNodeID, Profile, iType);
					Log(LOG_NORM, "If your Enocean RPS device uses another profile, you must update its configuration.");
				}
				else
				{
					// hardware device was already teached-in
					Profile=atoi(result[0][1].c_str());
					iType=atoi(result[0][2].c_str());
					Debug(DEBUG_HARDWARE, "Sender_ID %08X found in the database with profile F6-%02X-%02X", iNodeID, Profile, iType);
					if( (Profile == 0x01) &&						// profile 1 (D2-01) is Electronic switches and dimmers with Energy Measurement and Local Control
						 ((iType == 0x0F) || (iType == 0x12))	// type 0F and 12 have external switch/push button control, it means they also act as rocker
						)
					{
#ifdef ENOCEAN_BUTTON_DEBUG
						Log(LOG_STATUS,"%s, ignore button press", nodeID.c_str());
#endif // ENOCEAN_BUTTON_DEBUG
						break;
					}
				}

				// Whether we use the ButtonID reporting with ON/OFF
				bool useButtonIDs = true;

				// Profile F6-02-01
				// Rocker switch, 2 Rocker (Light and blind control, Application style 1)
				if ((Profile == 0x02) && (iType == 0x01))
				{

					if (STATUS & S_RPS_NU)
					{
						//Rocker

						unsigned char DATA_BYTE3=m_buffer[1];

						// NU == 1, N-Message
						unsigned char ButtonID = (DATA_BYTE3 & DB3_RPS_NU_BID) >> DB3_RPS_NU_BID_SHIFT;
						unsigned char RockerID = (DATA_BYTE3 & DB3_RPS_NU_RID) >> DB3_RPS_NU_RID_SHIFT;
						unsigned char UpDown=(DATA_BYTE3 & DB3_RPS_NU_UD)  >> DB3_RPS_NU_UD_SHIFT;
						unsigned char Pressed=(DATA_BYTE3 & DB3_RPS_NU_PR) >> DB3_RPS_NU_PR_SHIFT;

						unsigned char SecondButtonID = (DATA_BYTE3 & DB3_RPS_NU_SBID) >> DB3_RPS_NU_SBID_SHIFT;
						unsigned char SecondRockerID = (DATA_BYTE3 & DB3_RPS_NU_SRID) >> DB3_RPS_NU_SRID_SHIFT;
						unsigned char SecondUpDown=(DATA_BYTE3 & DB3_RPS_NU_SUD)>>DB3_RPS_NU_SUD_SHIFT;
						unsigned char SecondAction=(DATA_BYTE3 & DB3_RPS_NU_SA)>>DB3_RPS_NU_SA_SHIFT;

#ifdef ENOCEAN_BUTTON_DEBUG
						Log(LOG_NORM,
							"Received RPS N-Message   message: %02X Node %08x RockerID: %i ButtonID: %i Pressed: %i UD: %i Second Rocker ID: %i SecondButtonID: %i SUD: %i Second Action: %i",
							DATA_BYTE3,
							iNodeID,
							RockerID,
							ButtonID,
							UpDown,
							Pressed,
							SecondRockerID,
							SecondButtonID,
							SecondUpDown,
							SecondAction);
#endif // ENOCEAN_BUTTON_DEBUG

						//We distinguish 3 types of buttons from a switch: Left/Right/Left+Right
						if (Pressed==1)
						{
							RBUF tsen;
							memset(&tsen,0,sizeof(RBUF));
							tsen.LIGHTING2.packetlength=sizeof(tsen.LIGHTING2)-1;
							tsen.LIGHTING2.packettype=pTypeLighting2;
							tsen.LIGHTING2.subtype=sTypeAC;
							tsen.LIGHTING2.seqnbr=0;
							tsen.LIGHTING2.id1=(BYTE)ID_BYTE3;
							tsen.LIGHTING2.id2=(BYTE)ID_BYTE2;
							tsen.LIGHTING2.id3=(BYTE)ID_BYTE1;
							tsen.LIGHTING2.id4=(BYTE)ID_BYTE0;
							tsen.LIGHTING2.level=0;
							tsen.LIGHTING2.rssi=rssi;

							if (SecondAction==0)
							{
								if (useButtonIDs)
								{
									//Left/Right Pressed
									tsen.LIGHTING2.unitcode = ButtonID + 1;
									tsen.LIGHTING2.cmnd     = light2_sOn; // the button is pressed, so we don't get an OFF message here
								}
								else
								{
									//Left/Right Up/Down
									tsen.LIGHTING2.unitcode = RockerID + 1;
									tsen.LIGHTING2.cmnd     = (UpDown == 1) ? light2_sOn : light2_sOff;
								}
							}
							else
							{
								if (useButtonIDs)
								{
									//Left+Right Pressed
									tsen.LIGHTING2.unitcode = ButtonID + 10;
									tsen.LIGHTING2.cmnd     = light2_sOn;  // the button is pressed, so we don't get an OFF message here
								}
								else
								{
									//Left+Right Up/Down
									tsen.LIGHTING2.unitcode = SecondRockerID + 10;
									tsen.LIGHTING2.cmnd     = (SecondUpDown == 1) ? light2_sOn : light2_sOff;
								}
							}

#ifdef ENOCEAN_BUTTON_DEBUG
							Log(LOG_NORM, "EnOcean message: %02X Node %08x UnitID: %02X cmd: %02X ",
								DATA_BYTE3,
								iNodeID,
								tsen.LIGHTING2.unitcode,
								tsen.LIGHTING2.cmnd
								);
#endif //ENOCEAN_BUTTON_DEBUG

							sDecodeRXMessage(this, (const unsigned char *)&tsen.LIGHTING2, nullptr, 255, m_Name.c_str());
						}
					}
					else
					{
						if ((T21 == 1) && (NU == 0))
						{
							unsigned char DATA_BYTE3 = m_buffer[1];

							unsigned char ButtonID = (DATA_BYTE3 & DB3_RPS_BUTTONS) >> DB3_RPS_BUTTONS_SHIFT;
							unsigned char Pressed = (DATA_BYTE3 & DB3_RPS_PR) >> DB3_RPS_PR_SHIFT;

							unsigned char UpDown = !((DATA_BYTE3 == 0xD0) || (DATA_BYTE3 == 0xF0));

#ifdef ENOCEAN_BUTTON_DEBUG
							Log(LOG_NORM, "Received RPS T21-Message message: %02X Node %08x ButtonID: %i Pressed: %i UD: %i",
								DATA_BYTE3,
								iNodeID,
								ButtonID,
								Pressed,
								UpDown);
#endif //ENOCEAN_BUTTON_DEBUG

							RBUF tsen;
							memset(&tsen, 0, sizeof(RBUF));
							tsen.LIGHTING2.packetlength = sizeof(tsen.LIGHTING2) - 1;
							tsen.LIGHTING2.packettype = pTypeLighting2;
							tsen.LIGHTING2.subtype = sTypeAC;
							tsen.LIGHTING2.seqnbr = 0;
							tsen.LIGHTING2.id1 = (BYTE)ID_BYTE3;
							tsen.LIGHTING2.id2 = (BYTE)ID_BYTE2;
							tsen.LIGHTING2.id3 = (BYTE)ID_BYTE1;
							tsen.LIGHTING2.id4 = (BYTE)ID_BYTE0;
							tsen.LIGHTING2.level = 0;
							tsen.LIGHTING2.rssi = rssi;

							if (useButtonIDs)
							{
								// It's the release message of any button pressed before
								tsen.LIGHTING2.unitcode = 0; // does not matter, since we are using a group command
								tsen.LIGHTING2.cmnd = (Pressed == 1) ? light2_sGroupOn : light2_sGroupOff;
							}
							else
							{
								tsen.LIGHTING2.unitcode = 1;
								tsen.LIGHTING2.cmnd = (UpDown == 1) ? light2_sOn : light2_sOff;
							}
#ifdef ENOCEAN_BUTTON_DEBUG

							Log(LOG_NORM, "EnOcean message: %02X Node %08x UnitID: %02X cmd: %02X ",
								DATA_BYTE3,
								iNodeID,
								tsen.LIGHTING2.unitcode,
								tsen.LIGHTING2.cmnd);

#endif // ENOCEAN_BUTTON_DEBUG

							sDecodeRXMessage(this, (const unsigned char *)&tsen.LIGHTING2, nullptr, 255, m_Name.c_str());
						}
					}
				}
				// The code below supports all the F6-05-xx 'Detectors' profiles:
				// F6-05-00 : Wind speed threshold detector
				// F6-05-01 : Liquid leakage sensor
				// F6-05-02 : Smoke detector
				// Tested with an Ubiwizz UBILD001-QM smoke detector
				else if (Profile == 0x05)
				{
					Debug(DEBUG_HARDWARE, "message profile F6-05-xx: Data=%02X", m_buffer[1]);
					bool alarm = false;
					int batterylevel = 255;
					if (iType == 0x00 || iType == 0x02)  // only profiles F6-05-00 and F6-05-02 report Energy LOW warning
					{
						batterylevel = 100;
					}
					switch (m_buffer[1]) {
						case 0x00:   // profiles F6-05-00 and F6-05-02
						{
							Debug(DEBUG_HARDWARE, "Alarm OFF from Sender id %02X%02X%02X%02X", m_buffer[2], m_buffer[3], m_buffer[4], m_buffer[5]);
							break;
						}
						case 0x10:  // profiles F6-05-00 and F6-05-02
						{
							Log(LOG_NORM, "Alarm ON from Sender id %02X%02X%02X%02X", m_buffer[2], m_buffer[3], m_buffer[4], m_buffer[5]);
							alarm = true;
							break;
						}
						case 0x11:  // profile F6-05-01
						{
							Log(LOG_NORM, "Alarm ON water detected from Sender id %02X%02X%02X%02X", m_buffer[2], m_buffer[3], m_buffer[4], m_buffer[5]);
							alarm = true;
							break;
						}
						case 0x30:  // profiles F6-05-00 and F6-05-02
						{
							Log(LOG_NORM, "Energy LOW warning from Sender id %02X%02X%02X%02X", m_buffer[2], m_buffer[3], m_buffer[4], m_buffer[5]);
							batterylevel = 5;
							break;
						}
					}
					SendSwitch(iNodeID, 1, batterylevel, alarm, 0, "Detector", m_Name, rssi);
				}
			}
			break;

		case RORG_UTE:
				// Universal teach-in (0xD4)
				{
					unsigned char uni_bi_directional_communication = (m_buffer[1] >> 7) & 1;		// 0=mono, 1= bi
					unsigned char eep_teach_in_response_expected = (m_buffer[1] >> 6) & 1;			// 0=yes, 1=no
					unsigned char teach_in_request = (m_buffer[1] >> 4) & 3;								// 0= request, 1= deletion request, 2=request or deletion request, 3=not used
					unsigned char cmd = m_buffer[1] & 0x0F;

					if(cmd == 0x0)
					{
						// EEP Teach-In Query (UTE Message / CMD 0x0)

						unsigned char nb_channel = m_buffer[2];
						unsigned int manID = ((unsigned int)(m_buffer[4] & 0x7)) << 8 | (m_buffer[3]);
						unsigned char type = m_buffer[5];
						unsigned char func = m_buffer[6];
						unsigned char rorg = m_buffer[7];

						unsigned char ID_BYTE3=m_buffer[8];
						unsigned char ID_BYTE2=m_buffer[9];
						unsigned char ID_BYTE1=m_buffer[10];
						unsigned char ID_BYTE0=m_buffer[11];
						uint32_t iNodeID = GetINodeID(ID_BYTE3, ID_BYTE2, ID_BYTE1, ID_BYTE0);

						Log(LOG_NORM, "teach-in request received from %08X (manufacturer: %03X). number of channels: %d, device profile: %02X-%02X-%02X", iNodeID, manID, nb_channel, rorg,func,type);

						// Record EnOcean device profile
						std::string nodeID = GetNodeID(iNodeID);
						std::vector<std::vector<std::string> > result;
						result = m_sql.safe_query("SELECT ID FROM EnoceanSensors WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, nodeID.c_str());
						if (result.empty())
						{
							// If not found, add it to the database
							m_sql.safe_query("INSERT INTO EnoceanSensors (HardwareID, DeviceID, Manufacturer, Profile, [Type]) VALUES (%d,'%q',%d,%d,%d)", m_HwdID, nodeID.c_str(), manID, func, type);
							Log(LOG_NORM, "Sender_ID %08X inserted in the database", iNodeID);
						}
						else
							Log(LOG_NORM, "Sender_ID %08X already in the database", iNodeID);
						ReloadVLDNodes();

						if((rorg == 0xD2) && (func == 0x01) && ( (type == 0x12) || (type == 0x0F) ))
						{
							unsigned char nbc;

							for(nbc = 0; nbc < nb_channel; nbc ++)
							{
								RBUF tsen;

								memset(&tsen,0,sizeof(RBUF));
								tsen.LIGHTING2.packetlength=sizeof(tsen.LIGHTING2)-1;
								tsen.LIGHTING2.packettype=pTypeLighting2;
								tsen.LIGHTING2.subtype=sTypeAC;
								tsen.LIGHTING2.seqnbr=0;
								tsen.LIGHTING2.id1=(BYTE)ID_BYTE3;
								tsen.LIGHTING2.id2=(BYTE)ID_BYTE2;
								tsen.LIGHTING2.id3=(BYTE)ID_BYTE1;
								tsen.LIGHTING2.id4=(BYTE)ID_BYTE0;
								tsen.LIGHTING2.level=0;
								tsen.LIGHTING2.rssi=rssi;
								tsen.LIGHTING2.unitcode = nbc + 1;
								tsen.LIGHTING2.cmnd     = light2_sOff;

#ifdef ENOCEAN_BUTTON_DEBUG
								Log(LOG_NORM, "EnOcean message: 0xD4 Node %08x UnitID: %02X cmd: %02X ",
											iNodeID,
											tsen.LIGHTING2.unitcode,
											tsen.LIGHTING2.cmnd
										);
#endif //ENOCEAN_BUTTON_DEBUG

								Log(LOG_NORM, "channel = %d", nbc+1);
								sDecodeRXMessage(this, (const unsigned char *)&tsen.LIGHTING2, nullptr,
										 255, m_Name.c_str());
							}
							return;
						}
						break;
					}

					Log(LOG_NORM, "Unhandled RORG (%02X), uni_bi (%02X [1=bidir]), response_expected (%02X [0=yes]), request (%02X), cmd (%02X)", m_buffer[0], uni_bi_directional_communication,eep_teach_in_response_expected, teach_in_request, cmd);
				}
			break;

		case RORG_VLD:
			{
				uint8_t ID_BYTE3 = m_buffer[m_DataSize - 5];
				uint8_t ID_BYTE2 = m_buffer[m_DataSize - 4];
				uint8_t ID_BYTE1 = m_buffer[m_DataSize - 3];
				uint8_t ID_BYTE0 = m_buffer[m_DataSize - 2];
				uint32_t id = (ID_BYTE3 << 24) + (ID_BYTE2 << 16) + (ID_BYTE1 << 8) + ID_BYTE0;

				char szDeviceID[20];
				std::vector<std::vector<std::string> > result;
				sprintf(szDeviceID, "%08X", (unsigned int)id);

				// report status only if it is a known device else we may have an incorrect profile
				auto itt = m_VLDNodes.find(id);
				if (itt == m_VLDNodes.end())
				{
					Log(LOG_NORM, "Need Teach-In for %s", szDeviceID);
					return;
				}
				uint8_t func = itt->second.profile;
				uint8_t type = itt->second.type;

				Log(LOG_NORM, "EnOcean message VLD: from Node %s EEP: %02X-%02X-%02X", szDeviceID, RORG_VLD, func, type);

				if (func == 0x01)
				{ // D2-01-XX, Electronic Switches and Dimmers with Local Control
					uint8_t CMD = m_buffer[1] & 0x0F;			// Command ID
					if (CMD != 0x04)
					{
						Log(LOG_NORM, "EnOcean: VLD msg: Node %s, Unhandled CMD (%02X)", szDeviceID, CMD);
						return;
					}
					// CMD 0x4 - Actuator Status Response

					uint8_t IO = m_buffer[2] & 0x1F;	 		// I/O Channel
					uint8_t OV = m_buffer[3] & 0x7F;			// Output Value : 0x00 = OFF, 0x01...0x64: Output value 1% to 100% or ON

					RBUF tsen;
					memset(&tsen, 0, sizeof(RBUF));
					tsen.LIGHTING2.packetlength = sizeof(tsen.LIGHTING2) - 1;
					tsen.LIGHTING2.packettype = pTypeLighting2;
					tsen.LIGHTING2.subtype = sTypeAC;
					tsen.LIGHTING2.seqnbr = 0;
					tsen.LIGHTING2.id1 = (BYTE)ID_BYTE3;
					tsen.LIGHTING2.id2 = (BYTE)ID_BYTE2;
					tsen.LIGHTING2.id3 = (BYTE)ID_BYTE1;
					tsen.LIGHTING2.id4 = (BYTE)ID_BYTE0;
					tsen.LIGHTING2.level = OV;
					tsen.LIGHTING2.rssi = rssi;
					tsen.LIGHTING2.unitcode = IO + 1;
					tsen.LIGHTING2.cmnd = (OV > 0) ? light2_sOn : light2_sOff;

#ifdef ENOCEAN_BUTTON_DEBUG
					Log(LOG_NORM, "EnOcean: VLD->RX msg: Node %s CMD: 0x%X IO: %02X (UnitID: %d) OV: %02X (Cmnd: %d Level: %d)",
						szDeviceID, CMD, IO, tsen.LIGHTING2.unitcode, OV, tsen.LIGHTING2.cmnd, tsen.LIGHTING2.level);
#endif //ENOCEAN_BUTTON_DEBUG

					sDecodeRXMessage(this, (const unsigned char *)&tsen.LIGHTING2, nullptr, 255, m_Name.c_str());

					// Note: if a device uses simultaneously RPS and VLD (ex: nodon inwall module), it can be partially initialized.
					// Domoticz will show device status but some functions may not work because EnoceanSensors table has no info on this device (until teach-in is performed)
					// If a device has local control (ex: nodon inwall module with physically attached switched), domoticz will record the local control as unit 0.
					// Ex: nodon inwall 2 channels will show 3 entries. Unit 0 is the local switch, 1 is the first channel, 2 is the second channel.
					return;
				}
				if (func == 0x03 && type == 0x0A)
				{ // D2-03-0A Push Button – Single Button
					uint8_t BATT = (uint8_t) GetDeviceValue(m_buffer[1], 1, 100, 1, 100);
					uint8_t BA = m_buffer[2]; // 1 = simple press, 2=double press, 3=long press, 4=long press released
					SendGeneralSwitch(id, BA, BATT, 1, 0, "Switch", m_Name, 12);
					return;
				}
				Log(LOG_NORM, "EnOcean: Node %s, Unhandled EEP (%02X-%02X-%02X)", szDeviceID, RORG_VLD, func, type);
			}
			break;
		default:
			Log(LOG_NORM, "Unhandled RORG (%02X)", m_buffer[0]);
			break;
	}
}

struct _tPacketTypeTable
{
	uint8_t PT;
	const char *label;
	const char *description;
};

static const _tPacketTypeTable _packetTypeTable[] = {
	{ PACKET_RADIO_ERP1, "RADIO_ERP1", "ERP1 radio telegram" },
	{ PACKET_RESPONSE, "RESPONSE", "Response to any packet" },
	{ PACKET_RADIO_SUB_TEL, "RADIO_SUB_TEL", "Radio subtelegram" },
	{ PACKET_EVENT, "EVENT", "Event message" },
	{ PACKET_COMMON_COMMAND, "COMMON_COMMAND", "Common command" },
	{ PACKET_SMART_ACK_COMMAND, "SMART_ACK_COMMAND", "Smart Acknowledge command" },
	{ PACKET_REMOTE_MAN_COMMAND, "REMOTE_MAN_COMMAND", "Remote management command" },
	{ PACKET_RADIO_MESSAGE, "RADIO_MESSAGE", "Radio message" },
	{ PACKET_RADIO_ERP2, "RADIO_ERP2", "ERP2 radio telegram" },
	{ PACKET_CONFIG_COMMAND, "CONFIG_COMMAND", "RESERVED" },
	{ PACKET_COMMAND_ACCEPTED, "COMMAND_ACCEPTED", "For long operations, informs the host the command is accepted" },
	{ PACKET_RADIO_802_15_4, "RADIO_802_15_4", "802_15_4 Raw Packet" },
	{ PACKET_COMMAND_2_4, "COMMAND_2_4", "2.4 GHz Command" },
	{ 0, nullptr, nullptr }
};

const char *CEnOceanESP3::GetPacketTypeLabel(uint8_t PT)
{
	for (const _tPacketTypeTable *pTable = _packetTypeTable; pTable->PT; pTable++)
		if (pTable->PT == PT)
			return pTable->label;

	return "RESERVED";
}

const char *CEnOceanESP3::GetPacketTypeDescription(uint8_t PT)
{
	for (const _tPacketTypeTable *pTable = _packetTypeTable; pTable->PT; pTable++)
		if (pTable->PT == PT)
			return pTable->description;

	return "Reserved ESP3 packet type";
}

struct _tReturnCodeTable
{
	uint8_t RC;
	const char *label;
	const char *description;
};

static const _tReturnCodeTable _returnCodeTable[] = {
	{ RET_OK, "OK", "No error" },
	{ RET_ERROR, "ERROR", "There is an error occurred" },
	{ RET_NOT_SUPPORTED, "NOT_SUPPORTED", "The functionality is not supported by that implementation" },
	{ RET_WRONG_PARAM, "WRONG_PARAM", "There was a wrong parameter in the command" },
	{ RET_OPERATION_DENIED, "OPERATION_DENIED", "The operation cannot be performed" },
	{ RET_LOCK_SET, "LOCK_SET", "Duty cycle lock" },
	{ RET_BUFFER_TO_SMALL, "BUFFER_TO_SMALL", "The internal ESP3 buffer of the device is too small, to handle this telegram" },
	{ RET_NO_FREE_BUFFER, "NO_FREE_BUFFER", "Currently all internal buffers are used" },
	{ RET_MEMORY_ERROR, "MEMORY_ERROR", "The memory write process failed" },
	{ RET_BASEID_OUT_OF_RANGE, "BASEID_OUT_OF_RANGE", "Invalid BaseID" },
	{ RET_BASEID_MAX_REACHED, "BASEID_MAX_REACHED", "BaseID has already been changed 10 times, no more changes are allowed" },
	{ 0, nullptr, nullptr }
};

const char *CEnOceanESP3::GetReturnCodeLabel(uint8_t RC)
{
	for (const _tReturnCodeTable *pTable = _returnCodeTable; pTable->label; pTable++)
		if (pTable->RC == RC)
			return pTable->label;

	if (RC > 0x80)
		return "RC>0x80";

	return "UNKNOWN";
}

const char *CEnOceanESP3::GetReturnCodeDescription(uint8_t RC)
{
	for (const _tReturnCodeTable *pTable = _returnCodeTable; pTable->description; pTable++)
		if (pTable->RC == RC)
			return pTable->description;

	if (RC > 0x80)
		return "Return codes greater than 0x80 used for commands with special return information, not commonly useable";

	return "<<Unknown return code... Please report!<<";
}

struct _tEventCodeTable
{
	uint8_t EC;
	const char *label;
	const char *description;
};
static const _tEventCodeTable _eventCodeTable[] = {
	{ SA_RECLAIM_NOT_SUCCESSFUL, "RECLAIM_NOT_SUCCESSFUL", "Informs the external host about an unsuccessful reclaim by a Smart Ack client" },
	{ SA_CONFIRM_LEARN, "CONFIRM_LEARN", "Request to the external host about how to handle a received learn-in / learn-out of a Smart Ack. client" },
	{ SA_LEARN_ACK, "LEARN_ACK", "Response to the Smart Ack. client about the result of its Smart Acknowledge learn request" },
	{ CO_READY, "READY", "Inform the external about the readiness for operation" },
	{ CO_EVENT_SECUREDEVICES, "EVENT_SECUREDEVICES", "Informs the external host about an event in relation to security processing" },
	{ CO_DUTYCYCLE_LIMIT, "DUTYCYCLE_LIMIT", "Informs the external host about reaching the duty cycle limit" },
	{ CO_TRANSMIT_FAILED, "TRANSMIT_FAILED", "Informs the external host about not being able to send a telegram" },
	{ CO_TX_DONE, "TX_DONE", "Informs that all TX operations are done" },
	{ CO_LRN_MODE_DISABLED, "LRN_MODE_DISABLED", "Informs that the learn mode has time-out" },
	{ 0, nullptr, nullptr }
};

const char *CEnOceanESP3::GetEventCodeLabel(const uint8_t EC)
{
	for (const _tEventCodeTable *pTable = _eventCodeTable; pTable->EC; pTable++)
		if (pTable->EC == EC)
			return pTable->label;

	return "UNKNOWN";
}

const char *CEnOceanESP3::GetEventCodeDescription(const uint8_t EC)
{
	for (const _tEventCodeTable *pTable = _eventCodeTable; pTable->EC; pTable++)
		if (pTable->EC == EC)
			return pTable->description;

	return ">>Unkown function event code... Please report!<<";
}

struct _tCommonCommandTable
{
	uint8_t CC;
	const char *label;
	const char *description;
};

static const _tCommonCommandTable _commonCommandTable[] = {
	{ CO_WR_SLEEP, "WR_SLEEP", "Enter energy saving mode" },
	{ CO_WR_RESET, "WR_RESET", "Reset the device" },
	{ CO_RD_VERSION, "RD_VERSION", "Read the device version information" },
	{ CO_RD_SYS_LOG, "RD_SYS_LOG", "Read system log" },
	{ CO_WR_SYS_LOG, "WR_SYS_LOG", "Reset system log" },
	{ CO_WR_BIST, "WR_BIST", "Perform Self Test" },
	{ CO_WR_IDBASE, "WR_IDBASE", "Set ID range base address" },
	{ CO_RD_IDBASE, "RD_IDBASE", "Read ID range base address" },
	{ CO_WR_REPEATER, "WR_REPEATER", "Set Repeater Level" },
	{ CO_RD_REPEATER, "RD_REPEATER", "Read Repeater Level" },
	{ CO_WR_FILTER_ADD, "WR_FILTER_ADD", "Add filter to filter list" },
	{ CO_WR_FILTER_DEL, "WR_FILTER_DEL", "Delete a specific filter from filter list" },
	{ CO_WR_FILTER_DEL_ALL, "WR_FILTER_DEL_ALL", "Delete all filters from filter list" },
	{ CO_WR_FILTER_ENABLE, "WR_FILTER_ENABLE", "Enable / disable filter list" },
	{ CO_RD_FILTER, "RD_FILTER", "Read filters from filter list" },
	{ CO_WR_WAIT_MATURITY, "WR_WAIT_MATURITY", "Wait until the end of telegram maturity time before received radio telegrams will be forwarded to the external host" },
	{ CO_WR_SUBTEL, "WR_SUBTEL", "Enable / Disable transmission of additional subtelegram info to the external host" },
	{ CO_WR_MEM, "WR_MEM", "Write data to device memory" },
	{ CO_RD_MEM, "RD_MEM", "Read data from device memory" },
	{ CO_RD_MEM_ADDRESS, "RD_MEM_ADDRESS", "Read address and length of the configuration area and the Smart Ack Table" },
	{ CO_RD_SECURITY, "RD_SECURITY", "key)" },
	{ CO_WR_SECURITY, "WR_SECURITY", "key)" },
	{ CO_WR_LEARNMODE, "WR_LEARNMODE", "Enable / disable learn mode" },
	{ CO_RD_LEARNMODE, "RD_LEARNMODE", "ead learn mode status" },
	{ CO_WR_SECUREDEVICE_ADD, "WR_SECUREDEVICE_ADD", "DEPRECATED Add a secure device" },
	{ CO_WR_SECUREDEVICE_DEL, "WR_SECUREDEVICE_DEL", "Delete a secure device from the link table" },
	{ CO_RD_SECUREDEVICE_BY_INDEX, "RD_SECUREDEVICE_BY_INDEX", "DEPRECATED Read secure device by index" },
	{ CO_WR_MODE, "WR_MODE", "Set the gateway transceiver mode" },
	{ CO_RD_NUMSECUREDEVICES, "RD_NUMSECUREDEVICES", "Read number of secure devices in the secure link table" },
	{ CO_RD_SECUREDEVICE_BY_ID, "RD_SECUREDEVICE_BY_ID", "Read information about a specific secure device from the secure link table using the device ID" },
	{ CO_WR_SECUREDEVICE_ADD_PSK, "WR_SECUREDEVICE_ADD_PSK", "Add Pre-shared key for inbound secure device" },
	{ CO_WR_SECUREDEVICE_ENDTEACHIN, "WR_SECUREDEVICE_ENDTEACHIN", "Send Secure teach-In message" },
	{ CO_WR_TEMPORARY_RLC_WINDOW, "WR_TEMPORARY_RLC_WINDOW", "Set a temporary rolling-code window for every taught-in device" },
	{ CO_RD_SECUREDEVICE_PSK, "RD_SECUREDEVICE_PSK", "Read PSK" },
	{ CO_RD_DUTYCYCLE_LIMIT, "RD_DUTYCYCLE_LIMIT", "Read the status of the duty cycle limit monitor" },
	{ CO_SET_BAUDRATE, "SET_BAUDRATE", "Set the baud rate used to communicate with the external host" },
	{ CO_GET_FREQUENCY_INFO, "GET_FREQUENCY_INFO", "Read the radio frequency and protocol supported by the device" },
	{ CO_38T_STEPCODE, "38T_STEPCODE", "Read Hardware Step code and Revision of the Device" },
	{ CO_40_RESERVED, "40_RESERVED", "Reserved" },
	{ CO_41_RESERVED, "41_RESERVED", "Reserved" },
	{ CO_42_RESERVED, "42_RESERVED", "Reserved" },
	{ CO_43_RESERVED, "43_RESERVED", "Reserved" },
	{ CO_44_RESERVED, "44_RESERVED", "Reserved" },
	{ CO_45_RESERVED, "45_RESERVED", "Reserved" },
	{ CO_WR_REMAN_CODE, "WR_REMAN_CODE", "Set the security code to unlock Remote Management functionality via radio" },
	{ CO_WR_STARTUP_DELAY, "WR_STARTUP_DELAY", "Set the startup delay (time from power up until start of operation)" },
	{ CO_WR_REMAN_REPEATING, "WR_REMAN_REPEATING", "Select if REMAN telegrams originating from this module can be repeated" },
	{ CO_RD_REMAN_REPEATING, "RD_REMAN_REPEATING", "Check if REMAN telegrams originating from this module can be repeated" },
	{ CO_SET_NOISETHRESHOLD, "SET_NOISETHRESHOLD", "Set the RSSI noise threshold level for telegram reception" },
	{ CO_GET_NOISETHRESHOLD, "GET_NOISETHRESHOLD", "Read the RSSI noise threshold level for telegram reception" },
	{ CO_52_RESERVED, "52_RESERVED", "Reserved" },
	{ CO_53_RESERVED, "53_RESERVED", "Reserved" },
	{ CO_WR_RLC_SAVE_PERIOD, "WR_RLC_SAVE_PERIOD", "Set the period in which outgoing RLCs are saved to the EEPROM" },
	{ CO_WR_RLC_LEGACY_MODE, "WR_RLC_LEGACY_MODE", "Activate the legacy RLC security mode allowing roll-over and using the RLC acceptance window for 24bit explicit RLC" },
	{ CO_WR_SECUREDEVICEV2_ADD, "WR_SECUREDEVICEV2_ADD", "Add secure device to secure link table" },
	{ CO_RD_SECUREDEVICEV2_BY_INDEX, "RD_SECUREDEVICEV2_BY_INDEX", "Read secure device from secure link table using the table index" },
	{ CO_WR_RSSITEST_MODE, "WR_RSSITEST_MODE", "Control the state of the RSSI-Test mode" },
	{ CO_RD_RSSITEST_MODE, "RD_RSSITEST_MODE", "Read the state of the RSSI-Test Mode" },
	{ CO_WR_SECUREDEVICE_MAINTENANCEKEY, "WR_SECUREDEVICE_MAINTENANCEKEY", "Add the maintenance key information into the secure link table" },
	{ CO_RD_SECUREDEVICE_MAINTENANCEKEY, "RD_SECUREDEVICE_MAINTENANCEKEY", "Read by index the maintenance key information from the secure link table" },
	{ CO_WR_TRANSPARENT_MODE, "WR_TRANSPARENT_MODE", "Control the state of the transparent mode" },
	{ CO_RD_TRANSPARENT_MODE, "RD_TRANSPARENT_MODE", "Read the state of the transparent mode" },
	{ CO_WR_TX_ONLY_MODE, "WR_TX_ONLY_MODE", "Control the state of the TX only mode" },
	{ CO_RD_TX_ONLY_MODE, "RD_TX_ONLY_MODE", "Read the state of the TX only mode" },
	{ 0, nullptr, nullptr }
};

const char *CEnOceanESP3::GetCommonCommandLabel(const uint8_t CC)
{
	for (const _tCommonCommandTable *pTable = _commonCommandTable; pTable->CC; pTable++)
		if (pTable->CC == CC)
			return pTable->label;

	return "UNKNOWN";
}

const char *CEnOceanESP3::GetCommonCommandDescription(const uint8_t CC)
{
	for (const _tCommonCommandTable *pTable = _commonCommandTable; pTable->CC; pTable++)
		if (pTable->CC == CC)
			return pTable->description;

	return ">>Unkown Common Command... Please report!<<";
}

struct _tSmarkAckCodeTable
{
	uint8_t SA;
	const char *label;
	const char *description;
};

static const _tSmarkAckCodeTable _smarkAckCodeTable[] = {
	{ SA_WR_LEARNMODE, "WR_LEARNMODE", "Set/Reset Smart Ack learn mode" },
	{ SA_RD_LEARNMODE, "RD_LEARNMODE", "Get Smart Ack learn mode state" },
	{ SA_WR_LEARNCONFIRM, "WR_LEARNCONFIRM", "Used for Smart Ack to add or delete a mailbox of a client" },
	{ SA_WR_CLIENTLEARNRQ, "WR_CLIENTLEARNRQ", "Send Smart Ack Learn request (Client)" },
	{ SA_WR_RESET, "WR_RESET", "Send reset command to a Smart Ack client" },
	{ SA_RD_LEARNEDCLIENTS, "RD_LEARNEDCLIENTS", "Get Smart Ack learned sensors / mailboxes" },
	{ SA_WR_RECLAIMS, "WR_RECLAIMS", "Set number of reclaim attempts" },
	{ SA_WR_POSTMASTER, "WR_POSTMASTER", "Activate/Deactivate Post master functionality" },
	{ 0, nullptr, nullptr }
};

const char *CEnOceanESP3::GetSmarkAckCodeLabel(const uint8_t SA)
{
	for (const _tSmarkAckCodeTable *pTable = _smarkAckCodeTable; pTable->SA; pTable++)
		if (pTable->SA == SA)
			return pTable->label;

	return "UNKNOWN";
}

const char *CEnOceanESP3::GetSmartAckCodeDescription(const uint8_t SA)
{
	for (const _tSmarkAckCodeTable *pTable = _smarkAckCodeTable; pTable->SA; pTable++)
		if (pTable->SA == SA)
			return pTable->description;

	return ">>Unkown smark ack code... Please report!<<";
}

struct _tFunctionReturnCodeTable
{
	uint8_t RC;
	const char *label;
	const char *description;
};

static const _tFunctionReturnCodeTable _functionReturnCodeTable[] = {
	{ RC_OK, "RC_OK", "Action performed. No problem detected" },
	{ RC_EXIT, "RC_EXIT", "Action not performed. No problem detected" },
	{ RC_KO, "RC_KO", "Action not performed. Problem detected" },
	{ RC_TIME_OUT, "RC_TIME_OUT", "Action couldn't be carried out within a certain time." },
	{ RC_FLASH_HW_ERROR, "RC_FLASH_HW_ERROR", "The write/erase/verify process failed, the flash page seems to be corrupted" },
	{ RC_NEW_RX_BYTE, "RC_NEW_RX_BYTE", "A new UART/SPI byte received" },
	{ RC_NO_RX_BYTE, "RC_NO_RX_BYTE", "No new UART/SPI byte received" },
	{ RC_NEW_RX_TEL, "RC_NEW_RX_TEL", "New telegram received" },
	{ RC_NO_RX_TEL, "RC_NO_RX_TEL", "No new telegram received" },
	{ RC_NOT_VALID_CHKSUM, "RC_NOT_VALID_CHKSUM", "Checksum not valid" },
	{ RC_NOT_VALID_TEL, "RC_NOT_VALID_TEL", "Telegram not valid" },
	{ RC_BUFF_FULL, "RC_BUFF_FULL", "Buffer full, no space in Tx or Rx buffer" },
	{ RC_ADDR_OUT_OF_MEM, "RC_ADDR_OUT_OF_MEM", "Address is out of memory" },
	{ RC_NOT_VALID_PARAM, "RC_NOT_VALID_PARAM", "Invalid function parameter" },
	{ RC_BIST_FAILED, "RC_BIST_FAILED", "Built in self test failed" },
	{ RC_ST_TIMEOUT_BEFORE_SLEEP, "RC_ST_TIMEOUT_BEFORE_SLEEP", "Before entering power down, the short term timer had timed out." },
	{ RC_MAX_FILTER_REACHED, "RC_MAX_FILTER_REACHED", "Maximum number of filters reached, no more filter possible" },
	{ RC_FILTER_NOT_FOUND, "RC_FILTER_NOT_FOUND", "Filter to delete not found" },
	{ RC_BASEID_OUT_OF_RANGE, "RC_BASEID_OUT_OF_RANGE", "BaseID out of range" },
	{ RC_BASEID_MAX_REACHED, "RC_BASEID_MAX_REACHED", "BaseID was changed 10 times, no more changes are allowed" },
	{ RC_XTAL_NOT_STABLE, "RC_XTAL_NOT_STABLE", "XTAL is not stable" },
	{ RC_NO_TX_TEL, "RC_NO_TX_TEL", "No telegram for transmission in queue" },
	{ RC_ELEGRAM_WAIT, "RC_ELEGRAM_WAIT", "Waiting before sending broadcast message" },
	{ RC_OUT_OF_RANGE, "RC_OUT_OF_RANGE", "Generic out of range return code" },
	{ RC_LOCK_SET, "RC_LOCK_SET", "Function was not executed due to sending lock" },
	{ RC_NEW_TX_TEL, "RC_NEW_TX_TEL", "New telegram transmitted" },
	{ 0, nullptr, nullptr }
};

const char *CEnOceanESP3::GetFunctionReturnCodeLabel(const uint8_t RC)
{
	for (const _tFunctionReturnCodeTable *pTable = _functionReturnCodeTable; pTable->RC; pTable++)
		if (pTable->RC == RC)
			return pTable->label;

	return "UNKNOWN";
}

const char *CEnOceanESP3::GetFunctionReturnCodeDescription(const uint8_t RC)
{
	for (const _tFunctionReturnCodeTable *pTable = _functionReturnCodeTable; pTable->RC; pTable++)
		if (pTable->RC == RC)
			return pTable->description;

	return ">>Unkown function return code... Please report!<<";
}
