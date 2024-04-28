#pragma once
#include "top.h"

namespace I2C_Engine {

enum {I2CS_IDLE=0, I2CS_EXP_WRITE_WAIT, I2CS_DO_REGISTER_RW, I2CS_READ_REG, I2CS_READ_REG_WAIT_REG_XMIT, I2CS_READ_REG_WAIT_RCV, I2CS_WRITE_REG,
	I2CS_WRITE_REG_WAIT_REG_XMIT, I2CS_WRITE_REG_WAIT_DATA_XMIT, I2CS_FINISH};


enum {I2CT_READ_REG8=0, I2CT_WRITE_REG8, I2CT_MAX_I2C_TYPES};
enum {I2CEC_OK=0, I2CEC_NO_DEVICE, I2CEC_TRANS_FAILED, I2CEC_DMA_FAILED};
enum {EVENT_FLAG_ISR = 1, EVENT_FLAG_WORK = 2};

const uint8_t NUM_I2C_BUSSES = 1;
const uint8_t MAX_I2C_REG_DATA = 8;
const uint8_t I2C_TRANSACTION_QUEUE_DEPTH = 16;
const uint8_t I2C_BUS_EXPANDER_ADDRESS = 0x70;

/* I2C interrupt message queue codes */
const uint8_t MSG_I2C_NONE = 0;
const uint8_t MSG_I2C_RX = 1;
const uint8_t MSG_I2C_TX = 2;
const uint8_t MSG_I2C_ERR = 3;

typedef struct I2C_Queue_Message {
	uint8_t bus;
	uint8_t type;
	I2C_HandleTypeDef *handle;
} I2C_Queue_Message;

typedef I2C_Queue_Message *pI2C_Queue_Message;

typedef void (*I2C_Callback_Type)(uint32_t type, uint32_t status, uint32_t trans_id);

typedef struct I2C_Transaction {
	uint32_t hal_i2c_error_code;
	uint32_t id;
	uint8_t status;
	uint8_t type;
	uint8_t bus_num;
	int8_t expander_channel;
	uint8_t device_address;
	uint8_t device_address8;
	uint8_t register_address;
	uint8_t data_length;
	uint8_t *read_data;
	I2C_Callback_Type callback;
	I2C_HandleTypeDef *bus;
	uint8_t local_register_data[MAX_I2C_REG_DATA+1]; /* One more for write reg case to store register address */
} I2C_Transaction;



class I2C_Engine {
public:
	void init(void);
	bool queue_transaction(uint32_t type, uint32_t bus,  int32_t expander_channel, uint32_t device_address,
			uint32_t register_address, uint32_t data_length,
			uint8_t *register_data, I2C_Callback_Type callback = NULL, uint32_t trans_id = 0);
	void handler(I2C_HandleTypeDef *hi2c, uint32_t intr_type);
	bool is_working(void);
	void worker(void *args);

protected:
	bool _check_i2c_message(I2C_Queue_Message *m, uint8_t expected_message);
	bool _i2c_msg_ready;
	bool _working;
	uint8_t _state;
	I2C_Transaction trans;
	osEventFlagsId_t _event_flags;
	osMessageQueueId_t _queue_i2c_busses;
	osMessageQueueId_t _queue_i2c_transactions;
};



} /* End namespace I2C_Engine */

extern I2C_Engine::I2C_Engine I2c;

