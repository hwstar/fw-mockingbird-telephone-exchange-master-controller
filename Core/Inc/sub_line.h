#pragma once
#include "top.h"
#include "connector.h"

namespace Sub_Line {

const uint32_t DTMF_DIGIT_DIAL_TIME = 30000; /* 30 seconds */
const uint32_t CONGESTION_SEND_TIME = 30000; /* 30 Seconds */

const uint8_t MAX_DUAL_LINE_CARDS = 4;
const uint8_t LINE_CARD_I2C_ADDRESS = 0x30;
const uint8_t EVENT_MESSAGE_LENGTH = 2;

/* Register addresses for commands and status */
enum {REG_GET_EVENT=0, REG_GET_BUSY_STATUS=1, REG_SET_OR_ATTACHED=2, REG_SET_IN_CALL=3, REG_REQUEST_RINGING=4, REG_END_CALL=5, REG_POWER_CTRL=254};

/* Events received from card */
enum {EV_NONE=0, EV_READY=1, EV_REQUEST_OR=2, EV_DIALED_DIGIT=2, EV_HOOKFLASH=3, EV_BUSY=4, EV_RINGING=5, EV_ANSWERED=6, EV_HUNGUP=7, };


/* States */
enum {LS_IDLE=0, LS_SEIZE_JUNCTOR=1, LS_SEIZE_TG=2, LS_SEIZE_DTMFR=3, LS_WAIT_FIRST_DIGIT=4,
	LS_WAIT_ROUTE=5, LS_DIAL_TIMEOUT=6, LS_SEND_BUSY=7, LS_SEND_CONGESTION=8, LS_CONGESTION_DISCONNECT=9, LS_WAIT_ANSWER=10,
	LS_CALLED_PARTY_ANSWERED=11, LS_CALLED_PARTY_HUNGUP=12, LS_WAIT_END_CALL=13,
	LS_ORIG_DISCONNECT=14, LS_ORIG_DISCONNECT_B=15, LS_ORIG_DISCONNECT_C=16, LS_ORIG_DISCONNECT_D=17, LS_ORIG_DISCONNECT_E=18,
	LS_ORIG_DISCONNECT_F=19, LS_FAR_END_DISCONNECT=20, LS_FAR_END_DISCONNECT_B=21, LS_END_CALL=22,
	LS_WAIT_HANGUP=23, LS_RING=24, LS_RINGING=25, LS_ANSWER=26, LS_ANSWERED=27, LS_SEIZE_TRUNK=28,
	LS_WAIT_TRUNK_RESPONSE=29, LS_TRUNK_OUTGOING_RELEASE=30, LS_TRUNK_SEND_ADDR_INFO = 31, LS_TRUNK_WAIT_ADDR_SENT=32,
	LS_TRUNK_CONNECT_CALLER=33, LS_TRUNK_WAIT_SUPV=34, LS_TRUNK_ADVANCE=35, LS_RESET=255};


class Sub_Line {
protected:
	osMutexId_t _lock;
	osEventFlagsId_t _event_flags;
	uint8_t _line_to_service;
	Connector::Conn_Info _conn_info[MAX_DUAL_LINE_CARDS * 2];


public:
	void init(void);
	void config(void);
	void event_handler(uint32_t event_type, uint32_t resource);
	void set_power_state(uint32_t line, bool state);
	void _digit_receiver_callback(int32_t descriptor, char digit, uint32_t parameter);
	void poll(void);
	uint32_t peer_message_handler(Connector::Conn_Info *conn_info, uint32_t phys_line_trunk_number, uint32_t message);
	void _dial_timer_callback(void *arg);

};



} /* End namespace Sub_Line */

extern Sub_Line::Sub_Line Sub_line;
