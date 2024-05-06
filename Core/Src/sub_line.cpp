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

	Connector::Conn_Info *linfo = &this->_conn_info[parameter];

	LOG_DEBUG(TAG, "Received DTMF digit '%c' from line '%u', receiver %d", digit, parameter, (int) descriptor);
	if(linfo->num_dialed_digits < Connector::MAX_DIALED_DIGITS) {
		linfo->digit_buffer[linfo->num_dialed_digits++] = digit;
		linfo->digit_buffer[linfo->num_dialed_digits] = 0;
	}
}

/*
 * Release DTMF receiver and disconnect it from the junctor if seized
 */

void Sub_Line::_release_dtmf_receiver(Connector::Conn_Info *linfo) {
	if(!linfo) {
		LOG_PANIC(TAG, "Null pointer passed in");
	}
	if(linfo->dtmf_receiver_descriptor != -1) {
		Xps_logical.disconnect_dtmf_receiver(&linfo->jinfo);
		Dtmf_receivers.release(linfo->dtmf_receiver_descriptor);
		linfo->dtmf_receiver_descriptor = -1;

	}

}

/*
 * Release tone generator and disconnect it from the junctor if seized
 */

void Sub_Line::_release_tone_generator(Connector::Conn_Info *linfo) {
	if(!linfo) {
		LOG_PANIC(TAG, "Null pointer passed in");
	}
	if(linfo->tone_plant_descriptor != -1) {
		Xps_logical.disconnect_tone_plant_output(&linfo->jinfo);
		Tone_plant.channel_release(linfo->tone_plant_descriptor);
		linfo->tone_plant_descriptor = -1;


	}
}



/*
 * Event handler, receives events from a line on a dual line card
 */

void Sub_Line::event_handler(uint32_t event_type, uint32_t resource) {
	LOG_DEBUG(TAG, "Received event from physical line %u Type: %u ", resource, event_type);

	osMutexAcquire(this->_lock, osWaitForever); /*Get the lock */

	Connector::Conn_Info *linfo = &this->_conn_info[resource];

	/* Subscriber lifted receiver */
	if((linfo->state == LS_IDLE) && (event_type == EV_REQUEST_OR)) {
		linfo->state = LS_SEIZE_JUNCTOR;
	}
	else if (event_type == EV_HUNGUP) {
		/* Subscriber hung up */
		switch(linfo->state) {
		case LS_IDLE:
			break; /* Ignore in idle state */

		case LS_SEIZE_JUNCTOR: /* Calling party perspective */
		case LS_SEIZE_TG:
		case LS_SEIZE_DTMFR:
		case LS_WAIT_FIRST_DIGIT:
		case LS_WAIT_ROUTE:
		case LS_SEND_BUSY:
		case LS_SEND_CONGESTION:
		case LS_WAIT_HANGUP:
			linfo->state = LS_RESET;
			break;

		case LS_WAIT_ANSWER: /* Calling party perspective */
		case LS_CALLED_PARTY_ANSWERED:
		case LS_WAIT_END_CALL:
			linfo->state = LS_END_CALL;
			break;

		case LS_ANSWERED: /* Called party perspective */
			linfo->state = LS_CALLED_PARTY_HUNGUP;
			break;

		default:
			LOG_ERROR(TAG, "Unhandled event type %u from line %u, line state %u ",event_type, resource, linfo->state);
			break;
		}

	}
	else if (event_type == EV_ANSWERED) { /* Called subscriber perspective */
		if((linfo->state == LS_RING) || (linfo->state = LS_RINGING)) {
			linfo->state = LS_ANSWER;
		}
	}

	osMutexRelease(this->_lock); /* Release the lock */


}

/* This handler receives messages to change the line state or ask for status */

uint32_t Sub_Line::peer_message_handler(Connector::Conn_Info *conn_info, uint32_t phys_line_trunk_number, uint32_t message) {

	if(!conn_info) {
		LOG_PANIC(TAG, "Null pointer passed in");
	}
	if(phys_line_trunk_number >= MAX_DUAL_LINE_CARDS * 2) {
		LOG_PANIC(TAG, "Bad parameter value");
	}

	uint32_t res = Connector::PMR_OK;

	/* Point to the connection info for the physical line number */
	Connector::Conn_Info *linfo = &this->_conn_info[phys_line_trunk_number];


	switch(message) {
	case Connector::PM_SEIZE:
		if(linfo->state == LS_IDLE) {
			linfo->peer = conn_info;
			linfo->state = LS_RING;

		}
		else {
			/* Line in use */
			res = Connector::PMR_BUSY;

		}
		break;

	case Connector::PM_RELEASE:
		if((linfo->state == LS_ANSWERED) || (linfo->state == LS_RING)  || (linfo->state == LS_RINGING)) {
			linfo->state = LS_CALL_ENDED;
		}
		else {
			LOG_ERROR(TAG, "Bad message");
		}

		break;


	case Connector::PM_ANSWERED:
		if(linfo->state == LS_WAIT_ANSWER) {
			linfo->state = LS_CALLED_PARTY_ANSWERED;
		}
		else {
			LOG_ERROR(TAG, "Bad Message");
		}

		break;

	case Connector::PM_CALLED_PARTY_HUNGUP:
		if(linfo->state == LS_WAIT_END_CALL) {
			linfo->state = LS_RESET;
		}
		else {
			LOG_ERROR(TAG, "Bad Message");
		}
		break;

	default:
		LOG_ERROR(TAG, "Bad parameter value, message: %d, line_state: %d, line number: %d", message, linfo->state, phys_line_trunk_number);
		break;
	}
	return res;
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

	/*
	 * Caller perspective
	 */

	case LS_SEIZE_JUNCTOR: /* Caller perspective */
		if(Xps_logical.seize(&linfo->jinfo)) {
				Conn.prepare(linfo, Connector::ET_LINE, this->_line_to_service);
				linfo->junctor_seized = true;
				linfo->state = LS_SEIZE_TG;
			}
		break;


	case LS_SEIZE_TG: /* Caller perspective */
		if((linfo->tone_plant_descriptor = Tone_plant.channel_seize()) > -1) {
			linfo->state = LS_SEIZE_DTMFR;
		}
		break;


	case LS_SEIZE_DTMFR: /* Caller perspective */
		if((linfo->dtmf_receiver_descriptor = Dtmf_receivers.seize(__digit_receiver_callback, this->_line_to_service)) > -1) {
			Tone_plant.send_call_progress_tones(linfo->dtmf_receiver_descriptor, Tone_Plant::CPT_DIAL_TONE);
			/* Connect phone to junctor */
			Xps_logical.connect_phone_orig(&linfo->jinfo, this->_line_to_service);
			/* Connect DTMF receiver to junctor */
			Xps_logical.connect_dtmf_receiver(&linfo->jinfo, linfo->dtmf_receiver_descriptor);
			/* Clear digit buffer */
			linfo->num_dialed_digits = 0;
			linfo->digit_buffer[0] = 0;
			/* Connect tone generator to junctor */
			Xps_logical.connect_tone_plant_output(&linfo->jinfo, linfo->tone_plant_descriptor);
			/* Tell the line on the line card that the OR is connected */
			/* For future dial pulse support */
			Card_comm.send_command(Card_Comm::RT_LINE, this->_line_to_service, REG_SET_OR_ATTACHED);
			linfo->state = LS_WAIT_FIRST_DIGIT;
		}

		break;

	case LS_WAIT_FIRST_DIGIT: /* Caller perspective */
		if(linfo->num_dialed_digits) {
			/* Break dial tone */
			Tone_plant.stop(linfo->tone_plant_descriptor);
			linfo->state = LS_WAIT_ROUTE;

		}


	case LS_WAIT_ROUTE: /* Caller perspective */

		/* To save on CPU cycles, only call Conn.test() when the number of digits changes */

		if((linfo->num_dialed_digits == linfo->prev_num_dialed_digits) || (linfo->num_dialed_digits == 0)) {
			break;
		}
		linfo->prev_num_dialed_digits = linfo->num_dialed_digits;

		switch(Conn.test(linfo, linfo->digit_buffer)) {

		case Connector::ROUTE_INDETERMINATE:
			/* Don't have all the necessary dialed digits yet */
			break;

		case Connector::ROUTE_INVALID:
		default:
			/* Release DTMF Receiver */
			this->_release_dtmf_receiver(linfo);
			/* Send congestion */
			/* Could be a call cannot be completed as dialed in the future once we have the SPI SRAM */
			linfo->state = LS_SEND_CONGESTION;
			break;

		case Connector::ROUTE_VALID:
			/* Release the DTMF Receiver */
			this->_release_dtmf_receiver(linfo);
			/* Todo connect the call */
			switch(Conn.connect(linfo)) {

			case Connector::ROUTE_INVALID:
				linfo->state = LS_SEND_CONGESTION;
				break;

			case Connector::ROUTE_DEST_CONNECTED:
				{
					uint32_t equip_type = Conn.get_called_equip_type(linfo);
					if(equip_type == Connector::ET_LINE) {
						/* Send ringing */
						Tone_plant.send_buffer_loop_ulaw(linfo->tone_plant_descriptor, "city_ring", 0.0);
					} else if(equip_type == Connector::ET_TRUNK) {
						/* A trunk destination means we don't need the tone generator any more.
						 * Release it so it can be be available for MF tone generation */
						this->_release_tone_generator(linfo);
					}
				}

				linfo->state = LS_WAIT_ANSWER;
				break;

			case Connector::ROUTE_DEST_BUSY:
				linfo->state = LS_SEND_BUSY;
				break;

			case Connector::ROUTE_DEST_CONGESTED:
				linfo->state = LS_SEND_CONGESTION;
				break;

			default:
				LOG_PANIC(TAG, "Unhandled case");
				break;
			}
		}
		break;

	case LS_SEND_BUSY: /* Caller perspective */
			Tone_plant.send_call_progress_tones(linfo->tone_plant_descriptor, Tone_Plant::CPT_BUSY);
			linfo->state = LS_WAIT_HANGUP;
			break;


	case LS_SEND_CONGESTION: /* Caller perspective */
		Tone_plant.send_call_progress_tones(linfo->tone_plant_descriptor, Tone_Plant::CPT_CONGESTION);
		linfo->state = LS_WAIT_HANGUP;
		break;

	case LS_WAIT_ANSWER: /* Caller perspective */
		/* Wait here for peer message */
		break;

	case LS_CALLED_PARTY_ANSWERED: /* Caller perspective */

		/* Stop tone generator and release it */
		Tone_plant.stop(linfo->tone_plant_descriptor);
		this->_release_tone_generator(linfo);

		/* Send in call state to line card */
		Card_comm.send_command(Card_Comm::RT_LINE, this->_line_to_service, REG_SET_IN_CALL);
		/* Connect audio path */
		Conn.connect_called_party_audio(linfo);

		linfo->state = LS_WAIT_END_CALL;

		break;

	case LS_WAIT_END_CALL: /* Caller perspective */
		/* Wait here while in call*/
		break;


	case LS_END_CALL: /* Caller perspective */
	{
		/* Send message to called party to end the call on their end */
		uint32_t phys_line_trunk = Conn.get_called_phys_line_trunk(linfo);
		uint32_t equip_type = Conn.get_called_equip_type(linfo);
		Conn.send_peer_message(linfo, equip_type, phys_line_trunk, Connector::PM_RELEASE);
		linfo->state = LS_RESET;
	}
		break;


	case LS_WAIT_HANGUP: /* Caller perspective */
		/* There was an error, wait until caller hangs up */
		break;

	/*
	 * Called party perspective
	 */

	case LS_RING: /* Called perspective */
		/* Tell the SLIC to start ringing */
		Card_comm.send_command(Card_Comm::RT_LINE, this->_line_to_service, REG_REQUEST_RINGING);
		/* Todo: Add ringing tone */

		linfo->state = LS_RINGING;
		break;

	case LS_RINGING: /* Called perspective */
		break;

	case LS_ANSWER: /* Called perspective */
		{
			/* Send message back to caller that the called party answered */
			uint32_t caller_equip_type = Conn.get_caller_equip_type(linfo->peer);
			uint32_t caller_phys_number = Conn.get_caller_phys_line_trunk(linfo->peer);
			Conn.send_peer_message(linfo, caller_equip_type, caller_phys_number , Connector::PM_ANSWERED);
			linfo->state = LS_ANSWERED;
		}
		break;

	case LS_ANSWERED: /* Called perspective */
		break;

	case LS_CALLED_PARTY_HUNGUP: /* Called perspective */
		{
			uint32_t caller_equip_type = Conn.get_caller_equip_type(linfo->peer);
			uint32_t caller_phys_number = Conn.get_caller_phys_line_trunk(linfo->peer);
			Conn.send_peer_message(linfo, caller_equip_type, caller_phys_number, Connector::PM_CALLED_PARTY_HUNGUP);
			linfo->state = LS_RESET;
		}
		break;

	case LS_CALL_ENDED: /* Called perspective */
		/* Send End Call to Caller */
		Card_comm.send_command(Card_Comm::RT_LINE, this->_line_to_service, REG_END_CALL);
		linfo->state = LS_RESET;
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
		linfo->peer = NULL;
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
