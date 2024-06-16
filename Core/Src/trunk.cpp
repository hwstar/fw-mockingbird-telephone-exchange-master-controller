#include "top.h"
#include "util.h"
#include "logging.h"
#include "err_handler.h"
#include "card_comm.h"
#include "mf_receiver.h"
#include "drv_dtmf.h"
#include "tone_plant.h"
#include "xps_logical.h"
#include "connector.h"
#include "hw_pres.h"
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
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	Connector::Conn_Info *tinfo = (Connector::Conn_Info *) parameter;

	if(error_code == MF_Decoder::MFE_OK) {
		/* LOG_DEBUG(TAG, "Received MF address: %s on trunk %d", data, tinfo->phys_line_trunk_number); */
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
 * The MF sender has completed sending the address digits.
 * Set the next trunk state.
 */


static void __mf_sending_complete(uint32_t descriptor, void *data) {
	Trunks._mf_sending_complete(descriptor, data);
}

void Trunk::_mf_sending_complete(uint32_t descriptor, void *data) {
	Connector::Conn_Info *tinfo = (Connector::Conn_Info *) data;
	if(!tinfo) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	if(tinfo->state == TS_OUTGOING_SEND_ADDR_INFO_B) {
		tinfo->state = TS_OUTGOING_SEND_ADDR_INFO_C;
	}

	osMutexRelease(this->_lock); /* Release the lock */


}



/*
 * This gets called when an event from a trunk card is received
 */

void Trunk::event_handler(uint32_t event_type, uint32_t resource) {


	if(resource  >= MAX_TRUNK_CARDS) {
		POST_ERROR(Err_Handler::EH_ITN);
	}

	/* LOG_DEBUG(TAG, "Received event from physical trunk %u. Type: %u",resource, event_type); */

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	Connector::Conn_Info *tinfo = &this->_conn_info[resource];

	/* Incoming call from trunk */

	/* Only change trunk states if online, ignore them otherwise */

	if(tinfo->state != TS_OFFLINE) {

		/* Act on event */

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


			case TS_TANDEM_CALL:
			case TS_TANDEM_ADVANCE:
			case TS_TANDEM_SEND_ADDR_INFO:
			case TS_TANDEM_WAIT_ADDR_SENT:
			case TS_TANDEM_CONNECT_CALLER:
			case TS_TANDEM_WAIT_SUPV:
			case TS_TANDEM_SUPV:
			case TS_TANDEM_IN_CALL:
			case TS_TANDEM_SEND_CONGESTION:
			case TS_TANDEM_CONGESTION:
				tinfo->state = TS_TANDEM_CALLER_DISCONNECTED;
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
			break;

		case EV_FAREND_SUPV:
			if(tinfo->state == TS_OUTGOING_WAIT_SUPV) {
				tinfo->state = TS_OUTGOING_ANSWERED;
			}
			break;

		case EV_FAREND_DISC:
			if(tinfo->state == TS_OUTGOING_IN_CALL) {
				tinfo->state = TS_OUTGOING_SEND_FAREND_DISC;
			}
			break;


		}
	}
	osMutexRelease(this->_lock); /* Release the lock */
}

/* This handler receives messages from the connection peer */

uint32_t Trunk::peer_message_handler(Connector::Conn_Info *conn_info, uint32_t phys_line_trunk_number, uint32_t message, void *data) {

	if(!conn_info) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	if(phys_line_trunk_number >= MAX_TRUNK_CARDS) {
		POST_ERROR(Err_Handler::EH_INVP);
	}

	/* Point to the connection info for the physical line number */
	Connector::Conn_Info *tinfo = &this->_conn_info[phys_line_trunk_number];

	uint32_t res = Connector::PMR_OK;

	switch(message) {
	case Connector::PM_SEIZE:
		/* LOG_DEBUG(TAG,"Seize from equip_type %u, phys_line/trunk %u", conn_info->equip_type, conn_info->phys_line_trunk_number); */
		/* Seize trunk for outgoing call */
		if(tinfo->state == TS_IDLE) {
			/* Save caller connection info for permanent use */
			tinfo->peer = conn_info;
			/* LOG_DEBUG(TAG, "Seizing trunk %u", phys_line_trunk_number); */
			tinfo->state = TS_OUTGOING_START;
		}
		else {
			/* LOG_DEBUG(TAG, "Sending trunk busy PM at seizure"); */
			res = Connector::PMR_TRUNK_BUSY;
		}
		break;

	case Connector::PM_RELEASE:
		/* Release trunk used for outgoing call */
		/* LOG_DEBUG(TAG, "Got release from peer, current state is %u", tinfo->state); */
		if(!tinfo->pending_state) {
			tinfo->pending_state = TS_RELEASE_TRUNK;
		}
		else {
			LOG_ERROR(TAG, "Pending state set previously");
		}
		break;


	case Connector::PM_ANSWERED:
		/* Called party answered */
		if(tinfo->state == TS_INCOMING_WAIT_SUPV) {
			tinfo->peer = conn_info;
			tinfo->state = TS_INCOMING_CONNECT_AUDIO;
		}
		else if(tinfo->state == TS_TANDEM_WAIT_SUPV) {
			tinfo->peer = conn_info;
			tinfo->state = TS_TANDEM_SUPV;

		}
		break;

	case Connector::PM_CALLED_PARTY_HUNGUP:
		/* Called party hung up */
		if(tinfo->state == TS_TANDEM_IN_CALL) {
			tinfo->called_party_hangup = true;
			tinfo->state = TS_TANDEM_CALLED_DISCONNECTED;
		}
		else if(tinfo->state == TS_INCOMING_ANSWERED) {
			/* LOG_DEBUG(TAG, "Called party hung up"); */
			tinfo->called_party_hangup = true;
			tinfo->state = TS_INCOMING_TEARDOWN;
		}
		break;


	case Connector::PM_TRUNK_ADDR_INFO_READY:
		if(tinfo->state == TS_OUTGOING_WAIT_ADDR_INFO) {
			tinfo->state = TS_OUTGOING_SEND_ADDR_INFO;
		}
		break;

	case Connector::PM_TRUNK_BUSY:
		if(tinfo->state == TS_TANDEM_CALL) {
			LOG_INFO(TAG, "Trunk busy returned by PM");
			tinfo->state = TS_TANDEM_ADVANCE;

		}
		break;

	case Connector::PM_TRUNK_NO_WINK:
		if(tinfo->state == TS_TANDEM_CALL) {
			uint32_t dest_trunk_number = Conn.get_called_phys_line_trunk(tinfo);
			LOG_WARN(TAG, "No wink seen on trunk: %u", dest_trunk_number);
			tinfo->state = TS_TANDEM_ADVANCE;
		}
		break;

	case Connector::PM_TRUNK_READY_FOR_ADDR_INFO:
		if(tinfo->state == TS_TANDEM_CALL) {
			tinfo->state = TS_TANDEM_SEND_ADDR_INFO;
		}
		break;


	case Connector::PM_TRUNK_READY_TO_CONNECT_CALLER:
		if(tinfo->state == TS_TANDEM_WAIT_ADDR_SENT) {
			tinfo->state = TS_TANDEM_CONNECT_CALLER;
		}
		break;


	default: {
		uint32_t dest_trunk_number = Conn.get_called_phys_line_trunk(tinfo);
		LOG_ERROR(TAG, "Unhandled message. Message: %d, state: %d, trunk number: %d", message, tinfo->state, dest_trunk_number);
		break;
	}

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
		POST_ERROR(Err_Handler::EH_LCE);
	}

	for(uint8_t index = 0; index < MAX_TRUNK_CARDS; index++) {
		Connector::Conn_Info *tinfo = &this->_conn_info[index];
		tinfo->state = TS_IDLE;
		tinfo->pending_state = TS_IDLE;
		tinfo->phys_line_trunk_number = index;
		tinfo->equip_type = Connector::ET_TRUNK;
		tinfo->tone_plant_descriptor = tinfo->mf_receiver_descriptor = tinfo->dtmf_receiver_descriptor = -1;


	}


}


/*
 * Test for a pending state, if there is one, set the current state to it,
 * then clear the pending state and return true.
 *
 * If there is no pending state, return false;
 */

bool Trunk::_test_pending_state(Connector::Conn_Info *tinfo) {
	if(!tinfo) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	if(tinfo->pending_state) {
			tinfo->state = tinfo->pending_state;
			tinfo->pending_state = TS_IDLE;
			return true;
		}
	return false;
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
			case Connector::ROUTE_DEST_TRUNK_BUSY:
				/* If destination is a trunk, then we need to try to select another trunk in the group */
				if(tinfo->route_info.dest_equip_type == Connector::ET_TRUNK) {
					tinfo->state = TS_TANDEM_ADVANCE;

				}
				else if (tinfo->route_info.dest_equip_type == Connector::ET_LINE) {
					tinfo->state = TS_SEND_CONGESTION;
				}
				else {
					POST_ERROR(Err_Handler::EH_IET);
				}
				break;

			case Connector::ROUTE_DEST_BUSY:
				tinfo->state = TS_SEND_BUSY;
				break;

			case Connector::ROUTE_DEST_CONNECTED:
				/* Determine if local or tandem connection */
				if(tinfo->route_info.dest_equip_type == Connector::ET_LINE) {
					tinfo->state = TS_SEND_RINGING;
				}
				else if (tinfo->route_info.dest_equip_type == Connector::ET_TRUNK) {
					tinfo->state = TS_TANDEM_CALL;
				}
				else {
					POST_ERROR(Err_Handler::EH_IET);
				}
				break;

			default:
				POST_ERROR(Err_Handler::EH_INVR);
				break;
			}
			break;

		default:
			POST_ERROR(Err_Handler::EH_INVR);
			break;
		}
		break;
	}

	/*
	 * Incoming local trunk states
	 */

	case TS_SEND_RINGING:
		Conn.send_ringing(tinfo);
		tinfo->state = TS_INCOMING_WAIT_SUPV;
		break;

	case TS_RINGING_TEARDOWN: {
		Conn.send_message_to_dest(tinfo, Connector::PM_RELEASE);
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


	/*
	 * Incoming tandem trunk states
	 */

	case TS_TANDEM_CALL:
	    /* Wait for outgoing trunk peer message response */
		break;

	case TS_TANDEM_SEND_ADDR_INFO: {
		/* Send address info to outgoing tandem trunk */
		uint8_t start = tinfo->route_info.dest_dial_start_index + 1;
		uint8_t end = strlen(tinfo->route_info.dialed_number);

		/* Make the trunk dial string */
		Utility.make_trunk_dial_string(tinfo->trunk_outgoing_address, tinfo->digit_buffer, start, end,
				Connector::MAX_TRUNK_OUTGOING_ADDRESS, tinfo->route_info.trunk_prefix);
		LOG_DEBUG(TAG, "Tandem call outgoing address info: %s", tinfo->trunk_outgoing_address);

		/* Clear everything off of the junctor and let the outgoing trunk do what it needs to do */
		Conn.release_tone_generator(tinfo);
		Conn.disconnect_caller_party_audio(tinfo);

		/* Tell the outgoing trunk the address has been formatted and is ready to send */
		Conn.send_message_to_dest(tinfo, Connector::PM_TRUNK_ADDR_INFO_READY);

		/* Wait for address info to be sent */
		tinfo->state = TS_TANDEM_WAIT_ADDR_SENT;
	}

		break;

	case TS_TANDEM_WAIT_ADDR_SENT:
		/* Wait for MF digits to be sent */
		break;

	case TS_TANDEM_CONNECT_CALLER:
		Conn.connect_caller_party_audio(tinfo);
		tinfo->state = TS_TANDEM_WAIT_SUPV;
		/* Connect originating trunk to terminating trunk */
		break;

	case TS_TANDEM_WAIT_SUPV:
		/* Wait for trunk supervision on outgoing end of call */
		break;

	case TS_TANDEM_SUPV:
		/* Send answer supervision back to caller's switch */
		LOG_DEBUG(TAG, "Tandem answer supervision seen, relaying to originator");
		Card_comm.send_command(Card_Comm::RT_TRUNK, this->_trunk_to_service, REG_INCOMING_CONNECTED);
		tinfo->state = TS_TANDEM_IN_CALL;
		break;

	case TS_TANDEM_IN_CALL:
		/* Wait for PM or trunk card event */
		break;

	case TS_TANDEM_ADVANCE:
		/* If the originating trunk tone generator was disconnected, reconnect it here */
		if(tinfo->tone_plant_descriptor == -1) {
			Conn.seize_and_connect_tone_generator(tinfo);
		}

		tinfo->state = TS_TANDEM_SEND_CONGESTION;

		/* Todo: select next outgoing trunk for tandem operation */
		/* Trunk was busy */
		/* Advance to the next trunk in the group */
		break;

	case TS_TANDEM_SEND_CONGESTION:
		/* Seize a tone generator */
		Conn.send_congestion(tinfo);
		/* Wait for caller to disconnect */
		tinfo->state = TS_TANDEM_CONGESTION;
		break;


	case TS_TANDEM_CONGESTION:
		/* Wait for caller to hang up */
		break;



	case TS_TANDEM_CALLER_DISCONNECTED:
		/* Calling party disconnected */
		LOG_DEBUG(TAG, "Tandem calling party disconnected");
		Conn.send_message_to_dest(tinfo, Connector::PM_RELEASE); /* Release outgoing trunk */
		tinfo->state = TS_RESET;
		break;


	case TS_TANDEM_CALLED_DISCONNECTED:
		LOG_DEBUG(TAG, "Tandem called party disconnected");
		/* Called party disconnected */
		Card_comm.send_command(Card_Comm::RT_TRUNK, this->_trunk_to_service, REG_DROP_CALL);
		tinfo->state = TS_RESET;
		break;


	/*
	 * Outgoing trunk states
	 */

	case TS_OUTGOING_START:
		/* Test for call drop */
		if(this->_test_pending_state(tinfo)) {
			break;
		}

		/* Seize the trunk */
		LOG_DEBUG(TAG, "Send seize trunk command to card, wait for wink or busy");
		Card_comm.send_command(Card_Comm::RT_TRUNK, this->_trunk_to_service, REG_SEIZE_TRUNK);
		tinfo->state = TS_WAIT_WINK_OR_BUSY;
		break;

	case TS_WAIT_WINK_OR_BUSY:
		/* 4 possible outcomes:
		 * 1. receive wink event
		 * 2. receive busy event.
		 * 3. receive wink time out event.
		 * 4. caller hangs up.
		 */
		if(this->_test_pending_state(tinfo)) {
			break;
		}
		break;

	case TS_OUTGOING_REQUEST_ADDR_INFO: {
		/* Test for call drop */
		if(this->_test_pending_state(tinfo)) {
			break;
		}

		/* Got the wink event */
		/* Send message to caller to request address information */
		/* LOG_DEBUG(TAG, "Sending Ready for Address Info"); */
		Conn.send_peer_message(tinfo, Connector::PM_TRUNK_READY_FOR_ADDR_INFO);
		tinfo->state = TS_OUTGOING_WAIT_ADDR_INFO;
	}
		break;

	case TS_OUTGOING_WAIT_ADDR_INFO:
		/* Wait for caller state machine to send address information */
		/* Test for call drop */
		if(this->_test_pending_state(tinfo)) {
			break;
		}
		break;


	case TS_OUTGOING_SEND_ADDR_INFO:
		/* Test for call drop */
		if(this->_test_pending_state(tinfo)) {
				break;
		}
		/* Seize a tone plant channel */
		/* Utilize the peer data structure for this */
		if((tinfo->peer->tone_plant_descriptor = Tone_plant.channel_seize()) != -1) {
			/* Connect the tone plant to the junctor */
			Xps_logical.connect_tone_plant_output(&tinfo->peer->jinfo, tinfo->peer->tone_plant_descriptor, false);
			/* Connect the outgoing trunk to the junctor */
			Conn.connect_called_party_audio(tinfo->peer);
			tinfo->state = TS_OUTGOING_SEND_ADDR_INFO_B;
			/* Send Address info in MF */
			Tone_plant.send_mf(tinfo->peer->tone_plant_descriptor, tinfo->peer->trunk_outgoing_address, __mf_sending_complete, tinfo);

		}
		break;

	case TS_OUTGOING_SEND_ADDR_INFO_B:
		/* Test for call drop */
		if(this->_test_pending_state(tinfo)) {
				break;
		}
		/* Wait for MF digits to be sent */
		break;

	case TS_OUTGOING_SEND_ADDR_INFO_C: {
		/* Test for call drop */
		if(this->_test_pending_state(tinfo)) {
				break;
		}
		/* MF Digits have been sent */
		/* Release the tone generator */
		/* LOG_DEBUG(TAG, "MF Address sent"); */
		Conn.release_tone_generator(tinfo->peer);

		/* Send outgoing address complete message to trunk card */
		Card_comm.send_command(Card_Comm::RT_TRUNK, this->_trunk_to_service, REG_OUTGOING_ADDR_COMPLETE);

		/* Send message to peer that the caller can now be connected. */
		Conn.send_peer_message(tinfo, Connector::PM_TRUNK_READY_TO_CONNECT_CALLER);
		tinfo->state = TS_OUTGOING_WAIT_SUPV;
	}
		break;

	case TS_OUTGOING_WAIT_SUPV:
		/* Test for call drop */
		if(this->_test_pending_state(tinfo)) {
				break;
		}
		/* Call connected, wait for called party to answer */
		break;

	case TS_OUTGOING_ANSWERED: {
		/* The party on the far end of the trunk has answered */
		Conn.send_peer_message(tinfo, Connector::PM_ANSWERED);
		tinfo->state = TS_OUTGOING_IN_CALL;
	}
		break;


	case TS_OUTGOING_IN_CALL:
		/* Test for call drop */
		if(this->_test_pending_state(tinfo)) {
			break;
		}
		/* In an outgoing call over a trunk */
		break;

	case TS_OUTGOING_SEND_FAREND_DISC: {
		/* Received disconnect from the far end of a trunk */
		Conn.send_peer_message(tinfo, Connector::PM_CALLED_PARTY_HUNGUP);
		tinfo->state = TS_RESET;
	}
		break;



	case TS_GOT_NO_WINK: {
		/* Timed out waiting for wink */
	    /* Disconnect the tone generator, the originator will reconnect it to the correct place if need be */
		Conn.release_tone_generator(tinfo->peer);
		LOG_DEBUG(TAG, "Sending Wink timeout PM");
		Conn.send_peer_message(tinfo, Connector::PM_TRUNK_NO_WINK);
		tinfo->state = TS_RELEASE_TRUNK;
	}
		break;




	case TS_SEND_TRUNK_BUSY: {
		/* LOG_DEBUG(TAG, "Sending trunk busy PM"); */
		/* Disconnect the tone generator, the originator will reconnect it to the correct place if need be */
		Conn.release_tone_generator(tinfo->peer);
		Conn.send_peer_message(tinfo, Connector::PM_TRUNK_BUSY);
		tinfo->state = TS_RELEASE_TRUNK;
	}
		break;


	case TS_RELEASE_TRUNK:
		/* LOG_DEBUG(TAG, "Sending release command to trunk card %u", this->_trunk_to_service); */
		Card_comm.send_command(Card_Comm::RT_TRUNK, this->_trunk_to_service, REG_DROP_CALL);
		tinfo->state = TS_RESET;
		break;


	case TS_RESET:
		/* Clean up */
		/* Release any resources first */
		Conn.release_tone_generator(tinfo);
		Conn.release_mf_receiver(tinfo);
		Conn.release_dtmf_receiver(tinfo);

		/* Then release the junctor if we seized it initially */
		if(tinfo->junctor_seized) {
			Xps_logical.release(&tinfo->jinfo);
			tinfo->junctor_seized = false;
		}
		tinfo->pending_state = TS_IDLE;
		tinfo->called_party_hangup = false;
		tinfo->peer = NULL;
		tinfo->state = TS_IDLE;
		break;

	case TS_OFFLINE:
		break;


	default:
		LOG_ERROR(TAG, "Unhandled state %u", tinfo->state);
		tinfo->state = TS_RESET;
		break;
	}


	this->_trunk_to_service++;
	if(this->_trunk_to_service >= MAX_TRUNK_CARDS) {
		this->_trunk_to_service = 0;
	}



	osMutexRelease(this->_lock); /* Release the lock */


}

/*
 * Place a trunk off line for testing purposes
 *
 * Returns true if successful
 */

bool Trunk::go_offline(uint32_t trunk_number) {

	uint8_t trunk_map = HW_pres.get_trunk_card_positions();
	if((trunk_number > MAX_TRUNK_CARDS) || ((trunk_map & (1 << trunk_number)) == 0)) {
		return false;
	}

	bool res = true;
	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	Connector::Conn_Info *tinfo = &this->_conn_info[trunk_number];

	if(tinfo->state == TS_IDLE) {
		tinfo->state = TS_OFFLINE;
	}
	else {
		res = false;
	}



	osMutexRelease(this->_lock); /* Release the lock */

	return res;

/*
 * Place a trunk on line for testing purposes
 *
 * Returns true if successful
 */


}
bool Trunk::go_online(uint32_t trunk_number) {

	uint8_t trunk_map = HW_pres.get_trunk_card_positions();

	if((trunk_number > MAX_TRUNK_CARDS) || ((trunk_map & (1 << trunk_number)) == 0)) {
		return false;
	}

	bool res = true;
	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	Connector::Conn_Info *tinfo = &this->_conn_info[trunk_number];

	if(tinfo->state == TS_OFFLINE) {
		tinfo->state = TS_IDLE;
	}
	else {
		res = false;
	}


	osMutexRelease(this->_lock); /* Release the lock */

	return res;

}

/*
 * Test to see if a trunk is in use.
 *
 * Will return false if the trunk is idle.
 *
 * Will return true if the trunk is in use, or not installed.
 */


bool Trunk::is_in_use(uint32_t trunk_number) {
	uint8_t trunk_map = HW_pres.get_trunk_card_positions();

	if((trunk_number > MAX_TRUNK_CARDS) || ((trunk_map & (1 << trunk_number)) == 0)) {
		return true;
	}

	bool res = true;
	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	Connector::Conn_Info *tinfo = &this->_conn_info[trunk_number];

	if(tinfo->state == TS_IDLE) {
		res = false;
	}

	osMutexRelease(this->_lock); /* Release the lock */

	return res;

}

} /* End namespace trunk */

