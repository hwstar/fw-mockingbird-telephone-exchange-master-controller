#pragma once
#include "top.h"
#include "drv_atten.h"


namespace Card_Comm {
const uint8_t READ_DATA_MAX_LENGTH = 2;

enum {RT_LINE=0, RT_TRUNK}; /* Resource types */


typedef void (*Event_Handler)(uint32_t event_type, uint32_t resource );



class Card_Comm {

public:
	void _event_callback(uint32_t type, uint32_t status, uint32_t trans_id);
	void init(void);
	bool queue_get_event_request(uint32_t card, Event_Handler handler = NULL);
	bool send_command(uint32_t resource_type, uint32_t resource, uint32_t command,  uint32_t parameter = 0);


protected:
	osMutexId_t _lock;
	Event_Handler _event_handler;
	uint8_t _pending_bits;
	uint8_t _i2c_read_data[Atten::MAX_NUM_CARDS][READ_DATA_MAX_LENGTH];


};



} /* End namespace Card Comm */

extern Card_Comm::Card_Comm Card_comm;

