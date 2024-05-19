/*
 * console.h
 *
 *  Created on: Feb 21, 2024
 *      Author: srodgers
 */

#pragma once
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "top.h"
#include "uart.h"

namespace Console {

const uint8_t MAX_KEYWORD = 16;
const uint8_t MAX_ARGS = 8;

enum {AT_END = 0, AT_UINT, AT_FL};
enum {CEC_NONE = 0, CEC_UNK_CMD, CEC_INC_NUM_PARAMS, CEC_PARAM_ERROR, CEC_PARAM_OUT_OF_RANGE, CEC_TABLE_ERROR,
	CEC_RESOURCE_IN_USE, CEC_NO_RESOURCE, CEC_RESOURCE_ALLOCATED, CEC_TRUNK_NOT_PRESENT_OR_IN_USE,

	CEC_UNKNOWN};

typedef struct Holder_Type{
	uint8_t id;
	float fl;
	uint32_t uint;
}Holder_Type;

typedef void (*Callback_Type)(char *line, char c, uint16_t index, uint16_t max_length);
typedef bool (*Exec_Command_Type)(Holder_Type *vars, uint32_t *error_code);

typedef struct Command_Table_Entry_Type {
	const struct Command_Table_Entry_Type *next_table;
	Exec_Command_Type command;
	const uint8_t *arguments;
	const char keyword[MAX_KEYWORD];

} Command_Table_Entry_Type;

class Console {
public:
	void setup(void);
	void loop(void);
	char *get_line(char *line, uint16_t max_length, Callback_Type callback = NULL);
	uint32_t make_arglist(char **argv, int max_args, char *str);
	bool get_unsigned(Holder_Type *vars, unsigned parameter_index, unsigned *val);
	bool get_float(Holder_Type *vars, unsigned parameter_index, float *val);
	void set_tg_descriptor(int descriptor);
	int get_tg_descriptor();
	void set_mfr_descriptor(int descriptor);
	int get_mfr_descriptor();
	void set_dtmfr_descriptor(int descriptor);
	int get_dtmfr_descriptor();

protected:
	bool _parse_command(unsigned argc, const Command_Table_Entry_Type *top, uint32_t *error_code);
	void _print_error_code(uint32_t error_code);

	int _tg_descriptor;
	int _mfr_descriptor;
	int _dtmfr_descriptor;
	unsigned _max_parameters;
	Holder_Type _args[MAX_ARGS];
	char _line_buffer[128];
	char *_arg_list[MAX_ARGS];
};

} /* End namespace console */
extern Console::Console System_console;




