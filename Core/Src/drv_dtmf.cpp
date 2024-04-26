#include "top.h"
#include "util.h"
#include "drv_dtmf.h"
#include "logging.h"


static const char *TAG = "dtmfdrv";

namespace Dtmf {


/*
 * Read the state of the strobe pin
 */

bool Dtmf::_read_stb(uint32_t receiver) {

	bool res = true;

	if(receiver >= NUM_DTMF_RECEIVERS) {
			LOG_PANIC(TAG, "Invalid receiver");
	}
	else {
		switch(receiver) {
		case 0:
			res = (HAL_GPIO_ReadPin(DTMF_STB0_GPIO_Port, DTMF_STB0_Pin) > 0);
			break;

		case 1:
			res = (HAL_GPIO_ReadPin(DTMF_STB0_GPIO_Port, DTMF_STB1_Pin) > 0);
			break;
		}
	}
	return res;
}

/*
 * Read the digit code
 */

char Dtmf::_read_digit_code(uint32_t receiver) {

	uint8_t code;
	char digit;
	static const char *digit_map = "D1234567890*#ABC";


	if(receiver >= NUM_DTMF_RECEIVERS) {
				LOG_PANIC(TAG, "Invalid receiver");
	}

	/* Drive DTMF_TOEx high */
	switch(receiver) {
	case 0:
		Utility.set_gpio_pin_state(DTMF_TOE0, true);
		break;

	case 1:
		Utility.set_gpio_pin_state(DTMF_TOE1, true);
		break;
	}

	/* Wait setup time */
	for (volatile uint8_t i=0; i<= 10; i++);

	/* Get code from the data pins */
	code = (HAL_GPIO_ReadPin(DTMF_3_GPIO_Port, DTMF_3_Pin ) << 3) +
			(HAL_GPIO_ReadPin(DTMF_2_GPIO_Port, DTMF_2_Pin ) << 2) +
			(HAL_GPIO_ReadPin(DTMF_1_GPIO_Port, DTMF_1_Pin ) << 1) +
			HAL_GPIO_ReadPin(DTMF_0_GPIO_Port, DTMF_0_Pin );

	/* Drive DTMF_TOEx low */
	switch(receiver) {
	case 0:
		Utility.set_gpio_pin_state(DTMF_TOE0, false);
		break;

	case 1:
		Utility.set_gpio_pin_state(DTMF_TOE1, false);
		break;
	}

	/* Convert digit code to ASCII character */
	code &= 0x0F;
	digit = digit_map[code];

	return digit;

}


void Dtmf::init(void) {
	static const osMutexAttr_t dtmf_mutex_attr = {
			"DtmfDecoderMutex",
			osMutexRecursive | osMutexPrioInherit,
			NULL,
			0U
		};
	this->_lock = osMutexNew(&dtmf_mutex_attr);

}

/*
 * Called by event handler appx. every 10 ms.
 */
void Dtmf::poll(void) {
	/* Get the lock */
	osStatus status = osMutexAcquire(this->_lock, osWaitForever);

	if(status != osOK) {
			LOG_PANIC(TAG, "Lock acquisition failed, RTOS status %d", status);
		}


	for(int receiver = 0; receiver < NUM_DTMF_RECEIVERS; receiver++) {
		switch(this->_state[receiver]) {
		case DS_WAIT_STB_TRUE:
			/* Wait for stb true */
			if(this->_read_stb(receiver)) {
				this->_digit[receiver] = this->_read_digit_code(receiver);
				this->_state[receiver] = DS_WAIT_STB_FALSE;
			}
			break;

		case DS_WAIT_STB_FALSE:
			if(!this->_read_stb(receiver)) {
				/* Test for active callback */
				if(this->_callback[receiver]) {
					/* Call the callback */
					(*this->_callback[receiver])(receiver, this->_digit[receiver]);
				}
				this->_state[receiver] = DS_WAIT_STB_TRUE;
			}
			break;

		default:
			this->_state[receiver] = DS_WAIT_STB_TRUE;
			break;

		}
	}


	/* Release the lock */
	osMutexRelease(this->_lock);
}

/*
 * Seize a DTMF receiver
 *
 * Returns -1 if unsuccessful, else a receiver descriptor.
 */

int32_t Dtmf::seize(Dtmf_Callback callback, int32_t receiver) {
	int32_t receiver_to_test;

	/* Validate receiver number */
	if((receiver < 0) || (receiver >= NUM_DTMF_RECEIVERS)) {
		LOG_PANIC(TAG, "Invalid receiver number");
	}

	/* Get the lock */
	osStatus status = osMutexAcquire(this->_lock, osWaitForever);

	if(status != osOK) {
		LOG_PANIC(TAG, "Lock acquisition failed, RTOS status %d", status);
	}

	if(receiver == -1) {
		/* Try to find an available receiver */
		for(receiver_to_test = 0; receiver_to_test < NUM_DTMF_RECEIVERS; receiver_to_test++) {
			uint32_t receiver_mask_bit = (1 << receiver_to_test);
			if((this->_siezed_receivers & receiver_mask_bit) == 0) {
				this->_siezed_receivers |= receiver_mask_bit;
				receiver = receiver_to_test;
			}
		}
	}
	else {
		/* Caller is requesting a specific receiver number */
		uint32_t receiver_mask_bit = (1 << receiver);
		if((this->_siezed_receivers & receiver_mask_bit) == 0) {
			this->_siezed_receivers |= receiver_mask_bit ;
		}
		else {
			receiver = -1;
		}
	}

	if(receiver != -1) { /* If successful */
		this->_callback[receiver] = callback;
	}


	/* Release the lock */
	osMutexRelease(this->_lock);
	return receiver;
}

/*
 * Release a DTMF receiver
 */

void Dtmf::release(int32_t descriptor) {

	/* Validate descriptor */
	if((descriptor < 0) || (descriptor >= NUM_DTMF_RECEIVERS)) {
		LOG_PANIC(TAG, "Invalid descriptor: %d", descriptor);
	}
	/* Get the lock */

	osStatus status = osMutexAcquire(this->_lock, osWaitForever);
	if(status != osOK) {
		LOG_PANIC(TAG, "Lock acquisition failed, RTOS status %d", status);
	}

	/* Free DTMF receiver */
	uint32_t decoder_mask_bit = (1 << descriptor);
	this->_siezed_receivers &= ~decoder_mask_bit;
	this->_callback[descriptor] = NULL;


	/* Release the lock */
	osMutexRelease(this->_lock);
}



} /* End namespace Dtmf */

/* Class instantiation for global access */
Dtmf::Dtmf Dtmf_receivers;

