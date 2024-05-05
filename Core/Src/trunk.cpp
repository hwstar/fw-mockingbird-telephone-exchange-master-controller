#include "top.h"
#include "logging.h"
#include "card_comm.h"
#include "mf_receiver.h"
#include "drv_dtmf.h"
#include "tone_plant.h"
#include "xps_logical.h"
#include "connector.h"
#include "trunk.h"

Trunk::Trunk Trunks;
static const char *TAG = "trunk";

namespace Trunk {


/*
 * Called when there is an MF address to process
 */

static void __mf_receiver_callback(uint32_t descriptor, uint8_t error_code, uint8_t digit_count, char *data) {
	Trunks._mf_receiver_callback(descriptor, error_code, digit_count, data);
}

/*
 * Called from __mf_receiver_callback();
 */

void Trunk::_mf_receiver_callback(uint32_t descriptor, uint8_t error_code, uint8_t digit_count, char *data) {
	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */



	if(error_code == MF_Decoder::MFE_OK) {
		LOG_DEBUG(TAG, "Received MF address: %s from descriptor %u", data, descriptor);
	}
	else {
		LOG_ERROR(TAG, "Descriptor %d timed out waiting for MF Digits", descriptor);
		/* Todo: This needs to be properly handled */
	}

	osMutexRelease(this->_lock); /* Release the lock */


}

/*
 * This gets called when an event from a trunk card is received
 */

void Trunk::event_handler(uint32_t event_type, uint32_t resource) {


	if(resource  >= MAX_TRUNK_CARDS) {
		LOG_PANIC(TAG, "Invalid trunk number");
	}

	LOG_DEBUG(TAG, "Received event from physical trunk %u. Type: %u",resource, event_type);

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	Connector::Conn_Info *tinfo = &this->_conn_info[resource];

	/* Incoming call from trunk */
	if((tinfo->state == TS_IDLE) && (event_type == EV_REQUEST_IR)) {
		tinfo->state = TS_SEIZE_JUNCTOR;
	}
	else if(event_type == EV_CALL_DROPPED) {
		/* Call dropped by originator, check to see if we need to take action */
		if((tinfo->state == TS_SEIZE_JUNCTOR) ||
				(tinfo->state == TS_SEIZE_MFR) ||
				(tinfo->state == TS_WAIT_ADDR_INFO)) {
			tinfo->state = TS_RESET;
		}
	}


	osMutexRelease(this->_lock); /* Release the lock */


}

/* This handler receives messages from the connection peer */

uint32_t Trunk::peer_message_handler(Connector::Conn_Info *conn_info, uint32_t phys_line_trunk_number, uint32_t message) {
	/* Todo: add logic here*/

	return Connector::PMR_OK;
}


/*
 * Called once after RTOS is up.
 */

void Trunk::init(void) {
	/* Mutex attributes */
	static const osMutexAttr_t trunk_mutex_attr = {
		"XPSLogicalMutex",
		osMutexRecursive | osMutexPrioInherit,
		NULL,
		0U
	};

	/* Create the lock mutex */
	this->_lock = osMutexNew(&trunk_mutex_attr);
	if (this->_lock == NULL) {
		LOG_PANIC(TAG, "Could not create lock");
	}

	for(uint8_t index = 0; index < MAX_TRUNK_CARDS; index++) {
		Connector::Conn_Info *tinfo = &this->_conn_info[index];

		tinfo->state = TS_IDLE;
		tinfo->tone_plant_descriptor = tinfo->mf_receiver_descriptor = tinfo->dtmf_receiver_descriptor = -1;


	}


}

/*
 * Called repeatedly by event task
 */

void Trunk::poll(void) {



	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	Connector::Conn_Info *tinfo = &this->_conn_info[this->_trunk_to_service];

	switch(tinfo->state) {
	case TS_IDLE:
		/* Wait for Request IR event */
		break;

	case TS_SEIZE_JUNCTOR:
		if(Xps_logical.seize(&tinfo->jinfo)) {
			tinfo->junctor_seized = true;
			Conn.prepare(tinfo, Connector::ET_TRUNK, this->_trunk_to_service);
			tinfo->state = TS_SEIZE_MFR;
		}
		break;

	case TS_SEIZE_MFR: /* Seize MF receiver */
		if((tinfo->mf_receiver_descriptor = MF_decoder.seize(__mf_receiver_callback)) > -1) {
			/* Connect trunk RX and TX to assigned junctor */
			Xps_logical.connect_trunk_orig(&tinfo->jinfo, this->_trunk_to_service);
			/* Make audio connection to seized MF receiver */
			Xps_logical.connect_mf_receiver(&tinfo->jinfo, tinfo->mf_receiver_descriptor);
			/* Tell trunk card to send a wink */
			Card_comm.send_command(Card_Comm::RT_TRUNK, this->_trunk_to_service, REG_SEND_WINK);
			tinfo->state = TS_WAIT_ADDR_INFO;

		}
		break;

	case TS_WAIT_ADDR_INFO:
		break; /* ToDo */


	case TS_RESET:
		/* Release any resources first */
		if(tinfo->tone_plant_descriptor != -1) {
			Tone_plant.channel_release(tinfo->tone_plant_descriptor);
			tinfo->tone_plant_descriptor = -1;
		}
		if(tinfo->mf_receiver_descriptor != -1) {
				MF_decoder.release(tinfo->mf_receiver_descriptor);
				tinfo->mf_receiver_descriptor = -1;
			}
		if(tinfo->dtmf_receiver_descriptor != -1) {
				MF_decoder.release(tinfo->dtmf_receiver_descriptor);
				tinfo->dtmf_receiver_descriptor = -1;
			}

		/* Then release the junctor if we seized it initially */
		if(tinfo->junctor_seized) {
			Xps_logical.release(&tinfo->jinfo);
			tinfo->junctor_seized = false;
		}
		tinfo->state = TS_IDLE;
		break;


	default:
		tinfo->state = TS_RESET;
		break;
	}


	this->_trunk_to_service++;
	if(this->_trunk_to_service >= MAX_TRUNK_CARDS) {
		this->_trunk_to_service = 0;
	}

	osMutexRelease(this->_lock); /* Release the lock */


}

} /* End namespace trunk */

