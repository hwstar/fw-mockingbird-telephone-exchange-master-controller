#pragma once
#include "top.h"
#include "xps_logical.h"

namespace Connector {
const uint8_t MAX_DIALED_DIGITS = 15;
const uint8_t MAX_PHYS_LINE_TRUNK_TABLE = 3;


enum {ROUTE_INDETERMINATE = 0, ROUTE_VALID, ROUTE_INVALID, ROUTE_DEST_CONNECTED, ROUTE_DEST_BUSY, ROUTE_DEST_CONGESTED};
enum {DT_UNDEF=0, DT_LINE, DT_TRUNK};


typedef void (*Conn_Handler_Type)(uint32_t event, uint32_t equip_type, uint32_t phys_line_trunk_num);


typedef struct Route_Table_Entry {
	char match_string[MAX_DIALED_DIGITS + 1];
	uint8_t dest_equip_type;
	uint8_t trunk_addressing_start;
	uint8_t trunk_addressing_end;
	uint8_t phys_line_trunk_count;
	uint8_t phys_lines_trunks[MAX_PHYS_LINE_TRUNK_TABLE];
} Route_Table_Entry;


typedef struct Route_Info {
	uint8_t state;
	uint8_t source_equip_type;
	uint8_t source_phys_line_number;
	uint8_t dest_equip_type;
	uint8_t dest_line_trunk_count;
	uint8_t dest_phys_lines_trunks[MAX_PHYS_LINE_TRUNK_TABLE];
	char dialed_number[MAX_DIALED_DIGITS + 1];
	const Route_Table_Entry *route_table_entry;


} Route_Info;


typedef struct Conn_Info {
	uint8_t state;
	bool junctor_seized;
	int16_t tone_plant_descriptor;
	int16_t mf_receiver_descriptor;
	int16_t dtmf_receiver_descriptor;
	XPS_Logical::Junctor_Info jinfo;
	uint8_t num_dialed_digits;
	uint8_t prev_num_dialed_digits;
	char digit_buffer[MAX_DIALED_DIGITS + 1];
	Route_Info route_info;
	struct Conn_Info *peer;
}Conn_Info;


class Connector {
protected:
	uint8_t _max_route_length;
	uint32_t _test_against_route(const char *string_to_test, const char *route_table_entry);
	uint8_t _calc_max_route_digits(void);
public:
	void init(void);
	void prepare(Conn_Info *conn_info, uint32_t source_equip_type, uint32_t source_phys_line_number);
	uint32_t test(Conn_Info *conn_info, const char *digits_received);
	uint32_t connect(Conn_Info *conn_info);
	/* For use by lines and trunks only in the event process. Does not respect locking */
	uint32_t send_peer_message(Conn_Info *conn_info, uint8_t message);

};

} /* End namespace Connector */

extern Connector::Connector Conn;
