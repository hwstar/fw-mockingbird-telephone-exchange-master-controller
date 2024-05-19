/*
 * console.c
 *
 *  Created on: Feb 21, 2024
 *      Author: srodgers
 */

#include "top.h"
#include "util.h"
#include "console.h"
#include "logging.h"
#include "err_handler.h"
#include "uart.h"
#include "xps_logical.h"
#include "tone_plant.h"
#include "mf_receiver.h"
#include "drv_dtmf.h"
#include "sub_line.h"
#include "trunk.h"
#include "hw_pres.h"

const char *TAG = "console";

Console::Console System_console;






namespace Console {




static bool command_xps_open(Holder_Type *vars, uint32_t *error_code);
static bool command_xps_close(Holder_Type *vars, uint32_t *error_code);
static bool command_xps_status(Holder_Type *vars, uint32_t *error_code);
static bool command_xps_clear(Holder_Type *vars, uint32_t *error_code);
static bool command_tg_seize(Holder_Type *vars, uint32_t *error_code);
static bool command_tg_release(Holder_Type *vars, uint32_t *error_code);
static bool command_tg_milliwatt(Holder_Type *vars, uint32_t *error_code);
static bool command_tg_stop(Holder_Type *vars, uint32_t *error_code);
static bool command_tg_dialtone(Holder_Type *vars, uint32_t *error_code);
static bool command_tg_tone(Holder_Type *vars, uint32_t *error_code);
static bool command_mfr_seize(Holder_Type *vars, uint32_t *error_code);
static bool command_mfr_release(Holder_Type *vars, uint32_t *error_code);
static bool command_dtmfr_seize(Holder_Type *vars, uint32_t *error_code);
static bool command_dtmfr_release(Holder_Type *vars, uint32_t *error_code);
static bool command_config_hw_view_present(Holder_Type *vars, uint32_t *error_code);
static bool command_help(Holder_Type *vars, uint32_t *error_code);
static bool command_trunk_offline(Holder_Type *vars, uint32_t *error_code);
static bool command_trunk_online(Holder_Type *vars, uint32_t *error_code);
static bool command_trunk_busy(Holder_Type *vars, uint32_t *error_code);



const uint8_t trunk_commands_arg_type[] = {AT_UINT, AT_END};
const Command_Table_Entry_Type test_trunk_commands[] = {
	{NULL, command_trunk_offline, trunk_commands_arg_type, "offline"},
	{NULL, command_trunk_online, trunk_commands_arg_type, "online"},
	{NULL, command_trunk_busy, trunk_commands_arg_type, "busy"},

	{NULL, NULL, NULL, ""}
};



const uint8_t dtmfr_seize_arg_type[] = {AT_UINT, AT_END};
const Command_Table_Entry_Type test_xps_dtmfr_level[] = {
	{NULL, command_dtmfr_release, NULL, "release"},
	{NULL, command_dtmfr_seize, dtmfr_seize_arg_type, "seize"},

	{NULL, NULL, NULL, ""}

};

const uint8_t mfr_seize_arg_type[] = {AT_UINT, AT_END};
const Command_Table_Entry_Type test_xps_mfr_level[] = {
	{NULL, command_mfr_release, NULL, "release"},
	{NULL, command_mfr_seize, mfr_seize_arg_type, "seize"},

	{NULL, NULL, NULL, ""}

};


const uint8_t tg_tone_arg_type[] = {AT_FL, AT_FL, AT_END};
const uint8_t tg_seize_arg_type[] = {AT_UINT, AT_END};
const Command_Table_Entry_Type test_tg_command_level[] = {
	{NULL, command_tg_dialtone, NULL, "dialtone"},
	{NULL, command_tg_milliwatt, NULL, "milliwatt"},
	{NULL, command_tg_release,NULL, "release"},
	{NULL, command_tg_seize, tg_seize_arg_type, "seize"},
	{NULL, command_tg_stop, NULL, "stop"},
	{NULL, command_tg_tone, tg_tone_arg_type, "tone"},

	{NULL, NULL, NULL, ""}


};

const uint8_t xps_open_close_arg_types[] = {AT_UINT, AT_UINT, AT_END};
const Command_Table_Entry_Type test_xps_open_close_level[] = {
    {NULL, command_xps_clear, NULL, "clear"},
	{NULL, command_xps_open, xps_open_close_arg_types, "open"},
	{NULL, command_xps_close, xps_open_close_arg_types, "close"},
	{NULL, command_xps_status, NULL, "status"},

	{NULL, NULL, NULL, ""}
};

const Command_Table_Entry_Type test_xps_level[] = {
	{test_xps_dtmfr_level, NULL, NULL, "dtmfr"},
	{test_xps_mfr_level, NULL, NULL, "mfr"},
	{test_trunk_commands, NULL, NULL, "trunk"},
	{test_tg_command_level, NULL, NULL, "tg"},
	{test_xps_open_close_level, NULL, NULL, "xps"},

	{NULL, NULL, NULL, ""}
};

const Command_Table_Entry_Type config_view_hw_level[] = {
		{NULL, command_config_hw_view_present, NULL, "present" },

		{NULL, NULL, NULL, ""}
};

const Command_Table_Entry_Type config_view_level[] = {
		{config_view_hw_level, NULL, NULL, "hw"},

		{NULL, NULL, NULL, ""}
};

const Command_Table_Entry_Type config_level[] = {
		{config_view_level, NULL, NULL, "view"},

		{NULL, NULL, NULL, ""}

};

const Command_Table_Entry_Type command_table_top[] = {
		{config_level, NULL, NULL, "config"},
		{NULL, command_help, NULL, "help"},
		{test_xps_level, NULL, NULL, "test"},

		{NULL, NULL, NULL, ""}
};

const char *console_error_strings[] = {
	{"No error"},
	{"Unknown command"},
	{"Incorrect number of parameters"},
	{"Parameter error"},
	{"Parameter out of range"},
	{"Command Table Error"},
	{"Resource in use"},
	{"No resource"},
	{"Resource already allocated"},
	{"Trunk not present, or in use"},

	{"Unknown Error"}
};


/*
 * Command Helper functions
 *
 *
 *
 *
 */

/*
 * Walk the command table and print out the command keywords for each command type
 */

static void _help_walk_command_table(const Command_Table_Entry_Type *cte, char *work_string) {


	char *local_work_string = Utility.allocate_long_string();

	if(!local_work_string) {
				LOG_ERROR(TAG, "Out of memory");
				return;
			}

	while(strlen(cte->keyword)) {
		/* Make new copy of working string passed in */
		strncpy(local_work_string, work_string, Util::LONG_STRINGS_SIZE - 1);
		/* Append the current command keyword */
		strncat(local_work_string, cte->keyword, Util::LONG_STRINGS_SIZE - 1);
		/* Append one space */
		strncat(local_work_string, " ", Util::LONG_STRINGS_SIZE - 1);
		/* If we have a pointer to the next table, recurse to it */
		if(cte->next_table) {
			_help_walk_command_table(cte->next_table, local_work_string);
		}
		/* If we have a command, it means this is an ending node in the table */

		else if(cte->command) {
			if(cte->arguments) {
				const uint8_t *arg = cte->arguments;
				/* Append parameter list */
				while(*arg != AT_END) {
					switch(*arg) {
					case AT_FL:
						strncat(local_work_string, "<float> ", Util::LONG_STRINGS_SIZE - 1);
						break;
					case AT_UINT:
						strncat(local_work_string, "<uint> ", Util::LONG_STRINGS_SIZE - 1);
						break;
					}
					arg++;
				}
			}
			/* At end of command print string */
			printf("%s\n", local_work_string);
		}
		cte++;
	}
	Utility.deallocate_long_string(local_work_string);
}



/*
 *
 *
 * Command functions called by class
 *
 *
 *
 */


static bool command_help(Holder_Type *vars, uint32_t *error_code) {


	char *work_string = Utility.allocate_long_string();

	if(!work_string) {
			LOG_ERROR(TAG, "Out of memory");
			return false;
		}


	/* Walk the command table */
	_help_walk_command_table(command_table_top, work_string);

	Utility.deallocate_long_string(work_string);


	return true;
}

/*
 * Open a crosspoint switch
 */

static bool command_xps_open(Holder_Type *vars, uint32_t *error_code) {

	unsigned x,y;
	bool res;

	/* Get the parameters */
	res = System_console.get_unsigned(vars, 0, &x);
	if(res) {
		res = System_console.get_unsigned(vars, 1, &y);
	}

	/* Check for table programming error */
	if(!res) {
		*error_code = CEC_TABLE_ERROR;
	}
	else {
		/* Validate parameters */
		if((x >= 32) || (y >= 8)) {
			res = false;
			*error_code = CEC_PARAM_OUT_OF_RANGE;

		}
		else {
			/* Open the desired switch */
			Xps_logical.open_switch(x, y);
		}
	}

	return res;

}

/*
 * Close a crosspoint switch
 */

static bool command_xps_close(Holder_Type *vars, uint32_t *error_code) {

	unsigned x,y;
	bool res;

	/* Get the parameters */
	res = System_console.get_unsigned(vars, 0, &x);
	if(res) {
		res = System_console.get_unsigned(vars, 1, &y);
	}

	/* Check for table programming error */
	if(!res) {
		*error_code = CEC_TABLE_ERROR;
	}
	else {
		/* Validate parameters */
		if((x >= 32) || (y >= 8)) {
			res = false;
			*error_code = CEC_PARAM_OUT_OF_RANGE;

		}
		else {
			/* Close the desired switch */
			Xps_logical.close_switch(x, y);
		}
	}

	return res;

}


/*
 * Display crosspoint switch status
 */

static bool command_xps_status(Holder_Type *vars, uint32_t *error_code) {

	/* Print the header */
	printf("  v^v^v^v^v^v^v^v^ v^v^v^--v^v^v^v^\n");
	printf("  LLLLLLLLLLLLLLLL TTTTTT--GDGDGMGM\n");
	printf("  0011223344556677 001122--00112031\n");
	printf("  RTRTRTRTRTRTRTRT RTRTRT--RTRTRTRT\n");
	printf("  0000000000111111 1111222222222233\n");
	printf("  0123456789012345 6789012345678901\n");
	unsigned x,y;

	/* Print Matrix */
	for(y = 0; y < 8; y++) {
		if((y & 1) == 0) {
			printf("\n"); /* Print blank line between pairs */
		}
		printf("%u ", y);
		for(x = 0; x < 32; x++) {
			if(Xps_logical.get_switch_state(x,y)) {
				printf("X"); /* Connected */
			}
			else {
				printf("0"); /* Disconnected */
			}
			if(x == 15) {
				printf(" "); /* Print a space between groups of 16 X values */
			}
			if(x == 31) {
				printf("\n"); /* Print a newline at the end */
			}
		}
	}

	return true;
}

/*
 * Clear all crosspoint switch connections
 */

static bool command_xps_clear(Holder_Type *vars, uint32_t *error_code) {

	Xps_logical.clear();

	return true;

}



/*
 * Seize a tone plant channel
 */

static bool command_tg_seize(Holder_Type *vars, uint32_t *error_code) {
	bool res = true;

	unsigned channel;
	int32_t descriptor;

	descriptor = System_console.get_tg_descriptor();

	if(descriptor == -1) {
		/* No previously allocated descriptor */
		res = System_console.get_unsigned(vars, 0, &channel);
		if(res && (channel >= Tone_Plant::NUM_TONE_OUTPUTS)) {
			/* Channel out of range */
			*error_code = CEC_PARAM_OUT_OF_RANGE;
			res = false;
			return res;
		}

		if(res) {
			/* Seize the channel */
			descriptor = Tone_plant.channel_seize(channel);
			if(descriptor == -1) {
				/* Channel not available */
				res = false;
				*error_code = CEC_RESOURCE_IN_USE;
			}
			else {
				/* Save descriptor for future reference */
				System_console.set_tg_descriptor(descriptor);
			}
		}
		else {
			/* Table programming error */
			res = false;
			*error_code = CEC_TABLE_ERROR;
		}
	}
	else {
		/* Resource already allocated */
		res = false;
		*error_code = CEC_RESOURCE_ALLOCATED;
	}

	return res;

}


/*
 * Release a tone plant channel
 */

static bool command_tg_release(Holder_Type *vars, uint32_t *error_code) {
	bool res = true;

	int32_t descriptor;

	/* Get previous descriptor */
	descriptor = System_console.get_tg_descriptor();

	if(descriptor != -1) {
		/* Release descriptor */
		Tone_plant.channel_release(descriptor);
		System_console.set_tg_descriptor(-1);
	}
	else {
		/* No previously allocated descriptor */
		res = false;
		*error_code = CEC_NO_RESOURCE;
	}

	return res;

}

/*
 * Send arbitrary tone
 */

static bool command_tg_tone(Holder_Type *vars, uint32_t *error_code) {

	int descriptor = System_console.get_tg_descriptor();

	if(descriptor == -1) {
		*error_code = CEC_NO_RESOURCE;
		return false;
	}

	/* Get frequency and level */
	float freq;
	float level;
	bool res = System_console.get_float(vars, 0, &freq);
	if(res) {
		res = System_console.get_float(vars, 1, &level);
	}
	/* Check for table programming error */
	if(!res) {
		*error_code = CEC_TABLE_ERROR;
		return false;
	}

	/* Validate frequency and level */
	if((freq < 0.0) || (freq > 3400.0) || (level < -32.0) || (level > 0.0)) {
		*error_code = CEC_PARAM_OUT_OF_RANGE;
		return false;
	}

	/* Set frequency and level */
	Tone_plant.send_single_tone(descriptor, freq, level);


	return res;
}

/*
 * Send milliwatt tone
 */

static bool command_tg_milliwatt(Holder_Type *vars, uint32_t *error_code) {
	bool res = true;
	int32_t descriptor = System_console.get_tg_descriptor();
	if(descriptor != -1) {
		/* Valid descriptor */
		Tone_plant.send_single_tone(descriptor, 1004.0, 0.0);
	}
	else {
		/* No descriptor seized previously */
		res = false;
		*error_code = CEC_NO_RESOURCE;
	}

	return res;
}


/*
 * Send dial tone
 */


static bool command_tg_dialtone(Holder_Type *vars, uint32_t *error_code) {
	bool res = true;
	int32_t descriptor = System_console.get_tg_descriptor();
	if(descriptor != -1) {
		/* Valid descriptor */
		Tone_plant.send_call_progress_tones(descriptor, Tone_Plant::CPT_DIAL_TONE);
	}
	else {
		/* No descriptor seized previously */
		res = false;
		*error_code = CEC_NO_RESOURCE;
	}

	return res;
}

/*
 * Stop tone
 */

static bool command_tg_stop(Holder_Type *vars, uint32_t *error_code) {
	bool res = true;
	int32_t descriptor = System_console.get_tg_descriptor();
	if(descriptor != -1) {
		/* Valid descriptor */
		Tone_plant.stop(descriptor);
	}
	else {
		/* No descriptor seized previously */
		res = false;
		*error_code = CEC_NO_RESOURCE;
	}

	return res;

}

/*
 * Display installed line and trunk cards
 */

static bool command_config_hw_view_present(Holder_Type *vars, uint32_t *error_code) {

	uint8_t lcc = HW_pres.get_count_dual_line_cards();
	uint8_t tcc = HW_pres.get_count_trunk_cards();
	uint8_t lcp = HW_pres.get_dual_line_card_positions();
	uint8_t tcp = HW_pres.get_trunk_card_positions();

	if(lcc) {
		printf("\n*** Dual Line Cards ***\n");
		printf("Position Installed\n");
		for(uint8_t index = 0; index < Sub_Line::MAX_DUAL_LINE_CARDS; index++) {
			printf("%-8u ", index);
			if((lcp & (1 << index))) {
				printf("%-s\n", "Yes");
			}
			else {
				printf("%-s\n", "No");
			}
		}
	}

	if(tcc) {
		printf("\n*** Trunk Cards ***\n");
		printf("Position Installed\n");
		for(uint8_t index = 0; index < Trunk::MAX_TRUNK_CARDS; index++) {
			printf("%-8u ", index);
			if((tcp & (1 << index))) {
				printf("%-s\n", "Yes");
			}
			else {
				printf("%-s\n", "No");
			}
		}
	}




	//printf("*** Trunk Cards ***\n");



	return true;
}



/*
 * MF Receiver callback
 */

static void mf_event_callback(void *parameter, uint8_t error_code, uint8_t digit_count, char *data) {
	if(error_code == MF_Decoder::MFE_OK) {
		/* Can't use printf here must log. */
		LOG_DEBUG(TAG, "MF Receiver, Digit count: %d, MF Digits Received: %s", digit_count, data);

	}
	else {
		/* Can't use printf here must log. */
		LOG_WARN(TAG, "MF Receiver timeout\n");
	}
}

/*
 * Seize MF receiver
 */

static bool command_mfr_seize(Holder_Type *vars, uint32_t *error_code) {

	bool res;

	unsigned channel;
	int32_t descriptor;

	/* Get channel to seize */
	res = System_console.get_unsigned(vars, 0, &channel);

	if(res == false) {
		*error_code = CEC_TABLE_ERROR;
		return false;
	}
	/* Range Checking */
	if(channel >= MF_Decoder::NUM_MF_RECEIVERS) {
		*error_code = CEC_PARAM_OUT_OF_RANGE;
		return false;
	}

	/* Test for resource allocation */
	descriptor = System_console.get_mfr_descriptor();
	/* If was allocated previously it must be released and re-seized to properly set up the receiver */
	if(descriptor != -1) {
		MF_decoder.release(descriptor);
	}
	/* Attempt to seize or re-seize the MF Receiver */
	descriptor = MF_decoder.seize(mf_event_callback, NULL, (int32_t) channel, true);
	System_console.set_mfr_descriptor(descriptor);
	if(descriptor == -1) {
		/* Someone else is using it */
		*error_code = CEC_RESOURCE_IN_USE;
		return false;
	}

	return true;
}


/*
 * Release MF receiver
 */

static bool command_mfr_release(Holder_Type *vars, uint32_t *error_code) {

	int32_t descriptor = System_console.get_mfr_descriptor();


	if(descriptor != -1) {
		/* Release the receiver */
		MF_decoder.release(descriptor);
		System_console.set_mfr_descriptor(-1);
	}
	return true;
}


/*
 * DTMF Receiver callback
 */

static void dtmf_event_callback(int32_t descriptor, char digit, uint32_t parameter) {
	LOG_DEBUG(TAG, "DTMF Receiver: %d: DTMF Digit Received: %c", (int) descriptor, digit);
}
/*
 * Seize DTMF receiver
 */

static bool command_dtmfr_seize(Holder_Type *vars, uint32_t *error_code) {

	bool res;

	unsigned channel;
	int32_t descriptor;

	/* Get channel to seize */
	res = System_console.get_unsigned(vars, 0, &channel);

	if(res == false) {
		*error_code = CEC_TABLE_ERROR;
		return false;
	}
	/* Range Checking */
	if(channel >= Dtmf::NUM_DTMF_RECEIVERS) {
		*error_code = CEC_PARAM_OUT_OF_RANGE;
		return false;
	}

	/* Test for resource allocation */
	descriptor = System_console.get_dtmfr_descriptor();
	/* If was allocated previously it must be released and re-seized to properly set up the receiver */
	if(descriptor != -1) {
		Dtmf_receivers.release(descriptor);
	}
	/* Attempt to seize or re-seize the MF Receiver */
	descriptor = Dtmf_receivers.seize(dtmf_event_callback, 711, (int32_t) channel);
	System_console.set_dtmfr_descriptor(descriptor);
	if(descriptor == -1) {
		/* Someone else is using it */
		*error_code = CEC_RESOURCE_IN_USE;
		return false;
	}

	return true;
}


/*
 * Release DTMF receiver
 */

static bool command_dtmfr_release(Holder_Type *vars, uint32_t *error_code) {

	int32_t descriptor = System_console.get_dtmfr_descriptor();


	if(descriptor != -1) {
		/* Release the receiver */
		Dtmf_receivers.release(descriptor);
		System_console.set_dtmfr_descriptor(-1);
	}
	return true;
}

/*
 * Place a trunk online
 */

static bool command_trunk_online(Holder_Type *vars, uint32_t *error_code) {
	unsigned trunk_number;
	bool res;

	res = System_console.get_unsigned(vars, 0, &trunk_number);

	if(res == false) {
		*error_code = CEC_TABLE_ERROR;
		return false;
	}

	res = Trunks.go_online(trunk_number);

	if(res == false) {
		*error_code = CEC_TRUNK_NOT_PRESENT_OR_IN_USE;

	}

	return res;

}

/*
 * Place a trunk offline
 */

static bool command_trunk_offline(Holder_Type *vars, uint32_t *error_code) {
	unsigned trunk_number;
	bool res;

	res = System_console.get_unsigned(vars, 0, &trunk_number);

	if(res == false) {
		*error_code = CEC_TABLE_ERROR;
		return false;
	}

	res = Trunks.go_offline(trunk_number);

	if(res == false) {
		*error_code = CEC_TRUNK_NOT_PRESENT_OR_IN_USE;

	}

	return res;

}

static bool command_trunk_busy(Holder_Type *vars, uint32_t *error_code) {
	unsigned trunk_number;
	bool res;

	res = System_console.get_unsigned(vars, 0, &trunk_number);

	if(res == false) {
		*error_code = CEC_TABLE_ERROR;
		return false;
	}
	res = Trunks.is_in_use(trunk_number);
	if(res) {
		printf("Trunk %d is busy\n", trunk_number);
	}
	else {
		printf("Trunk %d is not busy\n", trunk_number);
	}
	return true;

}




/*
 *
 *
 *  Class implementation
 *
 *
 *
 */


void Console::_print_error_code(uint32_t error_code) {
	if(error_code > CEC_UNKNOWN) {
		error_code = CEC_UNKNOWN;
	}
	const char *error_string = console_error_strings[error_code];

	printf("CE%02u: %s\n", (unsigned) error_code, error_string);
}

/*
 * Parse a command entered by the user
 */

bool Console::_parse_command(unsigned argc, const Command_Table_Entry_Type *top, uint32_t *error_code) {
	const Command_Table_Entry_Type *current_table = top;
	bool done = false;
	bool res = true;
	unsigned param = 0;
	unsigned command_table_index;
	const Command_Table_Entry_Type *cte = NULL;

	*error_code = CEC_NONE;

	while(!done) {

		for(command_table_index = 0; !done; command_table_index++) {
			cte = current_table + command_table_index;
			/* Test for end of argument list */
			/* Test for end of table */
			if((param >= argc) || (cte->keyword[0] == 0)) {
				done = true;
				res = false;
				*error_code = CEC_UNK_CMD;
				break;
			}
			if(!strcmp(cte->keyword, this->_arg_list[param])) {
				/* Keyword Match */
				if(cte->next_table) { /* Case 1: Another command table? */
					current_table = cte->next_table;
					param++;
					break;
				}
				else if(cte->command) { /* Case 2: Call a command function */
					/* Check for args */
					if(cte->arguments) {
						uint32_t table_arguments;
						param++;

						/* Calculate the number of parameters */
						uint32_t num_params = argc - param;

						/* Calculate the number of arguments in the table */
						for(table_arguments = 0; (cte->arguments[table_arguments] != AT_END) &&
							(table_arguments < MAX_ARGS) ; table_arguments++);

						/* Check that the correct number of arguments have been processed */
						if(num_params != table_arguments) {
							/* Argument mismatch error */
							*error_code = CEC_INC_NUM_PARAMS;
							done = true;
							res = false;
						}
						/* Save number of parameters for get unsigned and float methods */
						this->_max_parameters = num_params;
						/* Process arguments here */
						uint32_t a_index;
						bool ap_done = false;
						for(a_index = 0; !done && !ap_done && (a_index < MAX_ARGS); a_index++) {
							uint8_t a_type = cte->arguments[a_index];

							switch(a_type) {
								case AT_UINT: {
									unsigned val;
									if(sscanf(this->_arg_list[param],"%u", &val) != 1) {
										*error_code = CEC_PARAM_ERROR;
										done = true;
										res = false;
										break;
									}

									this->_args[a_index].id = AT_UINT;
									this->_args[a_index].uint = val;
									break;
								}

								case AT_FL: {
									float val;
									if(sscanf(this->_arg_list[param],"%f", &val) != 1) {
										*error_code = CEC_PARAM_ERROR;
										done = true;
										res = false;
										break;
									}
									this->_args[a_index].id = AT_FL;
									this->_args[a_index].fl = val;
									break;
								}

								default:
									/* No more arguments to convert */
									ap_done = true;
									this->_args[a_index].id = AT_END;
									break;

							}
							param++;
						}

					}
					else {
						this->_args[0].id = AT_END; /* No arguments for this command */
					}

					/* Ensure no previous error */
					if(!done) {
						/* Execute command */
						if(cte->command) {
							res = (*cte->command)(this->_args, error_code);
						}
						else {
							*error_code = CEC_TABLE_ERROR;
							res = false;
						}
						done = true;
					}

				}
				else {
					/* Unrecognized action */
					*error_code = CEC_TABLE_ERROR;
					res = false;
					done = true;
				}
			}
		}
	}
	return res;
}


/*
 * Called by top.cpp to set up the console
 */

void Console::setup(void) {
	/* Set up console UART to use UART 5 */
	Console_uart.setup(&huart5);
	this->set_tg_descriptor(-1);
	this->set_mfr_descriptor(-1);
	this->set_dtmfr_descriptor(-1);
}

/*
 * Called by top.cpp repeatedly to process console data
 */

void Console::loop(void) {
	int argc;
	uint32_t error_code;
	Console_uart.putc(':'); /* Bypass buffering in newlib */
	Console_uart.putc(' ');
	this->get_line(this->_line_buffer, sizeof(this->_line_buffer));
	if(strlen(this->_line_buffer)) {
		argc = this->make_arglist((char **) &this->_arg_list, MAX_ARGS, this->_line_buffer);
		bool res = this->_parse_command(argc, command_table_top, &error_code);
		if(!res) {
			this->_print_error_code(error_code);
		}
	}
}


/*
 * Simple line input with backspace support
 */

char *Console::get_line(char *line, uint16_t max_length, Callback_Type callback) {

	for(uint16_t index = 0;;) {
		char c;
		do {
			osDelay(5); /* Let other tasks run while we wait for input */
			c = Console_uart.getc();
		}
		while(!c);

		if(c == 0x0a) { /* Ignore line feed */
			continue;
		}
		else if(c == 0x0d) { /* Enter key (linux) */
			line[index] = 0;
			Console_uart.putc('\n');
			break;

		}
		else if(c == 0x08) { /* Backspace key */
			if(index) {
				Console_uart.putc(0x08);
				Console_uart.putc(' ');
				Console_uart.putc(0x08);
				index--;
			}

		}

		else if((callback) && (c < 0x20)) { /* Extensibility handler */
			(*callback)(line, c, index, max_length);
		}
		else if(c < 0x20) { /* Ignore other control characters */
			continue;
		}
		else { /* Printable characters */
			if(index == (max_length - 1)) {
				/* No more room in line buffer */
				continue;
			}
			else {
				Console_uart.putc(c);
				line[index++] = c;
			}

		}

	}
	return line;
}

/*
 * Set tone generator descriptor in use.
 *
 * Set to -1 if no tone generator is in use.
 */

void Console::set_tg_descriptor(int descriptor) {
	this->_tg_descriptor = descriptor;

}

/*
 * Get tone generator descripter currently in use.
 * -1 means no tone generator siezed.
 */

int Console::get_tg_descriptor() {
	return this->_tg_descriptor;
}

/*
 * Set MF receiver descriptor in use.
 *
 * Set to -1 if no MF receiver is in use.
 */

void Console::set_mfr_descriptor(int descriptor) {
	this->_mfr_descriptor = descriptor;

}

/*
 * Get MF receiver descripter currently in use.
 * -1 means no MF Receiver siezed.
 */

int Console::get_mfr_descriptor() {
	return this->_mfr_descriptor;
}

/*
 * Set DTMF receiver descriptor in use.
 *
 * Set to -1 if no DTMF receiver is in use.
 */

void Console::set_dtmfr_descriptor(int descriptor) {
	this->_dtmfr_descriptor = descriptor;

}

/*
 * Get DTMF receiver descripter currently in use.
 * -1 means no DTMF Receiver siezed.
 */

int Console::get_dtmfr_descriptor() {
	return this->_dtmfr_descriptor;
}




/*
 * Retreive an integer parameter
 * Return true if successful or false if not.
 */

bool Console::get_unsigned(Holder_Type *vars, unsigned parameter_index, unsigned *val) {
	if((parameter_index <= this->_max_parameters) && (vars[parameter_index].id == AT_UINT)) {
		*val = vars[parameter_index].uint;
		return true;
	}
	else {
		return false;
	}
}

/*
 * Retreive a float parameter
 * Return true if successful or false if not.
 */

bool Console::get_float(Holder_Type *vars, unsigned parameter_index, float *val) {
	if((parameter_index <= this->_max_parameters) && (vars[parameter_index].id == AT_FL)) {
		*val = vars[parameter_index].fl;
		return true;
	}
	else {
		return false;
	}
}


/*
 * Make a list of pointers to arguments from string passed in.
 *
 * Note: modifies the string passed in.
 *
 * Return number of arguments found.
 */

enum {FIND_FIRST_CHAR=0, FIND_FIRST_SPACE};

uint32_t Console::make_arglist(char **argv, int max_args, char *str) {
	int argc = 0;
	int state = FIND_FIRST_CHAR;
	bool done = false;
	char *arg_start = NULL;

	/* Trim spaces from end, if any */

	int index = strlen(str);

	/* index to last char */
	if(index) {
		index--;
	}

	while (index) {
		if(str[index] != ' ') {
			break;
		}
		str[index] = 0;
		index--;
	}

	/* Build Argument list */

	for(int index = 0; done == false; index++) {
		char c = str[index];
		switch(state) {
			case FIND_FIRST_CHAR:
				if(c != ' ') {
					arg_start = str + index;
					state = FIND_FIRST_SPACE;

				}
				else if (!c) {
					/* Found end of string */
					done = true;
				}
				break;

			case FIND_FIRST_SPACE:
				if((c == ' ') || (!c)) {
					if(arg_start) {
						if(c) {
							/* Terminate argument */
							str[index] = 0;
						}
						argv[argc++] = arg_start;
						arg_start = NULL;
						state = FIND_FIRST_CHAR;
						if(argc >= max_args) {
							done = true;
						}
					}
					if(!c) {
						/* Found end of string */
						done = true;
					}

				}
				break;
		}

	}

	return argc;

}

} /* End namespace console */
