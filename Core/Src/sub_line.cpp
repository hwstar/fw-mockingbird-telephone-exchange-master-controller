#include "top.h"
#include "logging.h"
#include "err_handler.h"
#include "util.h"
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
 * Tone complete callback
 */

static void __tone_complete_callback(uint32_t channel_number, void *data) {
	Sub_line._tone_complete_callback(channel_number, data);

}

void Sub_Line::_tone_complete_callback(uint32_t channel_number, void *data) {
	if(!data) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	Connector::Conn_Info *linfo = (Connector::Conn_Info *) data;

	switch(linfo->state) {
	case LS_WAIT_FOR_DR_SAMPLE:
		linfo->state = LS_CALL_SETUP;
		break;

	default:
		break;
	}
}



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




static void __dial_timer_callback(void *arg) {
	Sub_line._dial_timer_callback(arg);

}

/*
 * Called when dial timer expires
 */

void Sub_Line::_dial_timer_callback(void *arg) {
	Connector::Conn_Info *linfo = (Connector::Conn_Info *) arg;

	osMutexAcquire(this->_lock, osWaitForever); /*Get the lock */

	switch(linfo->state) {

	case LS_WAIT_FIRST_DIGIT: /* Calling party perspective */
	case LS_WAIT_ROUTE:
		linfo->state = LS_DIAL_TIMEOUT;
		break;

	case LS_WAIT_HANGUP: /* Calling party perspective */
		linfo->state = LS_CONGESTION_DISCONNECT;
		break;


	case LS_ORIG_DISCONNECT_D: /* Called party perspective */
		linfo->state = LS_ORIG_DISCONNECT_F;
		break;

	case LS_WAIT_BEFORE_DR_SAMPLE:
		linfo->state = LS_SETUP_DR_SAMPLE;
		break;


	}
	osMutexRelease(this->_lock); /* Release the lock */
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
		case LS_TEST_FOR_DR_SAMPLE:
		case LS_WAIT_BEFORE_DR_SAMPLE:
		case LS_SETUP_DR_SAMPLE:
		case LS_WAIT_FOR_DR_SAMPLE:
		case LS_CALL_SETUP:
		case LS_DIAL_TIMEOUT:
		case LS_SEND_BUSY:
		case LS_SEND_CONGESTION:
		case LS_CONGESTION_DISCONNECT:
		case LS_WAIT_HANGUP:
		case LS_FAR_END_DISCONNECT:
		case LS_FAR_END_DISCONNECT_B:
			linfo->state = LS_RESET;
			break;

		case LS_SEIZE_TRUNK:
		case LS_WAIT_TRUNK_RESPONSE:
		case LS_TRUNK_ADVANCE:
		case LS_TRUNK_SEND_ADDR_INFO:
		case LS_TRUNK_WAIT_ADDR_SENT:
		case LS_TRUNK_CONNECT_CALLER:
		case LS_TRUNK_WAIT_SUPV:
			linfo->state = LS_TRUNK_OUTGOING_RELEASE;
			break;


		/* Calling party hung up*/
		case LS_ORIG_DISCONNECT:
		case LS_ORIG_DISCONNECT_B:
		case LS_ORIG_DISCONNECT_C:
		case LS_ORIG_DISCONNECT_D:
		case LS_ORIG_DISCONNECT_F:
			linfo->state = LS_ORIG_DISCONNECT_E;
			break;


		case LS_WAIT_ANSWER: /* Calling party perspective */
		case LS_CALLED_PARTY_ANSWERED: /* Calling party perspective */
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
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	if(phys_line_trunk_number >= MAX_DUAL_LINE_CARDS * 2) {
		POST_ERROR(Err_Handler::EH_INVP);
	}

	uint32_t res = Connector::PMR_OK;

	/* Point to the connection info for the physical line number */
	Connector::Conn_Info *linfo = &this->_conn_info[phys_line_trunk_number];

	LOG_DEBUG(TAG,"Line %u got peer message %u", phys_line_trunk_number, message);

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
		if(linfo->state == LS_ANSWERED) {
			/* Send congestion to called party */
			linfo->state = LS_ORIG_DISCONNECT;

		}
		else if ((linfo->state == LS_RING)  || (linfo->state == LS_RINGING)) {
			/* Caller hung up while called line was ringing */
			linfo->state = LS_ORIG_DISCONNECT_E;
		}
		else {
			LOG_ERROR(TAG, "Bad message");
		}

		break;


	case Connector::PM_ANSWERED:
		if((linfo->state == LS_WAIT_ANSWER) || (linfo->state == LS_TRUNK_WAIT_SUPV )) {
			LOG_DEBUG(TAG, "New state: LS_CALLED_PARTY_ANSWERED");
			linfo->state = LS_CALLED_PARTY_ANSWERED;
		}
		else {
			LOG_ERROR(TAG, "Bad Message");
		}
		break;

	case Connector::PM_CALLED_PARTY_HUNGUP:
		if(linfo->state == LS_WAIT_END_CALL) {
			linfo->state = LS_FAR_END_DISCONNECT;
		}
		else {
			LOG_ERROR(TAG, "Bad Message");
		}
		break;

	case Connector::PM_TRUNK_BUSY:
		if((linfo->state == LS_WAIT_TRUNK_RESPONSE)) {
			LOG_INFO(TAG, "Trunk busy returned by PM");
			linfo->state = LS_TRUNK_ADVANCE;
		}
		break;

	case Connector::PM_TRUNK_NO_WINK:
		if((linfo->state == LS_WAIT_TRUNK_RESPONSE)) {
				uint32_t dest_trunk_number = Conn.get_called_phys_line_trunk(linfo);
				LOG_WARN(TAG, "No wink seen on trunk: %u", dest_trunk_number);
				linfo->state = LS_TRUNK_ADVANCE;
			}
		break;

	case Connector::PM_TRUNK_READY_FOR_ADDR_INFO:
		if((linfo->state == LS_WAIT_TRUNK_RESPONSE) || (linfo->state = LS_TRUNK_ADVANCE)) {
			linfo->state = LS_TRUNK_SEND_ADDR_INFO;
		}
		break;

	case Connector::PM_TRUNK_READY_TO_CONNECT_CALLER:
		if(linfo->state == LS_TRUNK_WAIT_ADDR_SENT) {
			linfo->state = LS_TRUNK_CONNECT_CALLER;
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
		POST_ERROR(Err_Handler::EH_LCE);
		  }
	/* Create event flags */
	this->_event_flags = osEventFlagsNew(&event_flags_attributes);

	/* Initialization of line-specific information */
	for(uint8_t index = 0; index < MAX_DUAL_LINE_CARDS * 2; index++) {
		Connector::Conn_Info *linfo = &this->_conn_info[index];


		/* Digit dialing timer */
		linfo->dial_timer = osTimerNew(__dial_timer_callback, osTimerOnce, linfo, NULL);
		if(linfo->dial_timer == NULL) {
			POST_ERROR(Err_Handler::EH_TCE);
		}

		/* Variables */
		linfo->equip_type = Connector::ET_LINE;
		linfo->phys_line_trunk_number = index;
		linfo->state = LS_IDLE;
		linfo->tone_plant_descriptor = linfo->mf_receiver_descriptor = linfo->dtmf_receiver_descriptor = -1;
	}
}


/*
 * Called when the system is about to go on line
 *
 * Set up config here.
 */

void Sub_Line::config(void) {

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
			/* Connect phone to junctor */
			Xps_logical.connect_phone_orig(&linfo->jinfo, this->_line_to_service);
			/* Connect DTMF receiver to junctor */
			Xps_logical.connect_dtmf_receiver(&linfo->jinfo, linfo->dtmf_receiver_descriptor);
			/* Clear digit buffer */
			linfo->num_dialed_digits = 0;
			linfo->digit_buffer[0] = 0;
			/* Connect tone generator to junctor */
			Xps_logical.connect_tone_plant_output(&linfo->jinfo, linfo->tone_plant_descriptor);
			/* Send Dial tone */
			Conn.send_dial_tone(linfo->tone_plant_descriptor);
			/* Tell the line on the line card that the OR is connected */
			/* For future dial pulse support */
			Card_comm.send_command(Card_Comm::RT_LINE, this->_line_to_service, REG_SET_OR_ATTACHED);
			/* Start the dial timer */
			osTimerStart(linfo->dial_timer, DTMF_DIGIT_DIAL_TIME);
			linfo->state = LS_WAIT_FIRST_DIGIT;
		}

		break;

	case LS_WAIT_FIRST_DIGIT: /* Caller perspective */
		if(linfo->num_dialed_digits) {
			/* Restart Dial Timer */
			osTimerStart(linfo->dial_timer, DTMF_DIGIT_DIAL_TIME);
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

		/* Restart dial timer */
		osTimerStart(linfo->dial_timer, DTMF_DIGIT_DIAL_TIME);

		/* Test for a valid route */
		switch(Conn.test(linfo, linfo->digit_buffer)) {

		case Connector::ROUTE_INDETERMINATE:
			/* Don't have all the necessary dialed digits yet */
			break;

		case Connector::ROUTE_INVALID:
		default:
			/* Release DTMF Receiver */
			Conn.release_dtmf_receiver(linfo);
			/* Send congestion */
			/* Could be a call cannot be completed as dialed in the future once we have the SPI SRAM */
			linfo->state = LS_SEND_CONGESTION;
			break;

		case Connector::ROUTE_VALID:
			/* Stop the dial timer */
			osTimerStop(linfo->dial_timer);
			/* Release the DTMF Receiver */
			Conn.release_dtmf_receiver(linfo);
			linfo->state = LS_TEST_FOR_DR_SAMPLE;
			break;
		}
		break;

	case LS_TEST_FOR_DR_SAMPLE: /* Caller perspective */
		/* Test for digits recognized sample */
		if(!Conn.get_digits_recognized_buffer_name()) {
			linfo->state = LS_CALL_SETUP;
		}
		else {
			linfo->state = LS_WAIT_BEFORE_DR_SAMPLE;
			osTimerStart(linfo->dial_timer, DIGITS_RECOGNIZED_DELAY);
		}

	case LS_WAIT_BEFORE_DR_SAMPLE: /* Caller perspective */
		/* Wait for timer to expire */
		break;

	case LS_SETUP_DR_SAMPLE: /* Caller perspective */
		/* Set up the digits recognized audio sample */
		static const char *buffer_name = Conn.get_digits_recognized_buffer_name();
		linfo->state = LS_WAIT_FOR_DR_SAMPLE;
		if(!Tone_plant.send_buffer_ulaw(linfo->tone_plant_descriptor, buffer_name, __tone_complete_callback, linfo, 0.0)) {
			POST_ERROR(Err_Handler::EH_INVR);
		}

		break;

	case LS_WAIT_FOR_DR_SAMPLE:
		/* Wait for Digits recognized sample to finish */
		break;


	case LS_CALL_SETUP: /* Caller perspective */
		/* Resolve the call */
		switch(Conn.resolve(linfo)) {

		case Connector::ROUTE_INVALID:
			linfo->state = LS_SEND_CONGESTION;
			break;

		case Connector::ROUTE_DEST_CONNECTED:
			{
				uint32_t equip_type = Conn.get_called_equip_type(linfo);
				if(equip_type == Connector::ET_LINE) {
					/* Send ringing */
					Conn.send_ringing(linfo);
					linfo->state = LS_WAIT_ANSWER;
				} else if(equip_type == Connector::ET_TRUNK) {
					linfo->state = LS_WAIT_TRUNK_RESPONSE;
				}
			}


			break;

		case Connector::ROUTE_DEST_BUSY:
			linfo->state = LS_SEND_BUSY;
			break;

		case Connector::ROUTE_DEST_TRUNK_BUSY:
			LOG_DEBUG(TAG, "Trunk busy returned by Conn.resolve()");
			linfo->state = LS_TRUNK_ADVANCE;
			break;

		default:
			POST_ERROR(Err_Handler::EH_UHC);
			break;
		}


	case LS_WAIT_TRUNK_RESPONSE: /* Caller perspective */
		/* Wait for the trunk to respond with a peer message */
		break;


	case LS_TRUNK_ADVANCE: {
		/* See if we can use a different trunk */
		uint32_t res = Conn.resolve_try_next_trunk(linfo);
		switch(res) {
		case Connector::ROUTE_DEST_TRUNK_BUSY:
			LOG_DEBUG(TAG, "Trunk busy after trunk advance");
			break; /* This trunk is busy, try again later */

		case Connector::ROUTE_NO_MORE_TRUNKS:
			/* No more outgoing trunks to try */
			LOG_DEBUG(TAG, "No more trunks after trunk advance");
			linfo->state = LS_SEND_CONGESTION;
			break;

		case Connector::ROUTE_DEST_CONNECTED:
			LOG_DEBUG(TAG, "Trunk advance successful");
			/* Connected, send address info */
			linfo->state = LS_WAIT_TRUNK_RESPONSE;
			break;

		default:
			/* Unhandled result */
			POST_ERROR(Err_Handler::EH_UHC);
			break;
		}
	}
		break;

	case LS_TRUNK_SEND_ADDR_INFO: { /* Caller perspective */
		/* Release the tone generator as we don't need it now */
		Conn.release_tone_generator(linfo);
		/* Temporarily disconnect the caller from the junctor */
		Conn.disconnect_caller_party_audio(linfo);
		/* Everything should be off of the junctor now */
		/* Prepare the address info */
		uint8_t start = linfo->route_info.dest_dial_start_index + 1;
		uint8_t end = strlen(linfo->route_info.dialed_number);

		/* Make the trunk dial string */
		Utility.make_trunk_dial_string(linfo->trunk_outgoing_address, linfo->route_info.dialed_number, start, end,
				Connector::MAX_TRUNK_OUTGOING_ADDRESS);
		LOG_DEBUG(TAG, "Trunk dialed digits: %s", linfo->trunk_outgoing_address);

		/* Tell the trunk the address has been formatted and is ready to send */
		Conn.send_message_to_dest(linfo, Connector::PM_TRUNK_ADDR_INFO_READY);

		/* Wait for address info to be sent */
		linfo->state = LS_TRUNK_WAIT_ADDR_SENT;
	}
		break;

	case LS_TRUNK_WAIT_ADDR_SENT: /* Caller perspective */
		/* Wait for MF Digits to be sent */
		break;

	case LS_TRUNK_CONNECT_CALLER: /* Caller perspective */
		/* Connect caller audio path */
		Conn.connect_caller_party_audio(linfo);
		linfo->state = LS_TRUNK_WAIT_SUPV;
		break;

	case LS_TRUNK_WAIT_SUPV: /* Caller perspective */
		/* Wait here for trunk supervision */
		break;


	case LS_TRUNK_OUTGOING_RELEASE: { /* Caller perspective */
		/* Send peer message to release trunk due to a subscriber hanging up */
		Conn.send_message_to_dest(linfo, Connector::PM_RELEASE);
		linfo->state = LS_RESET;

	}
		break;


	case LS_DIAL_TIMEOUT:
		/* Release DTMF Receiver */
		Conn.release_dtmf_receiver(linfo);
		/* Stop dial tone generation */
		Tone_plant.stop(linfo->tone_plant_descriptor);
		/* Send congestion */
		linfo->state = LS_SEND_CONGESTION;
		break;


	case LS_SEND_BUSY: /* Caller perspective */
		/* Tell tone plant to send call busy tone */
		Conn.send_busy(linfo);
		linfo->state = LS_WAIT_HANGUP;
		break;


	case LS_SEND_CONGESTION: /* Caller perspective */
		if(Conn.get_called_equip_type(linfo) == Connector::ET_TRUNK) {
			/* The trunk state machine releases any tone generator on an exception condition,
			 * and disconnects it from the junctor.
			 * We must seize it again and connect it to the junctor.
			 */
			if(!Conn.seize_and_connect_tone_generator(linfo)) {
				/* No generator available, wait */
				break;
			}
		}

		/* Tell tone plant to send congestion tone */
		osTimerStart(linfo->dial_timer, CONGESTION_SEND_TIME);
		Conn.send_congestion(linfo);
		linfo->state = LS_WAIT_HANGUP;
		break;

	case LS_CONGESTION_DISCONNECT:
		/* Release the tone generator */
		Conn.release_tone_generator(linfo);
		/* Release the junctor */
		if(linfo->junctor_seized) {
			Xps_logical.release(&linfo->jinfo);
			linfo->junctor_seized = false;
		}
		linfo->state = LS_WAIT_HANGUP;



	case LS_WAIT_ANSWER: /* Caller perspective */
		/* Wait here for peer message */
		break;

	case LS_CALLED_PARTY_ANSWERED: { /* Caller perspective */

		uint8_t et = Conn.get_called_equip_type(linfo);
		/* If called party is a line on this exchange */
		if(et == Connector::ET_LINE) {
			Conn.release_tone_generator(linfo);
			/* Send in call state to line card */
			Card_comm.send_command(Card_Comm::RT_LINE, this->_line_to_service, REG_SET_IN_CALL);
			/* Connect audio path */
			Conn.connect_called_party_audio(linfo);
		}

		linfo->state = LS_WAIT_END_CALL;
	}

		break;

	case LS_WAIT_END_CALL: /* Caller perspective */
		/* Wait here while in call*/
		break;

	case LS_FAR_END_DISCONNECT: /* Caller perspective */
		/* Re-acquire a tone generator */
		if((linfo->tone_plant_descriptor = Tone_plant.channel_seize()) > -1) {
				linfo->state = LS_FAR_END_DISCONNECT_B;
			}
		break;

	case LS_FAR_END_DISCONNECT_B: /* Caller perspective */
		/* Disconnect the called party from the junctor */
		Conn.disconnect_called_party_audio(linfo);
		/* Reconnect tone plant to junctor */
		Xps_logical.connect_tone_plant_output(&linfo->jinfo, linfo->tone_plant_descriptor);
		linfo->state = LS_SEND_CONGESTION;
		break;


	case LS_END_CALL: /* Caller perspective */
	{
		/* Send message to called party to end the call on their end */
		Conn.send_message_to_dest(linfo, Connector::PM_RELEASE);
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
		linfo->state = LS_RINGING;
		break;

	case LS_RINGING: /* Called perspective */
		break;

	case LS_ANSWER: /* Called perspective */
		{
			/* Send message back to caller that the called party answered */
			Conn.send_peer_message(linfo, Connector::PM_ANSWERED);
			LOG_DEBUG(TAG, "Setting call state to LS_ANSWERED");
			linfo->state = LS_ANSWERED;
		}
		break;

	case LS_ANSWERED: /* Called perspective */
		break;

	case LS_CALLED_PARTY_HUNGUP: /* Called perspective */
		{
			Conn.send_peer_message(linfo, Connector::PM_CALLED_PARTY_HUNGUP);
			linfo->state = LS_RESET;
		}
		break;



	case LS_ORIG_DISCONNECT: /* Called perspective */
		/* Send congestion to called party after caller hangs up */
		/* We don't tell the line card to end the call right away */
		/* We wait until we see the hangup event from the called party */
		/* then we send end call */
		/* Seize a junctor */

		if(Xps_logical.seize(&linfo->jinfo)) {
			Conn.prepare(linfo, Connector::ET_LINE, this->_line_to_service);
			linfo->junctor_seized = true;
			linfo->state = LS_ORIG_DISCONNECT_B;
		}
		break;

	case LS_ORIG_DISCONNECT_B: /* Called perspective */
		/* Seize a tone generator */
		if((linfo->tone_plant_descriptor = Tone_plant.channel_seize()) > -1) {
			linfo->state = LS_ORIG_DISCONNECT_C;
		}
		break;

	case LS_ORIG_DISCONNECT_C: /* Called perspective */
		/* Connect phone to junctor */
		Xps_logical.connect_phone_orig(&linfo->jinfo, this->_line_to_service);
		/* Connect tone plant to junctor */
		Xps_logical.connect_tone_plant_output(&linfo->jinfo, linfo->tone_plant_descriptor);
		/* Send Congestion */
		Conn.send_congestion(linfo);
		/* Start the dial timer for congestion time out*/
		osTimerStart(linfo->dial_timer, DTMF_DIGIT_DIAL_TIME);
		linfo->state = LS_ORIG_DISCONNECT_D;


		break;

	case LS_ORIG_DISCONNECT_D: /* Called perspective */
		/* Wait for called party to hang up */
		break;

	case LS_ORIG_DISCONNECT_E: /*Called perspective */
		/* Tell the line card we're done with this call */
		Card_comm.send_command(Card_Comm::RT_LINE, this->_line_to_service, REG_END_CALL);
		linfo->state = LS_RESET;
		break;

	case LS_ORIG_DISCONNECT_F: /* Called perspective */
		/* Called party did not hang up after the  congestion time out */
		/*Disconnect tone plant and junctor */

		if(linfo->tone_plant_descriptor != -1) {
			Tone_plant.channel_release(linfo->tone_plant_descriptor);
			linfo->tone_plant_descriptor = -1;
		}
		if(linfo->junctor_seized) {
			Xps_logical.release(&linfo->jinfo);
			linfo->junctor_seized = false;
		}
		/* Wait for hangup */
		linfo->state = LS_ORIG_DISCONNECT_D;
		break;



	case LS_RESET:
		/* Stop dial timer if it was running */
		osTimerStop(linfo->dial_timer);
		/* Release any resources first */
		Conn.release_tone_generator(linfo);
		Conn.release_mf_receiver(linfo);
		Conn.release_dtmf_receiver(linfo);

		/* Then release the junctor if we seized it initially */
		if(linfo->junctor_seized) {
			Xps_logical.release(&linfo->jinfo);
			linfo->junctor_seized = false;
		}
		linfo->peer = NULL;
		linfo->state = LS_IDLE;
		linfo->called_party_hangup = false;
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
		POST_ERROR(Err_Handler::EH_IPLN);
	}
	/* ToDo: Send command to card to power off */


}



} /* End namespace Sub_Line */
