#pragma once
#include "top.h"
#include "pool_alloc.h"

namespace Config_RW {


const uint32_t MAX_KEY = 23;
const uint32_t MAX_VALUE = 95;
const uint32_t MAX_SECTION = 23;
const uint32_t MAX_SECTION_BLOCKS = 64;
const uint32_t MAX_NODE_BLOCKS = 256;
const uint32_t LINE_BUFFER_SIZE = 127;


enum {VT_OBJECT, VT_STRING, VT_NUMBER, VT_BOOLEAN};
enum {PT_RINGING=0, PT_RECEIVER_LIFTED=1, PT_DIAL_TONE=2, PT_DIGITS_RECOGNIZED=3,
	PT_TRUNK_SIGNALING=4, PT_CALLED_PARTY_BUSY=5, PT_CONGESTION=6,

	MAX_PT_TYPE
};

/*
 * Data structures
 */


struct Config_Node {
	char key[MAX_KEY + 1];
	char value[MAX_VALUE + 1];
	uint32_t line_number;
	struct Config_Node *next;
	struct Config_Node *prev;
};

struct Config_Section {
	char section[MAX_SECTION + 1];
	struct Config_Section *next;
	struct Config_Section *prev;
	struct Config_Node *head;
	struct Config_Node *tail;
};

typedef struct Config_Node Config_Node_Type;
typedef struct Config_Section Config_Section_Type;
typedef bool (*Traverse_Nodes_Callback_Type)(const char *section, const char *key, const char *value, uint32_t line_num, void *data);


class Config_RW {

protected:
	bool _is_valid_label(const char *str);
	Config_Section_Type *_add_section(char *section_keyword);
	Config_Node_Type *_add_node(char *key, char *value);
	int32_t _read_line(void);
	void _process_line(void);

	char _line_buffer[LINE_BUFFER_SIZE + 1];
	uint8_t _section_memory_pool[sizeof(Config_Section_Type) * MAX_SECTION_BLOCKS];
	uint8_t _node_memory_pool[sizeof(Config_Node_Type) * MAX_NODE_BLOCKS];

	int32_t _fd;
	int32_t _file_status;
	uint32_t _line_number;

	Config_Section_Type *_section_head;
	Config_Section_Type *_section_tail;

	osMutexId_t _lock;
	Pool_Alloc::Pool_Alloc _section_pool;
	Pool_Alloc::Pool_Alloc _node_pool;


public:
	void init(void);
	const char *get_value(const char *section, const char *key, uint32_t &line_number);
	uint32_t get_arg_count(const char *value);
	int32_t get_arguments(const char *value, const char *format, ...);
	bool traverse_nodes(const char *section, Traverse_Nodes_Callback_Type callback=NULL, void *data=NULL);
	void syntax_error(uint32_t line_num, const char *message = NULL);
	bool stat_and_load_audio_sample(const char *sample_name, const char *sample_path);
	const char *get_progress_tone_buffer_name(uint32_t pt_type);
	Config_Section_Type *find_section(const char *section_name);
	Config_Node_Type *find_node(const char *node_name, Config_Node_Type *node_head);
	Config_Node_Type *find_node(unsigned num, Config_Node_Type *node_head);


};





} /* End Namespace Config_RW */

extern Config_RW::Config_RW Config_rw;

