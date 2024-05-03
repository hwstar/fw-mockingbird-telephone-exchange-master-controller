#pragma once
#include "top.h"
#include "connector.h"

namespace Sub_Line {

const uint8_t MAX_DUAL_LINE_CARDS = 4;
const uint8_t LINE_CARD_I2C_ADDRESS = 0x30;
const uint8_t EVENT_MESSAGE_LENGTH = 2;

/* Register addresses for commands and status */
enum {REG_GET_EVENT=0, REG_GET_BUSY_STATUS=1, REG_SET_OR_ATTACHED=2, REG_SET_IN_CALL=3, REG_REQUEST_RINGING=4, REG_END_CALL=5, REG_POWER_CTRL=254};

/* Events received from card */
enum {EV_NONE=0, EV_READY=1, EV_REQUEST_OR=2, EV_DIALED_DIGIT=2, EV_HOOKFLASH=3, EV_BUSY=4, EV_RINGING=5, EV_ANSWERED=6, EV_HUNGUP=7, };


/* States */
enum {LS_IDLE=0, LS_SEIZE_JUNCTOR, LS_SEIZE_TG, LS_SEIZE_DTMFR, LS_WAIT_ROUTE, LS_RESET};


class Sub_Line {
protected:
	osMutexId_t _lock;
	osEventFlagsId_t _event_flags;
	uint8_t _line_to_service;
	Connector::Conn_Info _conn_info[MAX_DUAL_LINE_CARDS * 2];


public:
	void init(void);
	void event_handler(uint32_t event_type, uint32_t resource);
	void set_power_state(uint32_t line, bool state);
	void _digit_receiver_callback(int32_t descriptor, char digit, uint32_t parameter);
	void poll(void);
};



} /* End namespace Sub_Line */

extern Sub_Line::Sub_Line Sub_line;
