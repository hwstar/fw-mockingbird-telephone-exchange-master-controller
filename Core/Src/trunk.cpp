#include "top.h"
#include "util.h"
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

static void __mf_receiver_callback(void *parameter, uint8_t error_code, uint8_t digit_count, char *data) {
	Trunks._mf_receiver_callback(parameter, error_code, digit_count, data);
}

/*
 * Called from __mf_receiver_callback();
 */

void Trunk::_mf_receiver_callback(void *parameter, uint8_t error_code, uint8_t digit_count, char *data) {
	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	if(!parameter) {
		LOG_PANIC(TAG, "NULL Pointer passed in");
	}
	Connector::Conn_Info *tinfo = (Connector::Conn_Info *) parameter;

	if(error_code == MF_Decoder::MFE_OK) {
		LOG_DEBUG(TAG, "Received MF address: %s on trunk %d", data, tinfo->phys_line_trunk_number);
		if(tinfo->state == TS_WAIT_ADDR_INFO) {
			/* Copy digit count */
			tinfo->num_dialed_digits = digit_count;
			/* Copy address omitting control key values */
			int sindex, dindex;
			for(sindex = 0, dindex = 0; ((dindex < Connector::MAX_DIALED_DIGITS) && (data[sindex])); sindex++) {
				if((data[sindex] < '0') || (data[sindex] > '9')) {
					continue;
				}
				tinfo->digit_buffer[dindex++] = data[sindex];
			}
			tinfo->digit_buffer[dindex] = 0;

			/* Set next state */
			tinfo->state = TS_HAVE_ADDR_INFO;
		}

	}
	else {
		LOG_ERROR(TAG, "Timed out waiting for MF Digits on trunk %d", tinfo->phys_line_trunk_number);
		if(tinfo->state == TS_WAIT_ADDR_INFO) {
			tinfo->state = TS_SEND_CONGESTION;
		}
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

	switch(event_type) {
	case EV_REQUEST_IR:
		if(tinfo->state == TS_IDLE) {
			tinfo->state = TS_SEIZE_JUNCTOR;
		}
		break;
	case EV_CALL_DROPPED:
		switch(tinfo->state) {
		case TS_SEIZE_JUNCTOR:
		case TS_SEIZE_TG:
		case TS_SEIZE_MFR:
		case TS_WAIT_ADDR_INFO:
		case TS_HAVE_ADDR_INFO:
		case TS_SEND_BUSY:
		case TS_SEND_CONGESTION:
		case TS_INCOMING_FAILED:
		case TS_INCOMING_CONNECT_AUDIO:
			tinfo->state = TS_RESET;
			break;

		case TS_INCOMING_ANSWERED:
			tinfo->state = TS_INCOMING_TEARDOWN;
			tinfo->called_party_hangup = false;
			break;

		case TS_INCOMING_WAIT_SUPV:
		case TS_SEND_RINGING:
			tinfo->state = TS_RINGING_TEARDOWN;
			break;

		default:
			break;
		}
		break;


	case EV_BUSY:
		if(tinfo->state == TS_WAIT_WINK_OR_BUSY) {
			tinfo->state = TS_SEND_TRUNK_BUSY;
		}
		break;

	case EV_NO_WINK:
		if(tinfo->state == TS_WAIT_WINK_OR_BUSY) {
			tinfo->state = TS_GOT_NO_WINK;
		}
		break;

	case EV_SEND_ADDR_INFO:
		if(tinfo->state == TS_WAIT_WINK_OR_BUSY) {
			tinfo->state = TS_OUTGOING_REQUEST_ADDR_INFO;
		}


	}
	osMutexRelease(this->_lock); /* Release the lock */
}

/* This handler receives messages from the connection peer */

uint32_t Trunk::peer_message_handler(Connector::Conn_Info *conn_info, uint32_t phys_line_trunk_number, uint32_t message, void *data) {

	if(!conn_info) {
		LOG_PANIC(TAG, "Null pointer passed in");
	}
	if(phys_line_trunk_number >= MAX_TRUNK_CARDS) {
		LOG_PANIC(TAG, "Bad parameter value");
	}

	/* Point to the connection info for the physical line number */
	Connector::Conn_Info *tinfo = &this->_conn_info[phys_line_trunk_number];

	uint32_t res = Connector::PMR_OK;

	switch(message) {
	case Connector::PM_SEIZE:
		/* Seize trunk for outgoing call */
		if(tinfo->state == TS_IDLE) {
			tinfo->peer = conn_info;
			tinfo->state = TS_OUTGOING_START;
		}
		else {
			tinfo->state = TS_SEND_TRUNK_BUSY;
		}
		break;

	case Connector::PM_RELEASE:
		/* Release trunk used for outgoing call */
		break;


	case Connector::PM_ANSWERED:
		/* Called party answered */
		if(tinfo->state == TS_INCOMING_WAIT_SUPV) {
			tinfo->peer = conn_info;
			tinfo->state = TS_INCOMING_CONNECT_AUDIO;
		}
		break;

	case Connector::PM_CALLED_PARTY_HUNGUP:
		/* Called party hung up */
		if(tinfo->state == TS_INCOMING_ANSWERED) {
			/* LOG_DEBUG(TAG, "Called party hung up"); */
			tinfo->called_party_hangup = true;
			tinfo->state = TS_INCOMING_TEARDOWN;
		}
		break;

	default:
		LOG_ERROR(TAG, "Unrecognized message: %u", message);
		break;

	}

	return res;
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
		/* Route table number */
		/* Todo: make this configurable */
		tinfo->route_table_number = 1;

		tinfo->state = TS_IDLE;
		tinfo->phys_line_trunk_number = index;
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
		/* Wait for Request IR  or seize event */
		break;

	case TS_SEIZE_JUNCTOR:
		if(Xps_logical.seize(&tinfo->jinfo)) {
			tinfo->junctor_seized = true;
			Conn.prepare(tinfo, Connector::ET_TRUNK, this->_trunk_to_service);
			tinfo->state = TS_SEIZE_TG;
		}
		break;

	case TS_SEIZE_TG:
		if((tinfo->tone_plant_descriptor = Tone_plant.channel_seize()) >= 0) {
			Xps_logical.connect_tone_plant_output(&tinfo->jinfo, tinfo->tone_plant_descriptor);
			tinfo->state = TS_SEIZE_MFR;
		}
		break;


	case TS_SEIZE_MFR: /* Seize MF receiver */
		if((tinfo->mf_receiver_descriptor = MF_decoder.seize(__mf_receiver_callback, tinfo )) > -1) {
			/* Connect trunk RX and TX to assigned junctor */
			Xps_logical.connect_trunk_orig(&tinfo->jinfo, this->_trunk_to_service);
			/* Make audio connection to seized MF receiver */
			Xps_logical.connect_mf_receiver(&tinfo->jinfo, tinfo->mf_receiver_descriptor);
			/* Tell trunk card to send a wink */
			Card_comm.send_command(Card_Comm::RT_TRUNK, this->_trunk_to_service, REG_SEND_WINK);
			Conn.prepare(tinfo, Connector::ET_TRUNK, tinfo->phys_line_trunk_number);
			tinfo->state = TS_WAIT_ADDR_INFO;

		}
		break;

	case TS_WAIT_ADDR_INFO:
		break;

	case TS_HAVE_ADDR_INFO: {
		/* Disconnect MF Receiver */
		Conn.release_mf_receiver(tinfo);
		/* Test route */
		uint32_t res = Conn.test(tinfo, tinfo->digit_buffer);
		switch(res) {
		case Connector::ROUTE_INDETERMINATE:
		case Connector::ROUTE_INVALID:
			tinfo->state = TS_SEND_CONGESTION;
			break;

		case Connector::ROUTE_VALID:
			/* Resolve Route */
			res = Conn.resolve(tinfo);
			switch(res) {
			case Connector::ROUTE_INVALID:
			case Connector::ROUTE_DEST_CONGESTED:
				tinfo->state = TS_SEND_CONGESTION;
				break;

			case Connector::ROUTE_DEST_BUSY:
				tinfo->state = TS_SEND_BUSY;
				break;

			case Connector::ROUTE_DEST_CONNECTED:
				tinfo->state = TS_SEND_RINGING;
				break;

			default:
				LOG_PANIC(TAG, "Invalid result");
				break;
			}
			break;

		default:
			LOG_PANIC(TAG, "Invalid result");
			break;
		}
		break;
	}


	case TS_SEND_RINGING:
		Conn.send_ringing(tinfo);
		tinfo->state = TS_INCOMING_WAIT_SUPV;
		break;

	case TS_RINGING_TEARDOWN: {
		uint32_t dest_equip_type = Conn.get_called_equip_type(tinfo);
		uint32_t dest_line_trunk_number = Conn.get_called_phys_line_trunk(tinfo);
		Conn.send_peer_message(tinfo, dest_equip_type, dest_line_trunk_number, Connector::PM_RELEASE);
		tinfo->state = TS_RESET;
	}
		break;


	case TS_SEND_BUSY:
		Conn.send_busy(tinfo);
		tinfo->state = TS_INCOMING_FAILED;
		break;

	case TS_SEND_CONGESTION:
		Conn.send_congestion(tinfo);
		tinfo->state = TS_INCOMING_FAILED;
		break;

	case TS_INCOMING_FAILED:
		/* Busy or Congested */
		break;

	case TS_INCOMING_WAIT_SUPV:
		/* Waiting for called party to answer */
		break;

	case TS_INCOMING_CONNECT_AUDIO:
		/* Disconnect and release the tone generator */
		Conn.release_tone_generator(tinfo);
		/* Connect the called party audio to the junctor */
		Conn.connect_called_party_audio(tinfo);
		/* Tell originator the called party answered */
		Card_comm.send_command(Card_Comm::RT_TRUNK, this->_trunk_to_service, REG_INCOMING_CONNECTED);
		tinfo->state = TS_INCOMING_ANSWERED;
		break;

	case TS_INCOMING_ANSWERED:
		break; /* Wait here for events and messages */

	case TS_INCOMING_TEARDOWN:
		if(tinfo->called_party_hangup) {
			/* LOG_DEBUG(TAG, "Sending drop call to trunk card"); */
			Card_comm.send_command(Card_Comm::RT_TRUNK, this->_trunk_to_service, REG_DROP_CALL);
		}
		else {
			/* LOG_DEBUG(TAG, "Releasing called party"); */
			Conn.release_called_party(tinfo);
		}
		tinfo->state = TS_RESET;
		break;


	case TS_OUTGOING_START:
		/* Disconnect the caller's audio */
		Conn.disconnect_caller_party_audio(tinfo->peer);
		/* Seize the trunk */
		Card_comm.send_command(Card_Comm::RT_TRUNK, this->_trunk_to_service, REG_SEIZE_TRUNK);
		tinfo->state = TS_WAIT_WINK_OR_BUSY;
		break;

	case TS_WAIT_WINK_OR_BUSY:
		/* 3 possible outcomes:
		 * 1. receive wink event
		 * 2. receive busy event.
		 * 3. receive wink time out event.
		 */
		break;

	case TS_REQUEST_ADDR_INFO:
		/* Got the wink event */
		/* Send message to caller to request address information */
		break;

	case TS_OUTGOING_WAIT_ADDR_INFO:
		/* Wait for caller state machine to send address information */
		break;

	case TS_GOT_NO_WINK:
		/* Timed out waiting for wink */

	case TS_RELEASE_TRUNK:
		/* Release the trunk by sending REG_DROP_CALL to the trunk card, then clean up */
		Card_comm.send_command(Card_Comm::RT_TRUNK, this->_trunk_to_service, REG_DROP_CALL);
		tinfo->state = TS_RESET;


	case TS_SEND_TRUNK_BUSY: {
		uint32_t source_equip_type = Conn.get_caller_equip_type(tinfo);
		uint32_t source_line_trunk_number = Conn.get_caller_phys_line_trunk(tinfo);
		Conn.send_peer_message(tinfo, source_equip_type, source_line_trunk_number, Connector::PM_TRUNK_BUSY);
		tinfo->state = TS_RESET;
		}

	case TS_RESET:
		/* Release any resources first */
		Conn.release_tone_generator(tinfo);
		Conn.release_mf_receiver(tinfo);
		Conn.release_dtmf_receiver(tinfo);

		/* Then release the junctor if we seized it initially */
		if(tinfo->junctor_seized) {
			Xps_logical.release(&tinfo->jinfo);
			tinfo->junctor_seized = false;
		}
		tinfo->called_party_hangup = false;
		tinfo->peer = NULL;
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

