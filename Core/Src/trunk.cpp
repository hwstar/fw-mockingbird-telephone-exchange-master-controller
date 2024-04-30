#include "top.h"
#include "logging.h"
#include "card_comm.h"
#include "mf_receiver.h"
#include "drv_dtmf.h"
#include "tone_plant.h"
#include "xps_logical.h"
#include "trunk.h"

Trunk::Trunk Trunks;
static const char *TAG = "trunk";

namespace Trunk {


void Trunk::event_handler(uint32_t event_type, uint32_t resource) {
	LOG_DEBUG(TAG, "Received event from physical trunk %u. Type: %u",resource, event_type);

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */


	osMutexRelease(this->_lock); /* Release the lock */


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
		Trunk_Info *tinfo = &this->_trunk_info[index];

		tinfo->state = TS_IDLE;
		tinfo->tone_plant_descriptor = tinfo->mf_receiver_descriptor = tinfo->dtmf_receiver_descriptor = -1;


	}


}

/*
 * Called repeatedly by event task
 */


void Trunk::poll(void) {



	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	Trunk_Info *tinfo = &this->_trunk_info[this->_trunk_to_service];

	switch(tinfo->state) {
	case TS_IDLE:
		/* Wait for Request IR event */

		break;

	case TS_RESET:
		if(tinfo->junctor_seized) {
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
			/* Then reset the junctor connections */
			Xps_logical.disconnect_all(&tinfo->jinfo);
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

