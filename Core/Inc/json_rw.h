#pragma once
#include "top.h"

namespace Json_RW {

const uint32_t MAX_NODE_BLOCKS = 128;
const uint32_t MAX_JSON_STRING = 16;
const uint32_t FILE_BUFFER_SIZE = 8192;
const uint32_t MAX_JSON_NODE_BUFFER = 128;

/*
 * JSON Node Data Type
 */

typedef struct Json_Node {
	char key[MAX_JSON_STRING];
	uint8_t value_type;
	union value {
		bool bool_val;
		int32_t int_val;
		char str_val[MAX_JSON_STRING];
		struct Json_Node *ll_node;
	};
	struct Json_Node *next;

} Json_Node_Type;



class Json_RW {

protected:
	char _node_buffer[MAX_JSON_NODE_BUFFER];

public:
	void init(void);

};





} /* End Namespace JSON_RW */

extern Json_RW::Json_RW Json_rw;

