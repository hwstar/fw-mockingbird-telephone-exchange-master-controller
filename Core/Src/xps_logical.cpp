#include "top.h"
#include "logging.h"
#include "xps_logical.h"

static const char *TAG = "xpslogical";

namespace XPS_Logical {


/*
 * Validate a descriptor passed in by the caller
 */

bool XPS_Logical::_validate_descriptor(uint32_t descriptor) {
	if((descriptor >= 0) && (descriptor >= this->_max_junctors)){
		return false;
	}
	return true;
}


/*
 * Convert logical switch coordinates to physical coordinates
 */


void XPS_Logical::_logical_to_physical(Phys_Switch *s, uint32_t x, uint32_t y) {
	if(!s) {
		LOG_PANIC(TAG, "NULL passed in");
	}

	if((x > LOGICAL_NUM_X) || (y > LOGICAL_NUM_Y)) {
		LOG_PANIC(TAG, "Invalid logical X or Y values");
	}

	s->chip = x / PHYSICAL_NUM_X;
	s->x = x % PHYSICAL_NUM_X;
	s->y = y;
}


/*
 * Close a crosspoint switch
 */

void XPS_Logical::close_switch(uint32_t x, uint32_t y) {
	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */
	uint16_t byte_addr = (x >> 3)+(y << 2);
	uint8_t bit_location = x & 7;
	uint8_t state_bits = this->_matrix_state[byte_addr];
	Phys_Switch s;

	this->_logical_to_physical(&s, x, y);

	Xps_driver.modify(s.x, s.y, s.chip, true);

	state_bits = state_bits | (1 << bit_location);


	this->_matrix_state[byte_addr] = state_bits;
	osMutexRelease(this->_lock); /* Release the lock */

}

/*
 * Open a crosspoint switch
 */

void XPS_Logical::open_switch(uint32_t x, uint32_t y) {
	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */
	uint16_t byte_addr = (x >> 3)+(y << 2);
	uint8_t bit_location = x & 7;
	uint8_t state_bits = this->_matrix_state[byte_addr];
	Phys_Switch s;

	this->_logical_to_physical(&s, x, y);

	Xps_driver.modify(s.x, s.y, s.chip, false);

	state_bits = state_bits & ~(1 << bit_location);


	this->_matrix_state[byte_addr] = state_bits;
	osMutexRelease(this->_lock); /* Release the lock */

}


/*
 * Return the state of a crosspoint switch
 */

bool XPS_Logical::get_switch_state(uint32_t x, uint32_t y) {
	if((x > LOGICAL_MAX_X) || y > LOGICAL_MAX_Y) {
		LOG_PANIC(TAG, "X or Y out of range");
	}

	uint16_t byte_addr = (x >> 3)+(y << 2);
	uint8_t bit_location = x & 7;
	uint8_t state_bits = this->_matrix_state[byte_addr];

	if(state_bits & (1 << bit_location)) {
		return true;
	}
	return false;

}

/*
 * Open all crosspoint switches
 */


bool XPS_Logical::clear() {
	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */
	/* Initialize state matrix */
	for(uint32_t i = 0; i < sizeof(this->_matrix_state); i++) {
		_matrix_state[i] = 0;
	}

	/* Clear all connections in the crosspoint chips */
	Xps_driver.clear();
	osMutexRelease(this->_lock); /* Release the lock */
	return true;

}


/*
 * This gets called once after the RTOS is up and running by the default task
 */

void XPS_Logical::init(void) {

	/* Mutex attributes */
	static const osMutexAttr_t xps_logical_mutex_attr = {
		"XPSLogicalMutex",
		osMutexRecursive | osMutexPrioInherit,
		NULL,
		0U
	};

	/* Initialize state matrix */
	for(uint32_t i = 0; i < sizeof(this->_matrix_state); i++) {
		_matrix_state[i] = 0;
	}

	/* Clear all connections in the crosspoint chips */
	Xps_driver.clear();

	/* Todo: make dynamic in the future by doing presence testing */
	this->_installed_lines = 2;
	this->_installed_trunks = 1;

	this->_max_junctors = 4;

	/* Create the lock mutex */
	this->_lock = osMutexNew(&xps_logical_mutex_attr);
	if (this->_lock == NULL) {
		LOG_PANIC(TAG, "Could not create lock");
	}
}


/*
 * Seize an available junctor.
 *
 * If the optional requested_junctor_number is passed in, try to seize it, else return
 * the next available junctor.
 *
 * Initializes the junctor info.
 *
 * Returns false if no junctors are available, else the numeric value for the junctor.
 */

bool XPS_Logical::junctor_seize(Junctor_Info *info, int32_t requested_junctor_number) {
	int32_t descriptor = (int32_t) this->_max_junctors;
	bool res = true;

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	if(requested_junctor_number == -1) {
		for(descriptor = 0; descriptor < (int32_t) this->_max_junctors; descriptor++){
			if((this->_busy_bits & (1 << descriptor)) == 0) {
				this->_busy_bits |= (1 << descriptor);
				break;
			}
		}
	}
	else { /* Test and seize a specific junctor */
		if((requested_junctor_number >= 0) && (requested_junctor_number < (int32_t) this->_max_junctors) &&
				((this->_busy_bits & (1 << requested_junctor_number)) == 0)) {
			this->_busy_bits |= (1 << requested_junctor_number);
			descriptor = requested_junctor_number; /* Grant junctor */
		}
		else {
			descriptor = (int32_t) this->_max_junctors; /* Not available */
		}
	}



	if(descriptor >= (int32_t) this->_max_junctors) {
		info->junctor_descriptor = -1;
		res = false;
	}
	else {
		/* Initialize junctor info */
		info->junctor_descriptor = descriptor;

	}
	osMutexRelease(this->_lock); /* Release the lock */

	return res;
}


/*
 * Release junctor
 */

void XPS_Logical::junctor_release(Junctor_Info *info) {
	if(!info){
		LOG_PANIC(TAG, "Invalid junctor info");
	}


	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	/* Un-busy the channel */
	this->_busy_bits &= ~(1 << info->junctor_descriptor);


	/* Reset everything in the info block */
	info->junctor_descriptor = -1;

	osMutexRelease(this->_lock); /* Release the lock */

}

/*
 * Connect originating phone to a junctor
 */

bool XPS_Logical::junctor_connect_orig(Junctor_Info *info, int32_t phone_line_num_orig) {
	return true;

}

/*
 * Disconnect originating phone from a junctor
 */

bool XPS_Logical::junctor_disconnect_orig(Junctor_Info *info) {
	return true;

}

/*
 * Connect terminating phone to a junctor
 */

bool XPS_Logical::junctor_connect_term(Junctor_Info *info, int32_t phone_line_num_term) {
	return true;

}

/*
 * Disconnect terminating phone from a junctor
 */

bool XPS_Logical::junctor_disconnect_term(Junctor_Info *info) {
	return true;

}

/*
 * Disconnect everything from a junctor
 */

bool XPS_Logical::junctor_disconnect_all(Junctor_Info *info) {


	return true;

}




} /* End namespace XPS_Logical */

XPS_Logical::XPS_Logical Xps_logical;


