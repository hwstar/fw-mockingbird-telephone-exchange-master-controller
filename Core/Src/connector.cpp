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

/* Todo: Can't be hard coded in real implementation */

static const Route_Table_Entry line_route_table_entries[] = {
{"2980400", ET_LINE, 0, 0, 1, {0}},
{"2980401", ET_LINE, 0, 0, 1, {1}},
{"2980402", ET_LINE, 0, 0, 1, {2}},
{"2980403", ET_LINE, 0, 0, 1, {3}},
{"2980404", ET_LINE, 0, 0, 1, {4}},
{"2980405", ET_LINE, 0, 0, 1, {5}},
{"2980406", ET_LINE, 0, 0, 1, {6}},
{"2980407", ET_LINE, 0, 0, 1, {7}},
{"_298XXXX", ET_TRUNK,0, 0, 2, {2,1}},

{"", ET_UNDEF, 0, 0, 0, {0}}, /* Marks the end of the route table */

};


static const Route_Table_Entry incoming_trunk_route_table_entries[] = {
{"80400", ET_LINE, 0, 0, 1, {0}},
{"80401", ET_LINE, 0, 0, 1, {1}},
{"80402", ET_LINE, 0, 0, 1, {2}},
{"80403", ET_LINE, 0, 0, 1, {3}},
{"80404", ET_LINE, 0, 0, 1, {4}},
{"80405", ET_LINE, 0, 0, 1, {5}},
{"80406", ET_LINE, 0, 0, 1, {6}},
{"80407", ET_LINE, 0, 0, 1, {7}},

{"", ET_UNDEF, 0, 0, 0, {0}}, /* Marks the end of the route table */

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


	 /* Initialize the route table memory pool */
	this->_routing_pool.pool_init(this->_routing_pool_memory, sizeof(Route_Table_Node), MAX_ROUTE_NODES);

	/*
	 * Temporary code to get things initialized
	 * Todo: Remove
	 */

	/* Create the line routing table */
	for(int index = 0; line_route_table_entries[index].match_string[0]; index++) {
		/* Add the route */
		this->add_route(0, line_route_table_entries[index].dest_equip_type,
			line_route_table_entries[index].phys_line_trunk_count,
			line_route_table_entries[index].trunk_addressing_start,
			line_route_table_entries[index].trunk_addressing_end,
			line_route_table_entries[index].phys_lines_trunks,
			line_route_table_entries[index].match_string
			);
	}

	/* Create the incoming trunk routing table */
	for(int index = 0; incoming_trunk_route_table_entries[index].match_string[0]; index++) {
		/* Add the route */
		this->add_route(1, incoming_trunk_route_table_entries[index].dest_equip_type,
			incoming_trunk_route_table_entries[index].phys_line_trunk_count,
			line_route_table_entries[index].trunk_addressing_start,
			line_route_table_entries[index].trunk_addressing_end,
			incoming_trunk_route_table_entries[index].phys_lines_trunks,
			incoming_trunk_route_table_entries[index].match_string
			);
	}
}

/*
 * Called when the system is about to go on line
 *
 * Read configuration data and set up configuration here
 */

void Connector::config() {

}

/*
 * Add a route to a routing table
 */
bool Connector::add_route(uint32_t table_number, uint32_t dest_equip_type, uint32_t phys_line_trunk_count,
		uint32_t trunk_addressing_start, uint32_t trunk_addressing_end, const uint8_t *dest_phys_lines_trunks, const char *match_string ) {

	if(table_number >= MAX_ROUTE_TABLES) {
		POST_ERROR(Err_Handler::EH_INVP);
	}
	if((dest_phys_lines_trunks == NULL)||(match_string == NULL)) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	Route_Table_Node *rtn_new = (Route_Table_Node *) this->_routing_pool.allocate_object();

	if(!rtn_new) {
		LOG_ERROR(TAG, "Route table full");
		return false;
	}

	/* Copy the route data */
	rtn_new->dest_equip_type = dest_equip_type;
	rtn_new->phys_line_trunk_count = phys_line_trunk_count;
	rtn_new->trunk_addressing_start = trunk_addressing_start;
	rtn_new->trunk_addressing_end = trunk_addressing_end;
	memcpy(rtn_new->phys_lines_trunks, dest_phys_lines_trunks, MAX_PHYS_LINE_TRUNK_TABLE);
	Utility.strncpy_term(rtn_new->match_string, match_string, MAX_DIALED_DIGITS + 1);

	/* Set up the list pointers */
	if(!this->_route_table_heads[table_number]) {
		/* First insert */
		this->_route_table_heads[table_number] = rtn_new;
		this->_route_table_tails[table_number] = rtn_new;
		rtn_new->prev = NULL;
		rtn_new->next = NULL;

	}
	else {
		/* Subsequent insert */
		this->_route_table_tails[table_number]->next = rtn_new;
		rtn_new->prev = this->_route_table_tails[table_number];
		rtn_new->next = NULL;
		this->_route_table_tails[table_number] = rtn_new;

	}

	return true;
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
	conn_info->route_info.state = ROUTE_INDETERMINATE;
	conn_info->route_info.source_equip_type = source_equip_type;
	conn_info->route_info.source_phys_line_number = source_phys_line_number;
	conn_info->route_info.dest_equip_type = ET_UNDEF;
	conn_info->route_info.dest_line_trunk_count = 0;
}


/*
 * Called by line or trunk objects to test a route
 *
 * Returns a result code which must be checked.
 */


uint32_t Connector::test(Conn_Info *conn_info, const char *digits_received) {
	uint32_t res = ROUTE_INDETERMINATE;

	if((conn_info == NULL) || (digits_received == NULL)) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	Route_Info *ri = &conn_info->route_info;

	uint32_t rtn_res = ROUTE_INDETERMINATE;
	Route_Table_Node *rtn;
	ri->route_table_node = NULL;
	/* Traverse through the routing table */
	rtn = this->_route_table_heads[conn_info->route_table_number];
	if(rtn == NULL) {
		return ROUTE_INVALID;
	}
	while(rtn) {
		rtn_res = this->_test_against_route(digits_received, rtn->match_string);
		if(rtn_res == ROUTE_VALID) {
			/* Note the matching route table node */
			ri->route_table_node = rtn;
			/* Note the dialed digits matching the route */
			Utility.strncpy_term(ri->dialed_number, digits_received, MAX_DIALED_DIGITS + 1 );
			res = ROUTE_VALID;
			break;
		}
		rtn = rtn->next;
	}
	/* If we searched all routes and didn't find a match, then the dialed digits aren't valid */
	if(rtn_res == ROUTE_INVALID) {
		res = ROUTE_INVALID;
	}

	ri->state = res;
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

	/* Copy the information from the route table entry to the route info data */
	conn_info->route_info.dest_equip_type = conn_info->route_info.route_table_node->dest_equip_type;
	conn_info->route_info.dest_line_trunk_count = conn_info->route_info.route_table_node->phys_line_trunk_count;
	memcpy(conn_info->route_info.dest_phys_lines_trunks,
			conn_info->route_info.route_table_node->phys_lines_trunks,
			MAX_PHYS_LINE_TRUNK_TABLE);

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
	Tone_plant.send_call_progress_tones(info->tone_plant_descriptor, Tone_Plant::CPT_BUSY);
}


/*
 * Send congestion tones
 */


void Connector::send_congestion(Conn_Info *info) {
	Tone_plant.send_call_progress_tones(info->tone_plant_descriptor, Tone_Plant::CPT_CONGESTION);
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
