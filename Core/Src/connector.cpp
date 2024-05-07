#include <string.h>
#include "top.h"
#include "logging.h"
#include "util.h"
#include "xps_logical.h"
#include "tone_plant.h"
#include "mf_receiver.h"
#include "drv_dtmf.h"
#include "sub_line.h"
#include "trunk.h"



namespace Connector {

const char *TAG = "connector";

/* Todo: Can't be hard coded in real implementation. Real implementation needs to be a linked list in RAM */

static const Route_Table_Entry route_table_entries[] = {
{"2980406", ET_LINE, 0, 0, 1, {6}},
{"2980407", ET_LINE, 0, 0, 1, {7}},
{"2980400", ET_LINE, 0, 0, 1, {0}},
{"2980401", ET_LINE, 0, 0, 1, {1}},
{"2980402", ET_LINE, 0, 0, 1, {2}},
{"2980403", ET_LINE, 0, 0, 1, {3}},
{"2980404", ET_LINE, 0, 0, 1, {4}},
{"2980405", ET_LINE, 0, 0, 1, {5}},

{"", ET_UNDEF, 0, 0, 0, {0}}, /* Marks the end of the route table */


};

/*
 * Return the maximum number of digits in the route table
 */

uint8_t Connector::_calc_max_route_digits(void) {
	uint8_t max_digit_length = 0;

	for(int index = 0; route_table_entries[index].match_string[0]; index++) {
		int len = strlen(route_table_entries[index].match_string);
		if(len > max_digit_length) {
			max_digit_length = len;
		}
	}

	return max_digit_length;
}

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

	this->_max_route_length =  this->_calc_max_route_digits();

}



/*
 * Prepare for a connection
 */

void Connector::prepare(Conn_Info *conn_info, uint32_t source_equip_type, uint32_t source_phys_line_number) {
	if(!conn_info) {
		LOG_PANIC(TAG, "Null pointer passed in");
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
		LOG_PANIC(TAG, "Null pointer passed in");
	}
	Route_Info *ri = &conn_info->route_info;

	if(!(strlen(digits_received) > this->_max_route_length)) {
		uint32_t index;
		uint32_t rte_res = ROUTE_INDETERMINATE;
		ri->route_table_entry = NULL;
		/* Loop through the routing table */
		for(index = 0; route_table_entries[index].match_string[0]; index++) {
			const Route_Table_Entry *rte = &route_table_entries[index];
			rte_res = this->_test_against_route(digits_received, rte->match_string);
			if(rte_res == ROUTE_VALID) {
				/* Note the matching route table entry */
				ri->route_table_entry = rte;
				/* Note the dialed digits matching the route */
				Utility.strncpy_term(ri->dialed_number, digits_received, MAX_DIALED_DIGITS + 1 );
				res = ROUTE_VALID;
				break;
			}
		}
		/* If we searched all routes and didn't find a match, then the dialed digits aren't valid */
		if(rte_res == ROUTE_INVALID) {
			res = ROUTE_INVALID;
		}
	}
	else {
		/* Dialed digits exceed longest entry in the routing table */
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
		LOG_PANIC(TAG, "Null pointer passed in");
	}

	Route_Info *ri = &conn_info->route_info;

	if(ri->state != ROUTE_VALID) {
		return (ri->state = ROUTE_INVALID);
	}
	uint32_t res = ROUTE_DEST_CONNECTED;

	/* Copy the information from the route table entry to the route info data */
	conn_info->route_info.dest_equip_type = conn_info->route_info.route_table_entry->dest_equip_type;
	conn_info->route_info.dest_line_trunk_count = conn_info->route_info.route_table_entry->phys_line_trunk_count;
	memcpy(conn_info->route_info.dest_phys_lines_trunks,
			conn_info->route_info.route_table_entry->phys_lines_trunks,
			MAX_PHYS_LINE_TRUNK_TABLE);

	/* Send a seize message to the destination */
	/* Todo Note: This doesn't handle groups of trunks yet. */
	uint32_t pm_res = this->send_peer_message(
			conn_info,
			conn_info->route_info.dest_equip_type,
			conn_info->route_info.dest_phys_lines_trunks[0],
			PM_SEIZE);
	switch(pm_res) {
	case PMR_OK:
		res = ROUTE_DEST_CONNECTED;
		break;

	case PMR_BUSY:
		res = ROUTE_DEST_BUSY;
		break;

	default:
		LOG_PANIC(TAG, "Bad return value");
		break;
	}

	ri->state = res;
	return res;
}

/* This function sends a message to the other end of the connection */
/* For use by lines and trunks only in the event process. Does not respect locking */

uint32_t Connector::send_peer_message(Conn_Info *conn_info, uint32_t dest_equip_type,
		uint32_t dest_phys_line_trunk_number, uint32_t message) {

	if(!conn_info) {
		LOG_PANIC(TAG, "Null pointer passed in");
	}

	uint32_t pm_res;

	switch(dest_equip_type) {
	case ET_LINE:
		if(dest_phys_line_trunk_number >= Sub_Line::MAX_DUAL_LINE_CARDS * 2) {
			LOG_PANIC(TAG, "Bad parameter value");
		}
		pm_res = Sub_line.peer_message_handler(conn_info, dest_phys_line_trunk_number, message);
		break;

	case ET_TRUNK:
		if(dest_phys_line_trunk_number >= Trunk::MAX_TRUNK_CARDS) {
			LOG_PANIC(TAG, "Bad parameter value");
		}
		pm_res = Trunks.peer_message_handler(conn_info, dest_phys_line_trunk_number, message);
		break;

	default:
		LOG_PANIC(TAG, "Invalid equipment type");
		break;
	}

	return pm_res;

}

/*
 * Return the equipment type of the caller
 */
uint32_t Connector::get_caller_equip_type(Conn_Info *conn_info) {
	if(!conn_info) {
		LOG_PANIC(TAG, "Null pointer passed in");
	}
	return conn_info->route_info.source_equip_type;

}

/*
 * Return the equipment type of the called party
 */

uint32_t Connector::get_called_equip_type(Conn_Info *conn_info) {
	if(!conn_info) {
		LOG_PANIC(TAG, "Null pointer passed in");
	}
	return conn_info->route_info.dest_equip_type;

}

/*
 * Return the caller's physical line or trunk number
 */

uint32_t Connector::get_caller_phys_line_trunk(Conn_Info *conn_info) {
	if(!conn_info) {
		LOG_PANIC(TAG, "Null pointer passed in");
	}
	return conn_info->route_info.source_phys_line_number;
}


/*
 * Return the called party's physical line or trunk number
 */

uint32_t Connector::get_called_phys_line_trunk(Conn_Info *conn_info) {
	if(!conn_info) {
		LOG_PANIC(TAG, "Null pointer passed in");
	}
	/* Todo: does not support multiple trunks */
	return conn_info->route_info.dest_phys_lines_trunks[0];
}


/*
 * Connect called party to junctor
 */

void Connector::connect_called_party_audio(Conn_Info *linfo) {
	if(!linfo) {
			LOG_PANIC(TAG, "Null pointer passed in");
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
			LOG_PANIC(TAG, "Null pointer passed in");
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
			LOG_PANIC(TAG, "Null pointer passed in");
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
			LOG_PANIC(TAG, "Null pointer passed in");
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
		LOG_PANIC(TAG, "Null pointer passed in");
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

void Connector::release_tone_generator(Conn_Info *linfo) {
	if(!linfo) {
		LOG_PANIC(TAG, "Null pointer passed in");
	}
	if(linfo->tone_plant_descriptor != -1) {
		Xps_logical.disconnect_tone_plant_output(&linfo->jinfo);
		Tone_plant.channel_release(linfo->tone_plant_descriptor);
		linfo->tone_plant_descriptor = -1;


	}
}






} /* End namespace connector */

Connector::Connector Conn;
