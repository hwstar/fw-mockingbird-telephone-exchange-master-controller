#pragma once
#include "top.h"
#include "xps_logical.h"
#include "connector.h"
#include "trunk.h"



namespace Trunk {

const uint8_t MAX_TRUNK_CARDS = 3;
const uint8_t TRUNK_CARD_I2C_ADDRESS = 0x20;
const uint8_t EVENT_MESSAGE_LENGTH = 1;

/* Registers for commands and status */
enum {REG_GET_EVENT=0, REG_NONE=0, REG_GET_BUSY_STATUS=1, REG_SEIZE_TRUNK=2, REG_SEND_WINK=3, REG_INCOMING_CONNECTED=4,
REG_DROP_CALL=5, REG_OUTGOING_ADDR_COMPLETE=6, REG_RESET=7};

/* Events received from card */
enum {EV_NONE=0, EV_REQUEST_IR=1, EV_CALL_DROPPED=2, EV_NO_WINK=3, EV_SEND_ADDR_INFO=4, EV_FAREND_SUPV=5, EV_FAREND_DISC=6, EV_BUSY = 7};

/* Trunk states */
enum {TS_IDLE=0, TS_SEIZE_JUNCTOR=1, TS_SEIZE_TG=2, TS_SEIZE_MFR=3, TS_WAIT_ADDR_INFO=4, TS_HAVE_ADDR_INFO=5, TS_SEND_RINGING=6, TS_RINGING_TEARDOWN=7,
	TS_SEND_BUSY=8, TS_SEND_CONGESTION=9, TS_INCOMING_FAILED=10, TS_INCOMING_WAIT_SUPV=11, TS_INCOMING_CONNECT_AUDIO=12, TS_INCOMING_ANSWERED=13,
	TS_INCOMING_TEARDOWN=14, TS_OUTGOING_START=15, TS_WAIT_WINK_OR_BUSY=16, TS_OUTGOING_REQUEST_ADDR_INFO=17,
	TS_OUTGOING_WAIT_ADDR_INFO=18, TS_GOT_NO_WINK=19, TS_RELEASE_TRUNK=20, TS_SEND_TRUNK_BUSY=21,
	TS_SEND_TRUNK_BUSY_AT_SEIZURE=22, TS_OUTGOING_SEND_ADDR_INFO=23, TS_OUTGOING_SEND_ADDR_INFO_B=24,
	TS_OUTGOING_SEND_ADDR_INFO_C=25, TS_OUTGOING_WAIT_SUPV=26, TS_OUTGOING_ANSWERED=27, TS_OUTGOING_IN_CALL=28,
	TS_OUTGOING_SEND_FAREND_DISC=29, TS_TANDEM_CALL=30, TS_TANDEM_ADVANCE=31, TS_TANDEM_SEND_ADDR_INFO=32,
	TS_TANDEM_WAIT_ADDR_SENT=33, TS_TANDEM_CONNECT_CALLER=34, TS_TANDEM_WAIT_SUPV=35, TS_TANDEM_SUPV=36, TS_TANDEM_IN_CALL=37,
	TS_TANDEM_CALLER_DISCONNECTED=38, TS_TANDEM_CALLED_DISCONNECTED=39, TS_TANDEM_SEND_CONGESTION=40, TS_TANDEM_CONGESTION=41,
	TS_OFFLINE=254, TS_RESET=255};




class Trunk {
protected:
	uint8_t _trunk_to_service;
	osMutexId_t _lock;
	Connector::Conn_Info _conn_info[MAX_TRUNK_CARDS];
	bool _test_pending_state(Connector::Conn_Info *tinfo);




public:
	void _mf_receiver_callback(void *parameter, uint8_t error_code, uint8_t digit_count, char *data);
	void _mf_sending_complete(uint32_t descriptor, void *data);
	void event_handler(uint32_t event_type, uint32_t resource);
	void init(void);
	void poll(void);
	bool go_offline(uint32_t trunk_number);
	bool go_online(uint32_t trunk_number);
	bool is_in_use(uint32_t trunk_number);
	uint32_t peer_message_handler(Connector::Conn_Info *conn_info, uint32_t phys_line_trunk_number, uint32_t message, void *data = NULL);

};

} /* End namespace trunk */

extern Trunk::Trunk Trunks;
