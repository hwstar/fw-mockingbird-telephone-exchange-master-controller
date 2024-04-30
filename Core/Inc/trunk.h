#pragma once
#include "top.h"
#include "xps_logical.h"
#include "trunk.h"



namespace Trunk {

const uint8_t MAX_TRUNK_CARDS = 3;
const uint8_t TRUNK_CARD_I2C_ADDRESS = 0x20;
const uint8_t EVENT_MESSAGE_LENGTH = 1;

/* Registers for commands and status */
enum {REG_GET_EVENT=0, REG_NONE=0, REG_GET_BUSY_STATUS=1, REG_SEIZE_TRUNK=2, REG_SEND_WINK=3, REG_INCOMING_CONNECTED=4,
REG_DROP_CALL=5, REG_OUTGOING_ADDR_COMPLETE=6};

/* Events received from card */
enum {EV_NONE=0, EV_REQUEST_IR=1, EV_CALL_DROPPED=2, EV_NO_WINK=3, EV_SEND_ADDR_INFO=4, EV_FAREND_SUPV=5, EV_FAREND_DISC=6, EV_BUSY = 7};

/* Trunk states */
enum {TS_IDLE=0, TS_RESET};


typedef struct Trunk_Info {
	uint8_t state;
	bool junctor_seized;
	int16_t tone_plant_descriptor;
	int16_t mf_receiver_descriptor;
	int16_t dtmf_receiver_descriptor;
	XPS_Logical::Junctor_Info jinfo;


}Trunk_Info;

class Trunk {
protected:
	uint8_t _trunk_to_service;
	osMutexId_t _lock;
	Trunk_Info _trunk_info[MAX_TRUNK_CARDS];



public:
	void event_handler(uint32_t event_type, uint32_t resource);
	void init(void);
	void poll(void);

};

} /* End namespace trunk */

extern Trunk::Trunk Trunks;
