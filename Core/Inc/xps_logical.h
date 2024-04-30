#pragma once
#include "top.h"
#include "drv_xps.h"
#include "tone_plant.h"
#include "mf_receiver.h"
#include "drv_dtmf.h"


namespace XPS_Logical {

const uint8_t MATRIX_DEPTH = 2;
const uint8_t PHYSICAL_NUM_X = Xps::MAX_ROWS;
const uint8_t PHYSICAL_NUM_Y = Xps::MAX_COLUMNS;
const uint8_t LOGICAL_NUM_X = (PHYSICAL_NUM_X * MATRIX_DEPTH);
const uint8_t LOGICAL_NUM_Y = PHYSICAL_NUM_Y;
const uint8_t LOGICAL_MAX_X = (PHYSICAL_NUM_X * MATRIX_DEPTH) - 1;
const uint8_t LOGICAL_MAX_Y = PHYSICAL_NUM_Y - 1;

const uint8_t MAX_JUNCTORS = 4;

const uint8_t MAX_SUB_LINES = 8;
const uint8_t MAX_TRUNKS = 3;

const uint8_t PATH_ORIG_TO_TERM = 0; /* Even Y value */
const uint8_t PATH_TERM_TO_ORIG = 1; /* Odd Y value */

/* These have x values which increment by one */
const uint8_t LINE_COLUMN_START = 0;
const uint8_t TRUNK_COLUMN_START = 16;

/* These have x values which increment by two */
const uint8_t TONE_PLANT_COLUMN_START = 24;
const uint8_t DTMF_RECEIVER_COLUMN_START = 25;
const uint8_t MF_RECEIVER_COLUMN_START = 29;



enum {RSRC_NONE=0, RSRC_LINE, RSRC_TRUNK, RSRC_MF_RCVR, RSRC_DTMF_RCVR, RSRC_TONE_PLANT};


typedef struct Phys_Switch {
	uint8_t chip;
	uint8_t x;
	uint8_t y;
} Phys_Switch;

typedef struct Connection {
	uint8_t x;
	uint8_t y;
	uint8_t resource;
} Connection;


typedef struct Connections {
	Connection orig_send;
	Connection orig_recv;
	Connection term_send;
	Connection term_recv;
	Connection tone_plant;
	Connection digit_receiver;
} Connections;

typedef struct Junctor_Info {
	int32_t junctor_descriptor;
	Connections connections;
} Junctor_Info;

class XPS_Logical {
public:
	void init(void);

	/* High level logical methods */
	bool seize(Junctor_Info *info, int32_t requested_junctor_number = -1);
	void release(Junctor_Info *info);
	void connect_phone_orig(Junctor_Info *info, int32_t phone_line_num_orig);
	void disconnect_phone_orig(Junctor_Info *info);
	void connect_phone_term(Junctor_Info *info, int32_t phone_line_num_term);
	void disconnect_phone_term(Junctor_Info *info);
	void connect_trunk_orig(Junctor_Info *info, int32_t trunk_num_orig);
	void disconnect_trunk_orig(Junctor_Info *info);
	void connect_trunk_term(Junctor_Info *info, int32_t trunk_num_term);
	void disconnect_trunk_term(Junctor_Info *info);
	void connect_tone_plant_output(Junctor_Info *info, int32_t tone_plant_descriptor, bool orig_term = true);
	void disconnect_tone_plant_output(Junctor_Info *info);
	void connect_dtmf_receiver(Junctor_Info *info, int32_t dtmf_receiver_descriptor, bool orig_term = true);
	void disconnect_dtmf_receiver(Junctor_Info *info);
	void connect_mf_receiver(Junctor_Info *info, int32_t mf_receiver_descriptor, bool orig_term = true);
	void disconnect_mf_receiver(Junctor_Info *info);
	void disconnect_all(Junctor_Info *info);
	/* Low level logical methods (mainly used for testing) */
	void close_switch(uint32_t x, uint32_t y);
	void open_switch(uint32_t x, uint32_t y);
	void clear(); /* Clears the entire switch matrix */
	bool get_switch_state(uint32_t x, uint32_t y);



protected:
	bool _validate_descriptor(uint32_t descriptor);
	uint8_t get_mf_receiver_x(int32_t mf_descriptor);
	uint8_t get_dtmf_receiver_x(int32_t dtmf_descriptor);
	uint8_t get_tone_plant_x(int32_t tone_plant_descriptor);
	uint8_t get_sub_line_x(uint8_t line_number);
	uint8_t get_trunk_x(uint8_t trunk_number);
	uint8_t get_path_y(uint8_t junctor_number, bool orig_term = true);
	void _logical_to_physical(Phys_Switch *s, uint32_t x, uint32_t y);


	osMutexId_t _lock;
	uint32_t _busy_bits;
	uint8_t _matrix_state[(MATRIX_DEPTH * PHYSICAL_NUM_X * PHYSICAL_NUM_Y)/8];

};



} /* End namespace XPS_Logical */

/* Class declaration */
extern XPS_Logical::XPS_Logical Xps_logical;
