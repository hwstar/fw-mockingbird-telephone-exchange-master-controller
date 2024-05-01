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
 * Callback to indicate command completed and that error status should be checked.
 */

static void __event_callback_command(uint32_t type, uint32_t status, uint32_t trans_id) {
	if(status != I2C_Engine::I2CEC_OK) {
		LOG_ERROR(TAG, "I2C bus transaction error, status = %u", status);
	}

}


/*
 * Callback to get event info from card
 */

static void __event_callback(uint32_t type, uint32_t status, uint32_t trans_id) {
	Card_comm._event_callback(type, status, trans_id);

}

void Card_Comm::_event_callback(uint32_t type, uint32_t status, uint32_t trans_id) {

	uint32_t card = trans_id;

	if(card >= Atten::MAX_NUM_CARDS) {
		LOG_PANIC(TAG, "Invalid card number");
	}

	if(status != I2C_Engine::I2CEC_OK) {
		LOG_ERROR(TAG, "Get event failed with status %u on card %u", status, card);
		this->_pending_bits &= ~(1 << card);
		return;
	}

	if(card < Sub_Line::MAX_DUAL_LINE_CARDS) {
		/* Dual line card */
		if(this->_event_handler) {
			uint32_t type = this->_i2c_read_data[card][0]; /* Event type */
			uint32_t resource = (2 * trans_id) + this->_i2c_read_data[card][1]; /* Physical subscriber line */
			(*this->_event_handler)(type, resource);
		}
	}
	else {
		/* E&M trunk card */
		if(this->_event_handler) {
			uint32_t type = this->_i2c_read_data[card][0]; /* Event type */
			uint32_t resource = card - Sub_Line::MAX_DUAL_LINE_CARDS;
			(*this->_event_handler)(type, resource);
		}
	}
	/* Clear the card's pending bit */
	this->_pending_bits &= ~(1 << card);
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

bool Card_Comm::queue_get_event_request(uint32_t card, Event_Handler handler) {
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
		LOG_DEBUG(TAG, "Card %d asserted ATTEN", card);
		this->_event_handler = handler;

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

/*
 * Send a command to a line or trunk card
 */

bool Card_Comm::send_command(uint32_t resource_type, uint32_t resource, uint32_t command,  uint32_t parameter) {
	uint32_t card;
	uint32_t line;
	bool res = true;
	uint8_t i2c_write_data[2];

	/* Sanity checking */
	if((resource_type != RT_LINE) && (resource_type != RT_TRUNK)) {
		LOG_PANIC(TAG, "Bad resource type");
	}

	if(resource_type == RT_LINE) {
		if(resource >= (Sub_Line::MAX_DUAL_LINE_CARDS * 2)) {
			LOG_PANIC(TAG, "Bad physical line number");
		}
	}
	else {
		if(resource >= Trunk::MAX_TRUNK_CARDS) {
			LOG_PANIC(TAG, "Bad trunk number");
		}
	}

	//LOG_DEBUG(TAG, "Sending command %u to resourse %u, resource type %u, parameter %u",
	//			command, resource, resource_type, parameter);
	/* Get the lock */
	osStatus status = osMutexAcquire(this->_lock, osWaitForever);

	if(status != osOK) {
			LOG_PANIC(TAG, "Lock acquisition failed, RTOS status %d", status);
		}

	if(resource_type == RT_LINE) {
		uint32_t data_length = 1;
		card = resource >> 1;
		line = resource & 1;

		/* Set the card line number (0 or 1) */
		i2c_write_data[0] = line;


		if((command == Sub_Line::REG_POWER_CTRL) || (command == Sub_Line::REG_REQUEST_RINGING)) {
			/* Handle 2 byte commands */
			i2c_write_data[1] = parameter;
			data_length++;

		}

		res = I2c.queue_transaction(I2C_Engine::I2CT_WRITE_REG8, 0, card, Sub_Line::LINE_CARD_I2C_ADDRESS,
			command, data_length, i2c_write_data, __event_callback_command, card);

		if(!res) {
			LOG_ERROR(TAG, "I2C Queue Transaction failed due to full queue");
		}


	}
	else { /* RT_TRUNK */
		card = resource + Sub_Line::MAX_DUAL_LINE_CARDS;
		res = I2c.queue_transaction(I2C_Engine::I2CT_WRITE_REG8, 0, card, Trunk::TRUNK_CARD_I2C_ADDRESS,
			command, 0, i2c_write_data, __event_callback_command, card);

		if(!res) {
			LOG_ERROR(TAG, "I2C Queue Transaction failed due to full queue");
		}
	}

	osMutexRelease(this->_lock); /* Release the lock */

	return res;


}




} /* End namespace Card Comm */

Card_Comm::Card_Comm Card_comm;


