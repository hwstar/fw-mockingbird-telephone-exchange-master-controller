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

const uint8_t NUM_MF_RECEIVERS = MF_Decoder::NUM_MF_RECEIVERS;
const uint8_t NUM_DTMF_RECEIVERS = Dtmf::NUM_DTMF_RECEIVERS;
const uint8_t NUM_TONE_OUTPUTS = Tone_Plant::NUM_TONE_OUTPUTS;


/*
 * Switch matrix X locations
 */

/* These have x values which increment by one */
const uint8_t LINE_ROW_START = 0;
const uint8_t TRUNK_ROW_START = 16;

/* These have x values which increment by two */
const uint8_t TONE_PLANT_ROW_START = 24;
const uint8_t DTMF_RECEIVER_ROW_START = 25;
const uint8_t MF_RECEIVER_ROW_START = 29;


typedef struct Phys_Switch {
	uint8_t chip;
	uint8_t x;
	uint8_t y;
} Phys_Switch;

typedef struct Junctor_Setup {
	uint32_t resource_type;
	uint32_t resource_descriptor;
	uint32_t x;
	uint32_t y;
}Junctor_Setup;

typedef struct Junctor_Info {
	int32_t junctor_descriptor;
	Junctor_Setup setup_orig;
	Junctor_Setup setup_term;
} Junctor_Info;

class XPS_Logical {
public:
	void init(void);

	/* High level logical methods */
	bool junctor_seize(Junctor_Info *info, int32_t requested_junctor_number = -1);
	void junctor_release(Junctor_Info *info);
	bool junctor_connect_orig(Junctor_Info *info, int32_t phone_line_num_orig);
	bool junctor_disconnect_orig(Junctor_Info *info);
	bool junctor_connect_term(Junctor_Info *info, int32_t phone_line_num_term);
	bool junctor_disconnect_term(Junctor_Info *info);
	bool junctor_disconnect_all(Junctor_Info *info);
	/* Low level logical methods */
	void close_switch(uint32_t x, uint32_t y);
	void open_switch(uint32_t x, uint32_t y);
	bool clear();
	bool get_switch_state(uint32_t x, uint32_t y);



protected:
	bool _validate_descriptor(uint32_t descriptor);
	void _logical_to_physical(Phys_Switch *s, uint32_t x, uint32_t y);


	osMutexId_t _lock;
	uint32_t _busy_bits;
	uint8_t _max_junctors;
	uint8_t _installed_lines;
	uint8_t _installed_trunks;
	uint8_t _matrix_state[(MATRIX_DEPTH * PHYSICAL_NUM_X * PHYSICAL_NUM_Y)/8];

};



} /* End namespace XPS_Logical */

/* Class declaration */
extern XPS_Logical::XPS_Logical Xps_logical;
