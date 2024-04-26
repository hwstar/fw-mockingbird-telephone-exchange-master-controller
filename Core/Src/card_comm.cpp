#include "top.h"
#include "logging.h"
#include "card_comm.h"
#include "drv_atten.h"
#include "i2c_engine.h"
#include "sub_line.h"
#include "trunk.h"



namespace Card_Comm {

static const char *TAG = "cardcomm";


/*
 * Callback to get event info from card
 */

static void __event_callback(uint8_t status, uint32_t trans_id) {
	Card_comm._event_callback(status, trans_id);

}

void Card_Comm::_event_callback(uint8_t status, uint32_t trans_id) {

	uint32_t card = trans_id;

	if(card > Atten::MAX_NUM_CARDS) {
		LOG_PANIC(TAG, "Invalid card number");
	}

	if(card < 4) {
		LOG_DEBUG(TAG, "Got event %u from line card %u, line %u",
				card,
				this->_i2c_read_data[card][0],
				this->_i2c_read_data[card][1]);

	}
	else {
		uint32_t event = this->_i2c_read_data[card][0];
		LOG_DEBUG(TAG, "Got event %u from trunk card %u", event, card);
	}


}

void Card_Comm::init(void) {
	static const osMutexAttr_t card_comm_mutex_attr = {
			"CardCommMutex",
			osMutexRecursive | osMutexPrioInherit,
			NULL,
			0U
		};
	this->_lock = osMutexNew(&card_comm_mutex_attr);

}
/*
 * Queue a request to retrieve a card's information
 */

bool Card_Comm::queue_info_request(uint32_t card) {
	bool res = true;
	if(card >= Atten::MAX_NUM_CARDS) {
		LOG_PANIC(TAG,"Card number out of range");
	}

	/* Get the lock */
	osStatus status = osMutexAcquire(this->_lock, osWaitForever);

	if(status != osOK) {
		LOG_PANIC(TAG, "Lock acquisition failed, RTOS status %d", status);
	}


	if(this->_pending_bits & (1 << card)){
		/* Request is already in the queue */
		res = false;
	}
	else {
		/* New request */
		this->_pending_bits |= (1 << card);
		uint32_t device_address = (card >= 4) ? Trunk::TRUNK_CARD_I2C_ADDRESS : Sub_Line::LINE_CARD_I2C_ADDRESS;
		uint32_t register_address = (card >= 4) ? (uint32_t) Trunk::REG_GET_EVENT : (uint32_t) Sub_Line::REG_GET_EVENT;
		uint32_t event_message_length = (card >= 4) ? Trunk::EVENT_MESSAGE_LENGTH : Sub_Line::EVENT_MESSAGE_LENGTH;


		bool res = I2c.queue_transaction(I2C_Engine::I2CT_READ_REG8, 0, card, device_address,
				register_address, event_message_length,
				this->_i2c_read_data[card], __event_callback, card);

		if(!res) {
			LOG_ERROR(TAG, "I2C Queue Transaction failed due to full queue");
		}

	}
	osMutexRelease(this->_lock); /* Release the lock */




	return res;
}



} /* End namespace Card Comm */

Card_Comm::Card_Comm Card_comm;


