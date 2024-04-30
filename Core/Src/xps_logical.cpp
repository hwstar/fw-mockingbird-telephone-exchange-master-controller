#include "top.h"
#include "logging.h"
#include "util.h"
#include "mf_receiver.h"
#include "drv_dtmf.h"
#include "xps_logical.h"

static const char *TAG = "xpslogical";

namespace XPS_Logical {


/*
 * Validate a descriptor passed in by the caller
 */

bool XPS_Logical::_validate_descriptor(uint32_t descriptor) {
	if((descriptor >= 0) && (descriptor >= MAX_JUNCTORS)){
		return false;
	}
	return true;
}

/*
 * Return the x location of the mf receiver for the given descriptor
 */

uint8_t XPS_Logical::get_mf_receiver_x(int32_t mf_descriptor) {
	if((mf_descriptor < 0) || (mf_descriptor >= MF_Decoder::NUM_MF_RECEIVERS)) {
		LOG_PANIC(TAG, "Invalid descriptor");

	}
	uint8_t x = mf_descriptor * 2;

	x += MF_RECEIVER_COLUMN_START;

	return x;
}


/*
 * Return the x location of the dtmf receiver for the given descriptor
 */

uint8_t XPS_Logical::get_dtmf_receiver_x(int32_t dtmf_descriptor) {
	if((dtmf_descriptor < 0) || (dtmf_descriptor >= Dtmf::NUM_DTMF_RECEIVERS)) {
		LOG_PANIC(TAG, "Invalid descriptor");

	}
	uint8_t x = dtmf_descriptor * 2;

	x += DTMF_RECEIVER_COLUMN_START;

	return x;
}

/*
 * Return the x location of the tone plant for the given descriptor
 */

uint8_t XPS_Logical::get_tone_plant_x(int32_t tone_plant_descriptor) {
	if((tone_plant_descriptor < 0) || (tone_plant_descriptor >= Dtmf::NUM_DTMF_RECEIVERS)) {
		LOG_PANIC(TAG, "Invalid descriptor");

	}
	uint8_t x = tone_plant_descriptor * 2;

	x += TONE_PLANT_COLUMN_START;

	return x;
}

/*
 * Return the x location of the subscriber line for the given line number;
 */

uint8_t XPS_Logical::get_sub_line_x(uint8_t line_number) {
	if(line_number >= MAX_SUB_LINES) {
		LOG_PANIC(TAG, "Invalid line number");
	}
	return (line_number * 2) + LINE_COLUMN_START;
}

/*
 * Return the x location of the trunk for a given trunk number
 */

uint8_t XPS_Logical::get_trunk_x(uint8_t trunk_number) {
	if(trunk_number >= MAX_TRUNKS) {
		LOG_PANIC(TAG, "Invalid trunk number");
	}
	return (trunk_number * 2) + TRUNK_COLUMN_START;
}

/*
 * Return a y value based on the junctor number and orig_term
 */

uint8_t XPS_Logical::get_path_y(uint8_t junctor_number, bool orig_term) {
	if(junctor_number > MAX_JUNCTORS) {
		LOG_PANIC(TAG, "Invalid Junctor Number");
	}

	uint8_t y = junctor_number * 2;
	if(!orig_term) {
		y++;
	}
	return y;
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


void XPS_Logical::clear() {
	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */
	/* Initialize state matrix */
	for(uint32_t i = 0; i < sizeof(this->_matrix_state); i++) {
		_matrix_state[i] = 0;
	}

	/* Clear all connections in the crosspoint chips */
	Xps_driver.clear();
	osMutexRelease(this->_lock); /* Release the lock */
}

/**************************
 * High level methods
 **************************/

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




	/* Create the lock mutex */
	this->_lock = osMutexNew(&xps_logical_mutex_attr);
	if (this->_lock == NULL) {
		LOG_PANIC(TAG, "Could not create lock");
	}
}


/*
 * Seize an available junctor.
 *
 * If the optional requested_junctor_number is passed in, try to seize it, else use
 * the next available junctor.
 *
 * Initializes the junctor info.
 *
 * Returns false if no junctors are available, else the numeric value for the junctor.
 */

bool XPS_Logical::seize(Junctor_Info *info, int32_t requested_junctor_number) {
	int32_t descriptor = (int32_t) MAX_JUNCTORS;
	bool res = true;

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	if(requested_junctor_number == -1) {
		for(descriptor = 0; descriptor < (int32_t) MAX_JUNCTORS; descriptor++){
			if((this->_busy_bits & (1 << descriptor)) == 0) {
				this->_busy_bits |= (1 << descriptor);
				break;
			}
		}
	}
	else { /* Test and seize a specific junctor */
		if((requested_junctor_number >= 0) && (requested_junctor_number < (int32_t) MAX_JUNCTORS) &&
				((this->_busy_bits & (1 << requested_junctor_number)) == 0)) {
			this->_busy_bits |= (1 << requested_junctor_number);
			descriptor = requested_junctor_number; /* Grant junctor */
		}
		else {
			descriptor = (int32_t) MAX_JUNCTORS; /* Not available */
		}
	}



	if(descriptor >= (int32_t) MAX_JUNCTORS) {
		info->junctor_descriptor = -1;
		res = false;
	}
	else {
		Utility.memset(info, 0, sizeof(Junctor_Info));
		/* Initialize junctor info */
		info->junctor_descriptor = descriptor;

	}
	osMutexRelease(this->_lock); /* Release the lock */

	return res;
}


/*
 * Release junctor
 */

void XPS_Logical::release(Junctor_Info *info) {
	if(!info){
		LOG_PANIC(TAG, "NULL Junctor info pointer");
	}

	/* Open all closed switches */
	this->disconnect_all(info);

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */


	/* Un-busy the channel */
	this->_busy_bits &= ~(1 << info->junctor_descriptor);


	/* Indicate not active */
	info->junctor_descriptor = -1;

	osMutexRelease(this->_lock); /* Release the lock */

}

/*
 * Connect originating phone to a junctor
 */

void XPS_Logical::connect_phone_orig(Junctor_Info *info, int32_t phone_line_num_orig) {


	if(!info) {
		LOG_PANIC(TAG, "NULL Junctor info pointer");
	}

	if((info->connections.orig_send.resource == RSRC_NONE) && (info->connections.orig_recv.resource == RSRC_NONE)) {
		uint8_t x = this->get_sub_line_x(phone_line_num_orig);
		uint8_t y = this->get_path_y(info->junctor_descriptor);
		/* We make 2 connections here */
		/* X 0 */
		/* 0 X */
		info->connections.orig_recv.resource = info->connections.orig_send.resource = RSRC_LINE;
		info->connections.orig_recv.x = x;
		info->connections.orig_recv.y = y;
		this->close_switch(x, y);
		info->connections.orig_send.x = x + 1;
		info->connections.orig_send.y = y + 1;
		this->close_switch(info->connections.orig_send.x, info->connections.orig_send.y);
	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}
}

/*
 * Disconnect originating phone from a junctor
 */

void XPS_Logical::disconnect_phone_orig(Junctor_Info *info) {
	if(!info) {
		LOG_PANIC(TAG, "NULL Junctor info pointer");
	}

	if((info->connections.orig_recv.resource == RSRC_LINE) && (info->connections.orig_send.resource == RSRC_LINE)) {
		this->open_switch(info->connections.orig_recv.x, info->connections.orig_recv.y);
		this->open_switch(info->connections.orig_send.x, info->connections.orig_send.y);
		info->connections.orig_recv.resource = info->connections.orig_send.resource = RSRC_NONE;
	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}
}

/*
 * Connect terminating phone to a junctor
 */

void XPS_Logical::connect_phone_term(Junctor_Info *info, int32_t phone_line_num_term) {


	if(!info) {
		LOG_PANIC(TAG, "NULL Junctor info pointer");
	}

	if((info->connections.term_send.resource == RSRC_NONE) && (info->connections.term_send.resource == RSRC_NONE)) {
		uint8_t x = this->get_sub_line_x(phone_line_num_term);
		uint8_t y = this->get_path_y(info->junctor_descriptor);
		/* We make 2 connections here */
		/* 0 X */
		/* X 0 */
		info->connections.term_recv.resource = info->connections.term_send.resource = RSRC_LINE;
		info->connections.term_recv.x = x;
		info->connections.term_recv.y = y + 1;
		this->close_switch(info->connections.term_recv.x, info->connections.term_recv.y);
		info->connections.term_send.x = x + 1;
		info->connections.term_send.y = y;
		this->close_switch(info->connections.term_send.x, info->connections.term_send.y);

	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}


}


/*
 * Disconnect terminating phone from a junctor
 */

void XPS_Logical::disconnect_phone_term(Junctor_Info *info) {
	if(!info) {
		LOG_PANIC(TAG, "NULL Junctor info pointer");
	}

	if((info->connections.term_recv.resource == RSRC_LINE) && (info->connections.term_send.resource == RSRC_LINE)) {
		this->open_switch(info->connections.term_recv.x, info->connections.term_recv.y);
		this->open_switch(info->connections.term_send.x, info->connections.term_send.y);
		info->connections.term_recv.resource = info->connections.term_send.resource = RSRC_NONE;
	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}

}

/*
 * Connect originating trunk to a junctor
 */

void XPS_Logical::connect_trunk_orig(Junctor_Info *info, int32_t trunk_num_orig) {
	if(!info) {
		LOG_PANIC(TAG, "NULL Junctor info pointer");
	}

	if((info->connections.orig_send.resource == RSRC_NONE) && (info->connections.orig_recv.resource == RSRC_NONE)) {
		uint8_t x = this->get_trunk_x(trunk_num_orig);
		uint8_t y = this->get_path_y(info->junctor_descriptor);
		/* We make 2 connections here */
		/* X 0 */
		/* 0 X */
		info->connections.orig_recv.resource = info->connections.orig_send.resource = RSRC_TRUNK;
		info->connections.orig_recv.x = x;
		info->connections.orig_recv.y = y;
		this->close_switch(x, y);
		info->connections.orig_send.x = x + 1;
		info->connections.orig_send.y = y + 1;
		this->close_switch(info->connections.orig_send.x, info->connections.orig_send.y);
	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}

}

/*
 * Disconnect originating phone from a junctor
 */

void XPS_Logical::disconnect_trunk_orig(Junctor_Info *info) {
	if(!info) {
		LOG_PANIC(TAG, "NULL Junctor info pointer");
	}

	if((info->connections.orig_recv.resource == RSRC_TRUNK) && (info->connections.orig_send.resource == RSRC_TRUNK)) {
		this->open_switch(info->connections.orig_recv.x, info->connections.orig_recv.y);
		this->open_switch(info->connections.orig_send.x, info->connections.orig_send.y);
		info->connections.orig_recv.resource = info->connections.orig_send.resource = RSRC_NONE;
	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}

}

/*
 * Connect terminating phone to a junctor
 */

void XPS_Logical::connect_trunk_term(Junctor_Info *info, int32_t trunk_num_term) {


	if(!info) {
		LOG_PANIC(TAG, "NULL Junctor info pointer");
	}

	if((info->connections.term_send.resource == RSRC_NONE) && (info->connections.term_send.resource == RSRC_NONE)) {
		uint8_t x = this->get_trunk_x(trunk_num_term);
		uint8_t y = this->get_path_y(info->junctor_descriptor);
		/* We make 2 connections here */
		/* 0 X */
		/* X 0 */
		info->connections.term_recv.resource = info->connections.term_send.resource = RSRC_TRUNK;
		info->connections.term_recv.x = x;
		info->connections.term_recv.y = y + 1;
		this->close_switch(info->connections.term_recv.x, info->connections.term_recv.y);
		info->connections.term_send.x = x + 1;
		info->connections.term_send.y = y;
		this->close_switch(info->connections.term_send.x, info->connections.term_send.y);

	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}


}

/*
 * Disconnect terminating phone from a junctor
 */

void XPS_Logical::disconnect_trunk_term(Junctor_Info *info) {
	if(!info) {
		LOG_PANIC(TAG, "NULL Junctor info pointer");
	}

	if((info->connections.term_recv.resource == RSRC_TRUNK) && (info->connections.term_send.resource == RSRC_TRUNK)) {
		this->open_switch(info->connections.term_recv.x, info->connections.term_recv.y);
		this->open_switch(info->connections.term_send.x, info->connections.term_send.y);
		info->connections.term_recv.resource = info->connections.term_send.resource = RSRC_NONE;
	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}

}

/*
 * Connect a tone plant output to the junctor
 */

void XPS_Logical::connect_tone_plant_output(Junctor_Info *info, int32_t tone_plant_descriptor, bool orig_term) {
	if(!info)  {
		LOG_PANIC(TAG, "NULL Junctor info pointer");
	}
	if(tone_plant_descriptor < 0) {
		LOG_PANIC(TAG, "Bad Descriptor");
	}

	uint8_t x = this->get_tone_plant_x(tone_plant_descriptor);
	uint8_t y = this->get_path_y(info->junctor_descriptor, orig_term);

	if(info->connections.tone_plant.resource == RSRC_NONE) {
		info->connections.tone_plant.resource = RSRC_TONE_PLANT;
		info->connections.tone_plant.x = x;
		info->connections.tone_plant.y = y;
		this->close_switch(x, y);

	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}



}

/*
 * Disconnect a tone plant output from the junctor
 */


void XPS_Logical::disconnect_tone_plant_output(Junctor_Info *info) {
	if(!info)  {
			LOG_PANIC(TAG, "NULL Junctor info pointer");
		}

	if(info->connections.tone_plant.resource == RSRC_TONE_PLANT) {
		this->open_switch(info->connections.tone_plant.x, info->connections.tone_plant.y);

	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}
}

/*
 * Connect a DTMF receiver to the  the junctor
 */


void XPS_Logical::connect_dtmf_receiver(Junctor_Info *info, int32_t dtmf_receiver_descriptor, bool orig_term) {

	if(!info)  {
		LOG_PANIC(TAG, "NULL Junctor info pointer");
	}
	if(dtmf_receiver_descriptor < 0) {
		LOG_PANIC(TAG, "Bad Descriptor");
	}

	uint8_t x = this->get_dtmf_receiver_x(dtmf_receiver_descriptor);
	uint8_t y = this->get_path_y(info->junctor_descriptor, orig_term);

	if(info->connections.digit_receiver.resource == RSRC_NONE) {
		info->connections.digit_receiver.resource = RSRC_DTMF_RCVR;
		info->connections.digit_receiver.x = x;
		info->connections.digit_receiver.y = y;
		this->close_switch(x, y);

	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}

}

/*
 * Disconnect a DTMF receiver to the originating portion of the junctor
 */

void XPS_Logical::disconnect_dtmf_receiver(Junctor_Info *info) {
	if(!info)  {
			LOG_PANIC(TAG, "NULL Junctor info pointer");
		}

	if(info->connections.digit_receiver.resource == RSRC_DTMF_RCVR) {
		this->open_switch(info->connections.digit_receiver.x, info->connections.digit_receiver.y);

	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}

}

/*
 * Connect a MF receiver to the the junctor
 */


void XPS_Logical::connect_mf_receiver(Junctor_Info *info, int32_t mf_receiver_descriptor, bool orig_term) {

	if(!info)  {
		LOG_PANIC(TAG, "NULL Junctor info pointer");
	}
	if(mf_receiver_descriptor < 0) {
		LOG_PANIC(TAG, "Bad Descriptor");
	}

	uint8_t x = this->get_trunk_x(mf_receiver_descriptor);
	uint8_t y = this->get_path_y(info->junctor_descriptor, orig_term);

	if(info->connections.digit_receiver.resource == RSRC_NONE) {
		info->connections.digit_receiver.resource = RSRC_MF_RCVR;
		info->connections.digit_receiver.x = x;
		info->connections.digit_receiver.y = y;
		this->close_switch(x, y);

	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}


}

/*
 * Disconnect a MF receiver from the junctor
 */

void XPS_Logical::disconnect_mf_receiver(Junctor_Info *info) {
	if(!info)  {
			LOG_PANIC(TAG, "NULL Junctor info pointer");
		}

	if(info->connections.digit_receiver.resource == RSRC_MF_RCVR) {
		this->open_switch(info->connections.digit_receiver.x, info->connections.digit_receiver.y);

	}
	else {
		LOG_PANIC(TAG, "XPS Connection");
	}


}

/*
 * Disconnect everything from a junctor
 */

void XPS_Logical::disconnect_all(Junctor_Info *info) {
	if(!info) {
		LOG_PANIC(TAG, "NULL Junctor info pointer");
	}
	if(info->connections.orig_recv.resource == RSRC_LINE) {
		this->disconnect_phone_orig(info);
	}
	if(info->connections.orig_recv.resource == RSRC_TRUNK) {
		this->disconnect_trunk_orig(info);
	}
	if(info->connections.term_recv.resource == RSRC_LINE) {
		this->disconnect_phone_term(info);
	}
	if(info->connections.term_recv.resource == RSRC_TRUNK) {
		this->disconnect_trunk_term(info);
	}
	if(info->connections.tone_plant.resource == RSRC_TONE_PLANT) {
		this->disconnect_tone_plant_output(info);
	}
	if(info->connections.digit_receiver.resource == RSRC_MF_RCVR) {
		this->disconnect_mf_receiver(info);
	}
	if(info->connections.digit_receiver.resource == RSRC_DTMF_RCVR) {
			this->disconnect_dtmf_receiver(info);
		}
}




} /* End namespace XPS_Logical */

XPS_Logical::XPS_Logical Xps_logical;


