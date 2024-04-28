#include "top.h"
#include "i2c_engine.h"
#include "logging.h"

I2C_Engine::I2C_Engine I2c;

namespace I2C_Engine {

const char *TAG = "i2cengine";

const osEventFlagsAttr_t event_flags_attributes = {
		"I2cEngineEventFlags",
		0,
		NULL,
		0
};

const osMessageQueueAttr_t queue_I2C_transactions_attributes = {
  .name = "Queue_I2C_Transactions"
};

const osMessageQueueAttr_t queue_I2C_Busses_attributes = {
  .name = "Queue_I2C_Busses"
};

/* Worker thread attributes */
static const osThreadAttr_t worker_attr = {
		"I2CEngineWorkerThread",
		osThreadDetached,
		NULL,
		0,
		NULL,
		1024,
		osPriorityHigh,
		0,
		0
};


/*
 * Trick to call a C++ method from the RTOS
 */

static void _worker(void *args) {
	I2c.worker(args);
}

/*
 * Test I2C message for expected message type
 */

bool I2C_Engine::_check_i2c_message(I2C_Queue_Message *m, uint8_t expected_message) {
	if(m->type == expected_message) {
		return true;
	}
	else {
		switch(m->type) {
			case MSG_I2C_ERR:
				/* LOG_DEBUG(TAG, "I2C Transaction error. Error code: %d", m->type); */
				break;

			default:
				LOG_ERROR(TAG, "Got unexpected transaction message: %d, expecting: %d", m->type, expected_message );
				break;

		}
	}
	return false;
}
/*
 * Called before RTOS initialization
 */

void I2C_Engine::init(void) {
	/* Create event flags */
	this->_event_flags = osEventFlagsNew(&event_flags_attributes);

	/* Create i2c busses queue */
	this->_queue_i2c_busses = osMessageQueueNew (4, sizeof(I2C_Queue_Message), &queue_I2C_Busses_attributes);

	/* Create queue_I2C_transactions */
	this->_queue_i2c_transactions = osMessageQueueNew (I2C_TRANSACTION_QUEUE_DEPTH, sizeof(I2C_Transaction), &queue_I2C_transactions_attributes);

	/* Create worker task */
	if(osThreadNew(_worker, NULL, &worker_attr) == NULL) {
		LOG_PANIC(TAG, "Could not start worker thread");
	}

}

/*
 * This function is used to place an i2c transaction in the outgoing queue
 *
 * Return true if successful, false if otherwise
 *
 * Type can be I2CT_READ_REG8 or I2CT_WRITE_REG8
 * Bus can be 0 or 1
 * The expander_channel parameter can be  -1 if no expander is used, otherwise a channel number from 0 to 7.
 * Device Address is a 7 bit I1C drvice address
 * Register Address is the register address on the device being accessed.
 * Data length is the number of bytes of read or write data to send.
 * Register data is a pointer to the read or write buffer.
 * Callback is the address of the callback function. The callback parameter is optional.
 * Trans ID is an optional transaction number. Will default to 0 if not passed in.
 *
 * Returns true if successful, else false if the queue is full.
 */


bool I2C_Engine::queue_transaction(uint32_t type, uint32_t bus, int32_t expander_channel, uint32_t device_address,
		uint32_t register_address, uint32_t data_length, uint8_t *register_data,
		I2C_Callback_Type callback, uint32_t trans_id) {
	I2C_Transaction trans;

	/*  Sanity check parameters */
	if((type >= I2CT_MAX_I2C_TYPES) || (device_address > 0x7F) || (expander_channel > 7) || (expander_channel < -1) ||
			(bus >= NUM_I2C_BUSSES) || (data_length > MAX_I2C_REG_DATA) || (!register_data) ) {
		LOG_PANIC(TAG, "Bad parameters passed in");
	}
	trans.id = trans_id;
	trans.type = type;
	trans.bus_num = bus;
	trans.expander_channel = (int8_t) expander_channel;
	trans.device_address = device_address;
	trans.device_address8 = device_address << 1;
	trans.register_address = register_address;
	trans.data_length = data_length;
	if(type == I2CT_READ_REG8) {
		trans.read_data = register_data;
	}
	else {
		trans.read_data = NULL;
		if(data_length) {
			memcpy(trans.local_register_data + 1, register_data, data_length);
		}
	}
	trans.callback = callback;

	/* Choose the correct I2C bus handle */
	switch(trans.bus_num) {
		case 0:
			trans.bus = &hi2c1;
			break;

		case 1:
			trans.bus = &hi2c4;
			break;

	}
	/* Queue Transaction */
	osStatus_t status;



	osKernelLock(); /* Critical section start */

	status = osMessageQueuePut(this->_queue_i2c_transactions, &trans, 0U, 0U );
	if(status == osOK) {
		osEventFlagsSet(this->_event_flags, EVENT_FLAG_WORK);
		osKernelUnlock(); /* Critical section end */
		return true;
	}
	else {
		osKernelUnlock(); /* Critical section end */
		LOG_ERROR(TAG, "I2C transaction put failed, os status: %d", (uint8_t) status);
		return false;
	}
}


/*
 * Return TRUE if working
 */

bool I2C_Engine::is_working(void) {
		return this->_working;
	}


/*
 * Called repeatedly after RTOS initialization
 */

void I2C_Engine::worker(void *args) {
	osStatus_t status;
	I2C_Queue_Message msg;
	uint32_t event_flag;
	int res;

	for(;;) {
		if(this->_state == I2CS_IDLE) { /* If we need to wait for work */
			event_flag = osEventFlagsWait(this->_event_flags, EVENT_FLAG_WORK, osFlagsNoClear, osWaitForever );
			this->_working = true;
		}
		else if ((this->_state == I2CS_EXP_WRITE_WAIT) || /* If we need to wait for an I2C interrupt */
				(this->_state == I2CS_READ_REG_WAIT_REG_XMIT) ||
				(this->_state == I2CS_READ_REG_WAIT_RCV) ||
				(this->_state == I2CS_WRITE_REG_WAIT_DATA_XMIT)) {
			event_flag = osEventFlagsWait(this->_event_flags, EVENT_FLAG_ISR, 0, osWaitForever);
		}


		/* Process an I2C interrupt event if there is one */
		if((event_flag & EVENT_FLAG_ISR) == EVENT_FLAG_ISR) {
			status = osMessageQueueGet(this->_queue_i2c_busses, &msg, NULL, osWaitForever);
			if (status == osOK) {
				event_flag &= ~ EVENT_FLAG_ISR;
				this->_i2c_msg_ready = true;
			}
			else {
				LOG_ERROR(TAG, "osMessageQueGet() failed");
			}
		}


		switch(this->_state) {
			case I2CS_IDLE: /* Look for work */
				osKernelLock(); /* Critical section start */
				if (osMessageQueueGetCount(this->_queue_i2c_transactions)) {
					status = osMessageQueueGet(this->_queue_i2c_transactions, &this->trans, NULL, 0);
					if (status == osOK) {
						/* Decode Transaction Type */
						osKernelUnlock(); /* Critical section end */
						if(this->trans.expander_channel >= 0) {
							/* Set up I2C expander first */
							this->trans.local_register_data[0] = (1 << this->trans.expander_channel);
							this->_i2c_msg_ready = false;
							/* Do write transaction for expander */
							res = HAL_I2C_Master_Transmit_DMA(this->trans.bus, (I2C_BUS_EXPANDER_ADDRESS << 1), this->trans.local_register_data, 1);
							if (res != HAL_OK) {
								LOG_ERROR(TAG, "HAL_I2C_Master_Transmit_DMA failed");
								this->trans.hal_i2c_error_code = this->trans.bus->ErrorCode;
								this->trans.status = I2CEC_DMA_FAILED;
								this->_state = I2CS_FINISH;
							}
							else {
								this->_state = I2CS_EXP_WRITE_WAIT;
							}
						}
						else {
							this->_state = I2CS_DO_REGISTER_RW;
						}
					}
					else {
						osKernelUnlock(); /* Critical section end */
						LOG_ERROR(TAG, "osMessageQueGet() failed");
					}
				}
				else {
					/* No work left in queue, clear the event flag */
					osEventFlagsClear(this->_event_flags, EVENT_FLAG_WORK );
					event_flag &= ~ EVENT_FLAG_WORK;
					this->_working = false;
					osKernelUnlock(); /* Critical section end */
				}
				break;


			case I2CS_EXP_WRITE_WAIT: /* Wait for bus expander write to complete */
				if (this->_i2c_msg_ready) {
					this->_i2c_msg_ready = false;
					if (this->_check_i2c_message(&msg, MSG_I2C_TX)) { /* Expected response */
						this->_state = I2CS_DO_REGISTER_RW;
						}
					else { /* Unexpected response */
						this->trans.hal_i2c_error_code = this->trans.bus->ErrorCode;
						if (this->trans.hal_i2c_error_code == HAL_I2C_ERROR_AF) {
							this->trans.status = I2CEC_NO_DEVICE;
						}
						else {
							this->trans.status = I2CEC_TRANS_FAILED;
						}
						this->_state = I2CS_FINISH;
					}
				}
				break;


			case I2CS_DO_REGISTER_RW: /* Set the I2C transaction state based on the type */
				switch (this->trans.type) {
						case I2CT_READ_REG8:
							this->_state = I2CS_READ_REG;
							break;
						case I2CT_WRITE_REG8:
							this->_state = I2CS_WRITE_REG;
							break;
					}
				break;


			case I2CS_READ_REG:  /* Start I2C register read */
				this->_i2c_msg_ready = false;
				/* Send write register address transaction */
				res = HAL_I2C_Master_Transmit_DMA(this->trans.bus, this->trans.device_address8, &this->trans.register_address, 1);
				if (res != HAL_OK) {
					LOG_ERROR(TAG, "HAL_I2C_Master_Transmit_DMA failed");
					this->trans.hal_i2c_error_code = this->trans.bus->ErrorCode;
					this->trans.status = I2CEC_DMA_FAILED;
					this->_state = I2CS_FINISH;
				}
				else { /* Write register address DMA was started */
					this->_state = I2CS_READ_REG_WAIT_REG_XMIT;
				}
				break;


			case I2CS_READ_REG_WAIT_REG_XMIT: /* Wait for transmit register address to complete */
				if (this->_i2c_msg_ready) {
					this->_i2c_msg_ready = false;
					if (this->_check_i2c_message(&msg, MSG_I2C_TX)) {
						/* Expected response. Get the register data */
						res = HAL_I2C_Master_Receive_DMA(this->trans.bus, this->trans.device_address8, this->trans.local_register_data, this->trans.data_length);
						if (res != HAL_OK) {
							LOG_ERROR(TAG, "HAL_I2C_Master_Receive_DMA failed");
							this->trans.hal_i2c_error_code = this->trans.bus->ErrorCode;
							this->trans.status = I2CEC_DMA_FAILED;
							this->_state = I2CS_FINISH;
						}
						else {
							this->_state = I2CS_READ_REG_WAIT_RCV;
						}

					}
					else { /* Unexpected response */
						this->trans.hal_i2c_error_code = this->trans.bus->ErrorCode;
						if (this->trans.hal_i2c_error_code == HAL_I2C_ERROR_AF) {
							this->trans.status = I2CEC_NO_DEVICE;
						}
						else {
							this->trans.status = I2CEC_TRANS_FAILED;
						}
						this->_state = I2CS_FINISH;
					}
				}

				break;

			case I2CS_READ_REG_WAIT_RCV: /* Wait for read data to be received */
				if (this->_i2c_msg_ready) {
					this->_i2c_msg_ready = false;
					if (this->_check_i2c_message(&msg, MSG_I2C_RX)) { /* Expected response */
						//LOG_DEBUG(TAG,"I2C Read Register Complete");
						this->trans.status = I2CEC_OK;
						this->_state = I2CS_FINISH;
						}
					else { /* Unexpected response */
						this->trans.hal_i2c_error_code = this->trans.bus->ErrorCode;
						this->trans.status = I2CEC_TRANS_FAILED;
						this->_state = I2CS_FINISH;
					}
				}
				break;


			case I2CS_WRITE_REG: /* Start I2C register write */
				this->_i2c_msg_ready = false;
				/* Send write register transaction */
				/* Prepend register address and copy data to local buffer */
				trans.local_register_data[0] = trans.register_address;

				/* We write the register address and the data as one combined DMA transfer */
				res = HAL_I2C_Master_Transmit_DMA(this->trans.bus, this->trans.device_address8, this->trans.local_register_data, this->trans.data_length + 1);
				if (res != HAL_OK) {
					LOG_ERROR(TAG, "HAL_I2C_Master_Transmit_DMA failed");
					this->trans.hal_i2c_error_code = this->trans.bus->ErrorCode;
					this->trans.status = I2CEC_DMA_FAILED;
					this->_state = I2CS_FINISH;
				}
				else {
					this->_state = I2CS_WRITE_REG_WAIT_DATA_XMIT;
				}
				break;

			case I2CS_WRITE_REG_WAIT_DATA_XMIT: /* Wait for write data to be transmitted */
				if (this->_i2c_msg_ready) {
					this->_i2c_msg_ready = false;
					if (this->_check_i2c_message(&msg, MSG_I2C_TX)) { /* Expected response */
						this->trans.status = I2CEC_OK;
						this->_state = I2CS_FINISH;
						}
					else { /* Unexpected response */
						this->trans.hal_i2c_error_code = this->trans.bus->ErrorCode;
						if (this->trans.hal_i2c_error_code == HAL_I2C_ERROR_AF) {
							this->trans.status = I2CEC_NO_DEVICE;
						}
						else {
							this->trans.status = I2CEC_TRANS_FAILED;
						}
						this->_state = I2CS_FINISH;
					}
				}
				break;

			case I2CS_FINISH: /* Final steps */
				/* If OK and the command was a read */
				if((this->trans.status == I2CEC_OK) && (this->trans.type == I2CT_READ_REG8)) {
					/* Copy the read data to the user's buffer pointer */
					if(this->trans.read_data) {
						memcpy(this->trans.read_data, this->trans.local_register_data, this->trans.data_length);
					}
				}

				/* Call the user-supplied callback function if specified*/
				if(this->trans.callback) {
					(*this->trans.callback)(this->trans.type, this->trans.status, this->trans.id);
				}
				/* Go back to Idle and look for more work */
				this->_state = I2CS_IDLE;
				break;


			default:
				this->_state = I2CS_IDLE;
				break;
		}
	} /* End for(;;) */
}


/*
 * I2C Interrupt handler
 */

void I2C_Engine::handler(I2C_HandleTypeDef *hi2c, uint32_t intr_type) {
	uint8_t bus;
	I2C_Queue_Message msg;
	if (hi2c == &hi2c1) {
		bus = 0;
	}
	else {
		bus = 1;
	}
	msg.bus = bus;
	msg.type = intr_type;
	msg.handle = hi2c;
	osMessageQueuePut(this->_queue_i2c_busses, &msg, 0U, 0U); /* Send message to I2C task */
	osEventFlagsSet(this->_event_flags, EVENT_FLAG_ISR);
}



} /* End Namespace I2C_Engine */
