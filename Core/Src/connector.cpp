#include <string.h>
#include "top.h"
#include "logging.h"
#include "err_handler.h"
#include "util.h"
#include "pool_alloc.h"
#include "xps_logical.h"
#include "config_rw.h"
#include "tone_plant.h"
#include "mf_receiver.h"
#include "drv_dtmf.h"
#include "sub_line.h"
#include "trunk.h"




namespace Connector {

const char *TAG = "connector";

const Tone_Plant::Audio_Sequence_List_Type _receiver_lifted_sequence[2] = {
		{Tone_Plant::ASEQ_CMD_SEND_ULAW, false, 0.0, NULL, NULL, 0, "receiver_lifted"},
		{Tone_Plant::ASEQ_CMD_SEND_CPT, false, 0.0, NULL, NULL, Tone_Plant::CPT_DIAL_TONE, NULL }

};


/*
 * Test the digits currently received against a route table entry
 */

uint32_t Connector::_test_against_route(const char *string_to_test, const char *route_table_entry) {
	uint32_t match_type = ROUTE_INDETERMINATE;
	const char *r = route_table_entry;
	const char *s = string_to_test;

	/* If route table entry starts with '_' then this is a pattern match */

	if(route_table_entry[0] == '_') {
		r++;
	}
	while(*r && *s) {
		if(route_table_entry[0] == '_') {
			/* Pattern match mode */
			if((*r == 'N') && (*s >= '2') && (*s <= '9')) { /* Match 2-9 */
				r++;
				s++;
				continue;
			}
			else if((*r == 'X') && (*s >= '0') && (*s <= '9')) { /* Match 0-9 */
				r++;
				s++;
				continue;

			}
		}
		if(*r == *s) {
			r++;  /* Verbatim match */
			s++;
			continue;
		}
		else {
			/* Match failed */
			match_type = ROUTE_INVALID;
			break;
		}
	}

	/* If at the end of both strings */
	if((*r == 0) && (*s == 0) == true) {
		match_type = ROUTE_VALID;
	}

	return match_type;
}




/*
 * Called once after RTOS is initialized
 */

void Connector::init(void) {

}

/*
 * Called when the system is about to go on line
 *
 * Read configuration data and set up configuration here
 */

void Connector::config() {

}



/*
 * Prepare for a connection
 */

void Connector::prepare(Conn_Info *conn_info, uint32_t source_equip_type, uint32_t source_phys_line_number) {
	if(!conn_info) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	/* Reset digit buffer and digit counts */
	conn_info->num_dialed_digits = conn_info->prev_num_dialed_digits = 0;
	conn_info->digit_buffer[0] = 0;

	/* Reset the pointer to the peer */
	conn_info->peer = NULL;

	/* Configure initial routing info */
	Utility.memset(&conn_info->route_info, 0, sizeof(conn_info->route_info));
	conn_info->route_info.state = ROUTE_INDETERMINATE;
	conn_info->route_info.source_equip_type = source_equip_type;
	conn_info->route_info.source_phys_line_number = source_phys_line_number;
	conn_info->route_info.trunk_prefix = NULL;
}


/*
 * Called by line or trunk objects to test a route
 *
 * Returns a result code ROUTE_* which must be checked.
 */


uint32_t Connector::test(Conn_Info *conn_info, const char *dialed_digits) {


	if((!dialed_digits) || (!conn_info)) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	Route_Info *route_info = &conn_info->route_info;

	if(route_info->state == ROUTE_INVALID) {
		return ROUTE_INVALID;
	}



	uint32_t res = ROUTE_INDETERMINATE;

	/* Allocate a working string */
	char *alloc_str = Utility.allocate_long_string();
	/* The first time this is called, we need to compute a pointer to the routing table. */
	/* Once this is done, subsequent calls will use the computed pointer. */
	if(!route_info->rt_head) {
		/* Locate the routing table */
		unsigned pltn = route_info->source_phys_line_number;
		const char *start_section;
		snprintf(alloc_str, Util::LONG_STRINGS_SIZE, "%u/routing_table", pltn);
		switch(route_info->source_equip_type) {
		case ET_LINE:
			start_section="subscribers";
			break;

		case ET_TRUNK:
			start_section="incoming_trunks";
			break;

		default:
			POST_ERROR(Err_Handler::EH_UHC);
			break;
		}

		/* Get the node with the routing table information */
		route_info->rt_head = Config_rw.find_node_by_path(start_section, alloc_str);

		if(!route_info->rt_head) {
			/* Configuration invalid after being validated at boot up */
			/* This shouldn't happen, but we catch it here if it does */
			POST_ERROR(Err_Handler::EH_BRV);
		}
	}

	/* Deallocate working string */
	Utility.deallocate_long_string(alloc_str);

	/* At this point, we have a pointer to the routing table node */
	/* We now need to test the dialed digits against what is held in the routing table */

	/* Retrieve the route table section header */
	Config_RW::Config_Section_Type *route_table = Config_rw.find_section(route_info->rt_head->value);
	if(!route_table) {
		POST_ERROR(Err_Handler::EH_BRV);
	}
	/* Traverse the routing table list and attempt to match the dialed digits */
	Config_RW::Config_Node_Type *node = route_table->head;

	if(!node) {
		POST_ERROR(Err_Handler::EH_INVR);
	}

	for(;node; node = node->next) {
		/* Compare the key against the dialed digits */
		res = this->_test_against_route(dialed_digits, node->key);
		if(res == ROUTE_VALID) {
			LOG_DEBUG(TAG, "Valid route: %s", dialed_digits);
			break;
		}
	}
	route_info->state = (uint8_t) res;
	if(res == ROUTE_VALID) {
		/* Retrieve the destination information from the route */
		uint32_t substring_count = 3;
		char *substrings[3];
		/* Split value on comma */
		char *alloc_str = Utility.str_split(node->value, substrings, substring_count, ',');
		if(substring_count != 2) {
			POST_ERROR(Err_Handler::EH_INVR);
		}
		/* First string is the destination equipment type */
		/* Second string is the physical line node for lines or */
		/* a trunk group for trunks */
		static const char *routing_keywords[] = {"sub", "tg", NULL};
		switch(Utility.keyword_match(substrings[0], routing_keywords)) {
		case 0:
			route_info->dest_equip_type = ET_LINE;
			break;

		case 1:
			route_info->dest_equip_type = ET_TRUNK;
			break;

		default:
			POST_ERROR(Err_Handler::EH_UHC);
			break;

		}

		/* Act on destination line/trunk choice */

		if(route_info->dest_equip_type == ET_LINE) {
			/* Look up the destination line info section head*/
			Config_RW::Config_Section_Type *dl_section = Config_rw.find_section(substrings[1]);
			if(!dl_section) {
				POST_ERROR(Err_Handler::EH_BRV);
			}
			/* Destination section points to destination line */
			route_info->dest_section = dl_section;

			/* Deallocate working string */
			Utility.deallocate_long_string(alloc_str);

			/* Locate the physical line number for the destination */
			Config_RW::Config_Node_Type *dl_node = Config_rw.find_node("phys_line", dl_section->head);
			if(!dl_node) {
				POST_ERROR(Err_Handler::EH_BRV);
			}
			unsigned dest_phys_line_num;
			if(sscanf(dl_node->value, "%u", &dest_phys_line_num) != 1) {
				POST_ERROR(Err_Handler::EH_IPLN);
			}
			/* Update the routing info with the destination information */
			route_info->dest_line_trunk_count = 1;
			route_info->dest_phys_lines_trunks[0] = (uint8_t) dest_phys_line_num;
			/* Copy dialed digits for reference later */
			Utility.strncpy_term(route_info->dialed_number, dialed_digits, sizeof(route_info->dialed_number));
			LOG_DEBUG(TAG, "Route table updated for line destination");
		}
		else if(route_info->dest_equip_type == ET_TRUNK) {
			/* Look up the trunk group */
			Config_RW::Config_Section_Type *tg_section = Config_rw.find_section(substrings[1]);
			if(!tg_section) {
				POST_ERROR(Err_Handler::EH_BRV);
			}
			/* Destination section points to trunk group */
			route_info->dest_section = tg_section;
			/* Deallocate working string */
			Utility.deallocate_long_string(alloc_str);

			/* Look up mandatory key first */
			Config_RW::Config_Node_Type *tl_node = Config_rw.find_node("trunk_list", tg_section->head);
			if(!tl_node) {
				POST_ERROR(Err_Handler::EH_INVR);
			}
			/* Split into substrings to get trunk sections */
			substring_count = 3;
			char *alloc_str = Utility.str_split(tl_node->value, substrings, substring_count, ',');
			/* Look up all physical trunks and add their info to the route table */
			for(uint32_t i = 0; i < substring_count; i++) {
				Config_RW::Config_Section_Type *pt_section = Config_rw.find_section(substrings[i]);
				if(!pt_section) {
					POST_ERROR(Err_Handler::EH_BRV);
				}
				Config_RW::Config_Node_Type *pt_node = Config_rw.find_node("phys_trunk", pt_section->head);
				if(!pt_node) {
					POST_ERROR(Err_Handler::EH_INVR);
				}
				/* Convert value from char * to number */
				unsigned dest_phys_trunk_num;
				if(sscanf(pt_node->value, "%u", &dest_phys_trunk_num) != 1) {
					POST_ERROR(Err_Handler::EH_IPLN);
				}
				/* Add the physical trunk number to the destination trunk table */
				route_info->dest_phys_lines_trunks[route_info->dest_line_trunk_count] = (uint8_t) dest_phys_trunk_num;
				/* Increment the destination trunk count */
				route_info->dest_line_trunk_count++;
				/* Copy dialed digits for reference later if they weren't stored previously */
				if(!route_info->dialed_number[0]) {
					Utility.strncpy_term(route_info->dialed_number, dialed_digits, sizeof(route_info->dialed_number));
				}


			}
			/* Look up the optional start_index key */
			Config_RW::Config_Node_Type *si_node = Config_rw.find_node("start_index", tg_section->head);
			/* If found */
			if(si_node) {
				unsigned start_index;
				if(sscanf(si_node->value, "%u", &start_index) != 1) {
					POST_ERROR(Err_Handler::EH_IPLN);
				}
				route_info->dest_dial_start_index = (uint8_t) start_index;
			}
			/* Look up the optional prefix string pointer and add it to the route info */
			Config_RW::Config_Node_Type *prefix_node = Config_rw.find_node("prefix", tg_section->head);
			if(prefix_node) {
				route_info->trunk_prefix = prefix_node->value;
			}

			/* Deallocate working string */
			Utility.deallocate_long_string(alloc_str);

			LOG_DEBUG(TAG, "Route table updated for trunk destination");
		}
		else {
			POST_ERROR(Err_Handler::EH_UHC);
		}
	} /* Endif route valid */
	return res;
}



/*
 * Resolve an originating call to get to the desired termination.
 *
 * Returns a result code which must be checked.
 *
 * Return values:
 *
 * ROUTE_INVALID - Routing info supplied was incomplete or invalid
 * ROUTE_DEST_CONNECTED - Connected to destination
 * ROUTE_DEST_BUSY - Destination is busy
 * ROUTE_DEST_CONGESTED - No free path to destination
 */


uint32_t Connector::resolve(Conn_Info *conn_info) {

	if(!conn_info) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	Route_Info *ri = &conn_info->route_info;

	if(ri->state != ROUTE_VALID) {
		return (ri->state = ROUTE_INVALID);
	}
	uint32_t res = ROUTE_DEST_CONNECTED;

	conn_info->trunk_index = 0; /* Select the first trunk */
	/* Try to seize it */
	uint32_t pm_res = this->send_peer_message(
			conn_info,
			conn_info->route_info.dest_equip_type,
			conn_info->route_info.dest_phys_lines_trunks[conn_info->trunk_index],
			PM_SEIZE);
	/* Set the result for the caller */
	switch(pm_res) {
	case PMR_OK:
		res = ROUTE_DEST_CONNECTED;
		break;

	case PMR_BUSY:
		res = ROUTE_DEST_BUSY;
		break;

	case PMR_TRUNK_BUSY:
		res = ROUTE_DEST_TRUNK_BUSY;
		break;

	default:
		POST_ERROR(Err_Handler::EH_BRV);
		break;
	}

	ri->state = res;
	return res;
}

/*
 * Call when previous resolve or resolve_next trunk returns ROUTE_DEST_TRUNK_BUSY or the Trunk card returned BUSY after a seize message was sent to it.
 * This method will attempt to use the next trunk in the list, if there are no more trunks
 * in the list a ROUTE_NO_MORE_TRUNKS will be returned, otherwise ROUTE_DEST_CONNECTED, or ROUTE_DEST_TRUNK_BUSY will be returned.
 */
uint32_t Connector::resolve_try_next_trunk(Conn_Info *conn_info) {
	uint32_t res = ROUTE_DEST_CONNECTED;

	if(!conn_info) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	Route_Info *ri = &conn_info->route_info;

	if(ri->dest_equip_type != ET_TRUNK) {
		/* Cannot be called unless destination is a trunk */
		POST_ERROR(Err_Handler::EH_ETNT);
	};
	if((ri->state == ROUTE_INVALID) || (ri->state == ROUTE_INDETERMINATE)) {
		POST_ERROR(Err_Handler::EH_IRS);
	}

	/* See if there's another trunk available in the list */
	if(ri->state != ROUTE_NO_MORE_TRUNKS) {
		conn_info->trunk_index++;
	}
	if(conn_info->trunk_index >= ri->dest_line_trunk_count) {
		res = ROUTE_NO_MORE_TRUNKS;
	}
	else {
		/* Try seizing the next trunk in the list */
		uint32_t pm_res = this->send_peer_message(
				conn_info,
				conn_info->route_info.dest_equip_type,
				conn_info->route_info.dest_phys_lines_trunks[conn_info->trunk_index],
				PM_SEIZE);
		switch(pm_res) {
		case PMR_OK:
			res = ROUTE_DEST_CONNECTED;
			break;

		case PMR_TRUNK_BUSY:
			res = ROUTE_DEST_TRUNK_BUSY;
			break;

		default:
			POST_ERROR(Err_Handler::EH_BRV);
			break;
		}

	}

	/* Remember the route state if called later */
	ri->state = res;
	return res;

}


/*
 * Send message to called destination
 * For use by lines and trunks only in the event process.
 * Does not respect locking
 */

uint32_t Connector::send_message_to_dest(Conn_Info *conn_info, uint32_t message) {
	uint32_t dest_equip_type = this->get_called_equip_type(conn_info);
	uint32_t dest_phys_line_trunk_number = this->get_called_phys_line_trunk(conn_info);
	return this->send_peer_message(conn_info, dest_equip_type, dest_phys_line_trunk_number, message);
}


/*
 * This function sends a message to the other end of the connection
 * For use by lines and trunks only in the event process.
 * Does not respect locking
 * The first signature extracts the destination to be messaged from the peer connection info
 * The second signature allows permits messages to be sent to an arbitrary destination
 */


uint32_t Connector::send_peer_message(Conn_Info *conn_info, uint32_t message) {
	if((!conn_info) || (!conn_info->peer)) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	uint32_t equip_type = Conn.get_caller_equip_type(conn_info->peer);
	uint32_t phys_line_trunk_number = Conn.get_caller_phys_line_trunk(conn_info->peer);
	return this->send_peer_message(conn_info, equip_type, phys_line_trunk_number, message);
}

uint32_t Connector::send_peer_message(Conn_Info *conn_info, uint32_t dest_equip_type,
		uint32_t dest_phys_line_trunk_number, uint32_t message) {

	if(!conn_info) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	uint32_t pm_res;

	switch(dest_equip_type) {
	case ET_LINE:
		if(dest_phys_line_trunk_number >= Sub_Line::MAX_DUAL_LINE_CARDS * 2) {
			POST_ERROR(Err_Handler::EH_INVP);
		}
		pm_res = Sub_line.peer_message_handler(conn_info, dest_phys_line_trunk_number, message);
		break;

	case ET_TRUNK:
		if(dest_phys_line_trunk_number >= Trunk::MAX_TRUNK_CARDS) {
			POST_ERROR(Err_Handler::EH_INVP);
		}
		pm_res = Trunks.peer_message_handler(conn_info, dest_phys_line_trunk_number, message);
		break;

	default:
		POST_ERROR(Err_Handler::EH_IET);
		break;
	}

	return pm_res;

}

/*
 * Return the equipment type of the caller
 */
uint32_t Connector::get_caller_equip_type(Conn_Info *conn_info) {
	if(!conn_info) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	return conn_info->route_info.source_equip_type;

}

/*
 * Return the equipment type of the called party
 */

uint32_t Connector::get_called_equip_type(Conn_Info *conn_info) {
	if(!conn_info) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	return conn_info->route_info.dest_equip_type;

}

/*
 * Return the caller's physical line or trunk number
 */

uint32_t Connector::get_caller_phys_line_trunk(Conn_Info *conn_info) {
	if(!conn_info) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	return conn_info->route_info.source_phys_line_number;
}


/*
 * Return the called party's physical line or trunk number
 */

uint32_t Connector::get_called_phys_line_trunk(Conn_Info *conn_info) {
	uint8_t ltindex;

	if(!conn_info) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	if(this->get_called_equip_type(conn_info) == ET_TRUNK) {
		ltindex = conn_info->trunk_index;
	}
	else {
		ltindex = 0; /* Hunting not supported on lines yet */
	}

	return conn_info->route_info.dest_phys_lines_trunks[ltindex];
}


/*
 * Connect called party to junctor
 */

void Connector::connect_called_party_audio(Conn_Info *linfo) {
	if(!linfo) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	uint32_t called_party_equip_type = Conn.get_called_equip_type(linfo);
	uint32_t called_party_phys_line_trunk = Conn.get_called_phys_line_trunk(linfo);

	if(called_party_equip_type == ET_TRUNK) {
		Xps_logical.connect_trunk_term(&linfo->jinfo, called_party_phys_line_trunk);
	}
	else if (called_party_equip_type == ET_LINE) {
		Xps_logical.connect_phone_term(&linfo->jinfo, called_party_phys_line_trunk);
	}

}

/*
 * Disconnect called party from junctor
 */


void Connector::disconnect_called_party_audio(Conn_Info *linfo) {
	if(!linfo) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	uint32_t called_party_equip_type = Conn.get_called_equip_type(linfo);

	if(called_party_equip_type == ET_TRUNK) {
		Xps_logical.disconnect_trunk_term(&linfo->jinfo);
	}
	else if (called_party_equip_type == ET_LINE) {
		Xps_logical.disconnect_phone_term(&linfo->jinfo);
	}

}


/*
 * Connect called party to junctor
 */

void Connector::connect_caller_party_audio(Conn_Info *linfo) {
	if(!linfo) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	uint32_t caller_party_equip_type = Conn.get_caller_equip_type(linfo);
	uint32_t caller_party_phys_line_trunk = Conn.get_caller_phys_line_trunk(linfo);

	if(caller_party_equip_type == ET_TRUNK) {
		Xps_logical.connect_trunk_orig(&linfo->jinfo, caller_party_phys_line_trunk);
	}
	else if (caller_party_equip_type == ET_LINE) {
		Xps_logical.connect_phone_orig(&linfo->jinfo, caller_party_phys_line_trunk);
	}

}

/*
 * Disconnect called party from junctor
 */


void Connector::disconnect_caller_party_audio(Conn_Info *linfo) {
	if(!linfo) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	uint32_t caller_party_equip_type = Conn.get_caller_equip_type(linfo);

	if(caller_party_equip_type == ET_TRUNK) {
		Xps_logical.disconnect_trunk_orig(&linfo->jinfo);
	}
	else if (caller_party_equip_type == ET_LINE) {
		Xps_logical.disconnect_phone_orig(&linfo->jinfo);
	}

}

/*
 * Release MF receiver and disconnect it from the junctor if seized
 */

void Connector::release_mf_receiver(Conn_Info *linfo) {
	if(!linfo) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	if(linfo->mf_receiver_descriptor != -1) {
		Xps_logical.disconnect_mf_receiver(&linfo->jinfo);
		MF_decoder.release(linfo->mf_receiver_descriptor);
		linfo->mf_receiver_descriptor = -1;

	}

}



/*
 * Release DTMF receiver and disconnect it from the junctor if seized
 */

void Connector::release_dtmf_receiver(Conn_Info *linfo) {
	if(!linfo) {
		POST_ERROR(Err_Handler::EH_NPFA);
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

void Connector::release_tone_generator(Conn_Info *info) {
	if(!info) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	if(info->tone_plant_descriptor != -1) {
		Tone_plant.stop(info->tone_plant_descriptor);
		/* Only disconnect if the proper resource was allocated */
		if(info->jinfo.connections.tone_plant.resource == XPS_Logical::RSRC_TONE_PLANT) {
			Xps_logical.disconnect_tone_plant_output(&info->jinfo);
		}
		else if(info->jinfo.connections.tone_plant.resource != XPS_Logical::RSRC_NONE) {
			POST_ERROR(Err_Handler::EH_IRT);
		}
		Tone_plant.channel_release(info->tone_plant_descriptor);
		info->tone_plant_descriptor = -1;


	}
}


/*
 * Seize and connect a tone generator.
 * Return true if successful, or false if no generator is available.
 *
 * Note: Will disconnect and reconnect if a tone plant was previously connected.
 */
bool Connector::seize_and_connect_tone_generator(Conn_Info *info, bool orig_term) {
	/*
	 * If there is a valid descriptor, then the
	 * previous connection may have not been connected to the correct end of the call
	 * release it here.
	 */
	if(!info) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	Conn.release_tone_generator(info);

	info->tone_plant_descriptor = Tone_plant.channel_seize();
	if(info->tone_plant_descriptor == -1) {
		return false;
	}
	Xps_logical.connect_tone_plant_output(&info->jinfo, info->tone_plant_descriptor, orig_term);
	return true;
}

/*
 * Send dial tone and any audio sample which proceeds it if so configured.
 */

void Connector::send_dial_tone(int32_t descriptor) {

	if(Tone_plant.get_audio_buffer("receiver_lifted")) {
		Tone_plant.send_audio_sequence(descriptor, _receiver_lifted_sequence);
	}
	else {
		Tone_plant.send_call_progress_tones(descriptor, Tone_Plant::CPT_DIAL_TONE);
	}


}


/*
 * Return true if we need to send a sound sample for digits recognized
 */


const char *Connector::get_digits_recognized_buffer_name(void) {


	const char *progress_tone_name = Config_rw.get_progress_tone_buffer_name(Config_RW::PT_DIGITS_RECOGNIZED);
	return progress_tone_name;

}

/*
 * Send ringing call progress tones
 */


void Connector::send_ringing(Conn_Info *info) {
	const char *progress_tone_name = Config_rw.get_progress_tone_buffer_name(Config_RW::PT_RINGING);
	if(progress_tone_name) {
		LOG_DEBUG(TAG, "Sending sampled ring tone");
		Tone_plant.send_buffer_loop_ulaw(info->tone_plant_descriptor, progress_tone_name, 0.0);
	}
	else {
		LOG_DEBUG(TAG, "Sending precise ring tone");
		Tone_plant.send_call_progress_tones(info->tone_plant_descriptor, Tone_Plant::CPT_RINGING);
	}
}

/*
 * Send busy progress tones
 */


void Connector::send_busy(Conn_Info *info) {


	if(Tone_plant.get_audio_buffer("called_party_busy")) {
		Tone_plant.send_buffer_loop_ulaw(info->tone_plant_descriptor,"called_party_busy");
	}
	else {
		Tone_plant.send_call_progress_tones(info->tone_plant_descriptor, Tone_Plant::CPT_BUSY);
	}
}

/*
 * Send congestion tones
 */


void Connector::send_congestion(Conn_Info *info) {

	if(Tone_plant.get_audio_buffer("congestion")) {
		Tone_plant.send_buffer_loop_ulaw(info->tone_plant_descriptor ,"congestion");
	}
	else {
		Tone_plant.send_call_progress_tones(info->tone_plant_descriptor, Tone_Plant::CPT_CONGESTION);
	}

}

/*
 * Release called party
 */

void Connector::release_called_party(Conn_Info *info) {

	uint8_t equip_type = this->get_called_equip_type(info);
	uint8_t dest_phys_line_trunk_number = this->get_called_phys_line_trunk(info);
	uint8_t message;

	switch(equip_type) {
		case ET_TRUNK:
		case ET_LINE:
			message = PM_RELEASE;
			break;

		default:
			POST_ERROR(Err_Handler::EH_UET);
			break;
		}
		Conn.send_peer_message(info, equip_type, dest_phys_line_trunk_number, message);
}


} /* End namespace connector */

Connector::Connector Conn;
