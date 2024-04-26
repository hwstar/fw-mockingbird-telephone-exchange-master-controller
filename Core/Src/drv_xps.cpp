#include "top.h"
#include "drv_xps.h"
#include "util.h"
#include "logging.h"

static const char *TAG = "drvxps";

static const uint8_t x_map[16] = {0,1,2,3,4,5,8,9,10,11,12,13,6,7,14,15};

namespace Xps {

/*
 * Set the state of a specific CS line
 */

void Xps::_set_cs_state(uint32_t cs_number, bool state) {
	switch(cs_number) {
	case 0:
		Utility.set_gpio_pin_state(XB_SW_CS0, state);
		break;

	case 1:
		Utility.set_gpio_pin_state(XB_SW_CS1, state);
		break;

	default:
		LOG_PANIC(TAG, "Invalid CS number: %d", cs_number);

	}

}

void Xps::init(void) {

	static const osMutexAttr_t xps_mutex_attr = {
					"XPS_Mutex",
					osMutexRecursive | osMutexPrioInherit,
					NULL,
					0U
				};

	this->_lock = osMutexNew(&xps_mutex_attr);
	this->clear();
}


/*
 * Clear all crosspoint switch connections on all chips
 */

void Xps::clear(void) {
	Utility.pulse_gpio_pin(XB_SW_RESET);
}

void Xps::modify(uint32_t x, uint32_t y, uint32_t cs_number, bool state) {
	/* Check inputs */
	if((x >= MAX_ROWS) || (y >= MAX_COLUMNS)) {
		LOG_PANIC(TAG, "Invalid x or y inputs");
	}
	/* Get the lock */

	osStatus status = osMutexAcquire(this->_lock, 20U);
	if(status != osOK) {
		LOG_PANIC(TAG, "Lock acquisition failed, RTOS status %d", status);
	}

	x = x_map[x];

	/* Set up the x and y data bits */
	Utility.set_gpio_pin_state(XB_SW_X0, x & 1 );
	x >>= 1;

	Utility.set_gpio_pin_state(XB_SW_X1, x & 1 );
	x >>= 1;

	Utility.set_gpio_pin_state(XB_SW_X2, x & 1 );
	x >>= 1;

	Utility.set_gpio_pin_state(XB_SW_X3, x & 1 );


	Utility.set_gpio_pin_state(XB_SW_Y0, y & 1 );
	y >>= 1;

	Utility.set_gpio_pin_state(XB_SW_Y1, y & 1 );
	y >>= 1;

	Utility.set_gpio_pin_state(XB_SW_Y2, y & 1 );


	/* Set the data bit */

	Utility.set_gpio_pin_state(XB_SW_DATA, state);

	/* Set the chip CS */

	this->_set_cs_state(cs_number, true);

	/* Pulse the strobe line */

	Utility.pulse_gpio_pin(XB_SW_STB);

	/* Clear the chip CS */

	this->_set_cs_state(cs_number, false);


	/* Release the lock */
	osMutexRelease(this->_lock);

}



} /* End namespace Xps */

Xps::Xps Xps_driver;
