#include "top.h"
#include "logging.h"
#include "card_comm.h"
#include "xps_logical.h"
#include "tone_plant.h"
#include "drv_dtmf.h"
#include "connector.h"
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
 * DTMF receiver callback
 */

static void __digit_receiver_callback(int32_t descriptor, char digit, uint32_t parameter) {
	Sub_line._digit_receiver_callback(descriptor, digit, parameter);
}

void Sub_Line::_digit_receiver_callback(int32_t descriptor, char digit, uint32_t parameter) {
 	LOG_DEBUG(TAG, "Received DTMF digit '%c' from line '%u', receiver %d", digit, parameter, (int) descriptor);
}


/*
 * Event handler, processes events from a line on a dual line card
 */

void Sub_Line::event_handler(uint32_t event_type, uint32_t resource) {
	LOG_DEBUG(TAG, "Received event from physical line %u Type: %u ", resource, event_type);

	this->_lock = osMutexNew(&lock_mutex_attr); /* Get the lock */

	Connector::Conn_Info *linfo = &this->_conn_info[resource];

	/* Subscriber lifted receiver */
	if((linfo->state == LS_IDLE) && (event_type == EV_REQUEST_OR)) {
		linfo->state = LS_SEIZE_JUNCTOR;
	}
	else if (event_type == EV_HUNGUP) {
		/* Subscriber hung up */
		if((linfo->state == LS_SEIZE_JUNCTOR ) ||
			(linfo->state == LS_SEIZE_TG) ||
			(linfo->state == LS_SEIZE_DTMFR) ||
			(linfo->state == LS_WAIT_ROUTE)) {
			linfo->state = LS_RESET;
		}
	}


	osMutexRelease(this->_lock); /* Release the lock */


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

	for(uint8_t index = 0; index < MAX_DUAL_LINE_CARDS * 2; index++) {
		Connector::Conn_Info *linfo = &this->_conn_info[index];

		linfo->state = LS_IDLE;
		linfo->tone_plant_descriptor = linfo->mf_receiver_descriptor = linfo->dtmf_receiver_descriptor = -1;
	}
}

/*
 * Polling
 */

void Sub_Line::poll(void) {
	osMutexAcquire(this->_lock, osWaitForever); /*Get the lock */


	Connector::Conn_Info *linfo = &this->_conn_info[this->_line_to_service];

	switch(linfo->state) {
	case LS_IDLE:
		/* Wait for Request outgoing register */
		break;

	case LS_SEIZE_JUNCTOR:
		if(Xps_logical.seize(&linfo->jinfo)) {
				linfo->junctor_seized = true;
				linfo->state = LS_SEIZE_TG;
			}
		break;


	case LS_SEIZE_TG:
		if((linfo->tone_plant_descriptor = Tone_plant.channel_seize()) > -1) {
			linfo->state = LS_SEIZE_DTMFR;
		}
		break;


	case LS_SEIZE_DTMFR:
		if((linfo->dtmf_receiver_descriptor = Dtmf_receivers.seize(__digit_receiver_callback, this->_line_to_service)) > -1) {
			Tone_plant.send_call_progress_tones(linfo->dtmf_receiver_descriptor, Tone_Plant::CPT_DIAL_TONE);
			/* Connect phone to junctor */
			Xps_logical.connect_phone_orig(&linfo->jinfo, this->_line_to_service);
			/* Connect DTMF receiver to junctor */
			Xps_logical.connect_dtmf_receiver(&linfo->jinfo, linfo->dtmf_receiver_descriptor);
			/* Connect tone generator to junctor */
			Xps_logical.connect_tone_plant_output(&linfo->jinfo, linfo->tone_plant_descriptor);
			linfo->state = LS_WAIT_ROUTE;
		}

		break;

	case LS_WAIT_ROUTE:
		break;

	case LS_RESET:
		/* Release any resources first */
		if(linfo->tone_plant_descriptor != -1) {
			Tone_plant.channel_release(linfo->tone_plant_descriptor);
			linfo->tone_plant_descriptor = -1;
		}
		if(linfo->mf_receiver_descriptor != -1) {
				MF_decoder.release(linfo->mf_receiver_descriptor);
				linfo->mf_receiver_descriptor = -1;
			}
		if(linfo->dtmf_receiver_descriptor != -1) {
				Dtmf_receivers.release(linfo->dtmf_receiver_descriptor);
				linfo->dtmf_receiver_descriptor = -1;
			}

		/* Then release the junctor if we seized it initially */
		if(linfo->junctor_seized) {
			Xps_logical.release(&linfo->jinfo);
			linfo->junctor_seized = false;
		}
		linfo->state = LS_IDLE;
		break;

	default:
		linfo->state = LS_RESET;
		break;

	}

	this->_line_to_service++;
	if(this->_line_to_service >= (MAX_DUAL_LINE_CARDS * 2)) {
		this->_line_to_service = 0;
	}


	osMutexRelease(this->_lock); /* Release the lock */
}

/*
 * Set the power on state of a physical line
 */


void Sub_Line::set_power_state(uint32_t line, bool state) {

	if(line >= (MAX_DUAL_LINE_CARDS * 2)) {
		LOG_PANIC(TAG, "Invalid physical line number");
	}
	/* ToDo: Send command to card to power off */


}



} /* End namespace Sub_Line */
