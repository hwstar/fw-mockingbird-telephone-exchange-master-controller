#pragma once
#include "top.h"
#include "drv_atten.h"


namespace Card_Comm {
const uint8_t READ_DATA_MAX_LENGTH = 2;

class Card_Comm {

public:
	void _event_callback(uint8_t status, uint32_t trans_id);
	void init(void);
	bool queue_info_request(uint32_t card);

protected:
	osMutexId_t _lock;
	uint8_t _pending_bits;
	uint8_t _i2c_read_data[Atten::MAX_NUM_CARDS][READ_DATA_MAX_LENGTH];


};



} /* End namespace Card Comm */

extern Card_Comm::Card_Comm Card_comm;

