#include "top.h"
#include "logging.h"
#include "card_comm.h"
#include "sub_line.h"

namespace Sub_Line {

static const char *TAG = "subline";

const osEventFlagsAttr_t event_flags_attributes = {
		"SubLineEventFlags",
		0,
		NULL,
		0
};

/* Mutex attributes */
static const osMutexAttr_t lock_mutex_attr = {
	"SubLineMutex",
	osMutexRecursive | osMutexPrioInherit,
	NULL,
	0U
};

/*
 * Event handler, processes events from a line on a dual line card
 */

void Sub_Line::event_handler(uint32_t event_type, uint32_t resource) {
	LOG_DEBUG(TAG, "Received event from line card. Type: %u, Resource: %u", event_type, resource);

}

/*
 * Initialization function - Called once after RTOS is running
 */

void Sub_Line::init(void) {

	/* Create mutex to protect subscriber line data between tasks */

	this->_lock = osMutexNew(&lock_mutex_attr);
	if (this->_lock == NULL) {
			LOG_PANIC(TAG, "Could not create lock");
		  }
	/* Create event flags */
	this->_event_flags = osEventFlagsNew(&event_flags_attributes);


}

/*
 * Set the power on state of a physical line
 */


void Sub_Line::set_power_state(uint32_t line, bool state) {

	if(line >= (MAX_DUAL_LINE_CARDS * 2)) {
		LOG_PANIC(TAG, "Invalid physical line number");
	}



}



} /* End namespace Sub_Line */
