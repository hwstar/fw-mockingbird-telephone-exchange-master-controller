#include <string.h>
#include "top.h"
#include "logging.h"
#include "util.h"
#include "xps_logical.h"
#include "sub_line.h"
#include "trunk.h"



namespace Connector {

const char *TAG = "connector";

/* Todo: Can't be hard coded in real implementation. Real implementation needs to be a linked list in RAM */

static const Route_Table_Entry route_table_entries[] = {
{"2980400", DT_LINE, 0, 0, 1, {6}},
{"2980401", DT_LINE, 0, 0, 1, {7}},
{"", DT_LINE, 0, 0, 0, {0}}, /* Marks the end of the route table */


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
	conn_info->route_info.dest_equip_type = DT_UNDEF;
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
 * Connect an originating call to the desired termination.
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


uint32_t Connector::connect(Conn_Info *conn_info) {

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


	ri->state = res;
	return res;
}

/* This function sends a message to the other end of the connection */
/* For use by lines and trunks only in the event process. Does not respect locking */

uint32_t Connector::send_peer_message(Conn_Info *conn_info, uint8_t message) {

	return 0;

}


} /* End namespace connector */

Connector::Connector Conn;
