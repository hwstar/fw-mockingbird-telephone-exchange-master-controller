#pragma once
#include "top.h"
#include "xps_logical.h"
#include "pool_alloc.h"
#include "config_rw.h"

namespace Connector {
const uint8_t MAX_DIALED_DIGITS = 15;
const uint8_t MAX_PHYS_LINE_TRUNK_TABLE = 3;
const uint8_t MAX_TRUNK_OUTGOING_ADDRESS = 17;
const uint32_t MAX_ROUTE_NODES = 128;
const uint32_t MAX_ROUTE_TABLES = 8;


/* Route and connection return values */
enum {ROUTE_INDETERMINATE = 0, ROUTE_VALID=1, ROUTE_INVALID=2, ROUTE_DEST_CONNECTED=3, ROUTE_DEST_BUSY=4, ROUTE_DEST_TRUNK_BUSY=5, ROUTE_NO_MORE_TRUNKS=6};
/* Equipment types */
enum {ET_UNDEF=0, ET_LINE, ET_TRUNK};
/* Peer messages */
enum {PM_NOP=0,PM_SEIZE=1, PM_RELEASE=2, PM_ANSWERED=3, PM_CALLED_PARTY_HUNGUP=4, PM_TRUNK_BUSY=5,
	PM_TRUNK_NO_WINK=6, PM_TRUNK_READY_FOR_ADDR_INFO=7, PM_TRUNK_ADDR_INFO_READY=8,
	PM_TRUNK_READY_TO_CONNECT_CALLER=10};
/* Peer message return values */
enum {PMR_NOP=0, PMR_OK=1, PMR_BUSY=2, PMR_TRUNK_BUSY=3};

typedef void (*Conn_Handler_Type)(uint32_t event, uint32_t equip_type, uint32_t phys_line_trunk_num);


typedef struct Route_Info {
	uint8_t state;
	uint8_t source_equip_type;
	uint8_t source_phys_line_number;
	uint8_t dest_dial_start_index;
	uint8_t dest_equip_type;
	uint8_t dest_line_trunk_count;
	uint8_t dest_phys_lines_trunks[MAX_PHYS_LINE_TRUNK_TABLE];
	char dialed_number[MAX_DIALED_DIGITS + 1];
	char *trunk_prefix;
	Config_RW::Config_Node_Type *rt_head;
	Config_RW::Config_Section_Type *dest_section;

} Route_Info;


typedef struct Conn_Info {
	uint8_t phys_line_trunk_number;
	uint8_t trunk_index;
	uint8_t equip_type;
	uint8_t state;
	uint8_t pending_state;
	bool called_party_hangup;
	bool junctor_seized;
	int16_t tone_plant_descriptor;
	int16_t mf_receiver_descriptor;
	int16_t dtmf_receiver_descriptor;
	XPS_Logical::Junctor_Info jinfo;
	uint8_t num_dialed_digits;
	uint8_t prev_num_dialed_digits;
	char digit_buffer[MAX_DIALED_DIGITS + 1];
	char trunk_outgoing_address[MAX_TRUNK_OUTGOING_ADDRESS + 1];
	Route_Info route_info;
	struct Conn_Info *peer;
	osTimerId_t dial_timer;
}Conn_Info;


class Connector {
protected:
	uint32_t _test_against_route(const char *string_to_test, const char *route_table_entry);
	Pool_Alloc::Pool_Alloc _routing_pool;
public:

	void init(void);
	void config();
	void prepare(Conn_Info *conn_info, uint32_t source_equip_type, uint32_t source_phys_line_number);
	uint32_t test(Conn_Info *conn_info, const char *dialed_digits);
	uint32_t resolve(Conn_Info *conn_info);
	uint32_t resolve_try_next_trunk(Conn_Info *conn_info);

	/* Peer-to-peer messaging */
	/* For use by lines and trunks only in the event process. Does not respect locking */
	uint32_t send_message_to_dest(Conn_Info *conn_info, uint32_t message);
	/* 2 signatures */
	uint32_t send_peer_message(Conn_Info *conn_info, uint32_t message);
	uint32_t send_peer_message(Conn_Info *conn_info, uint32_t dest_equip_type,
			uint32_t dest_phys_line_trunk_number, uint32_t message);

	uint32_t get_caller_equip_type(Conn_Info *conn_info);
	uint32_t get_caller_phys_line_trunk(Conn_Info *conn_info);
	uint32_t get_called_equip_type(Conn_Info *conn_info);
	uint32_t get_called_phys_line_trunk(Conn_Info *conn_info);
	void connect_called_party_audio(Conn_Info *linfo);
	void disconnect_called_party_audio(Conn_Info *linfo);
	void connect_caller_party_audio(Conn_Info *linfo);
	void disconnect_caller_party_audio(Conn_Info *linfo);
	void release_mf_receiver(Conn_Info *linfo);
	void release_dtmf_receiver(Conn_Info *linfo);
	void release_tone_generator(Conn_Info *info);
	bool seize_and_connect_tone_generator(Conn_Info *info, bool orig_term=true);
	void send_ringing(Conn_Info *info);
	void send_busy(Conn_Info *info);
	void send_congestion(Conn_Info *info);
	void release_called_party(Conn_Info *info);
	const char *get_digits_recognized_buffer_name(void);
	void send_dial_tone(int32_t descriptor);


};

} /* End namespace Connector */

extern Connector::Connector Conn;
