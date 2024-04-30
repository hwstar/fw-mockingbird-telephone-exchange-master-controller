#pragma once
#include "top.h"

namespace Json_RW {



const uint32_t MAX_NODE_BLOCKS = 128;
const uint32_t FILE_BUFFER_SIZE = 8192;

enum {VT_OBJECT, VT_STRING, VT_NUMBER, VT_BOOLEAN, VT_NULL_TYPE };

/*
 * JSON Node Data Type
 */

struct Json_Node {
	union Values {
		bool b_val;
		float f_val;
		char *str_val;
	} values;
	uint8_t value_type;
};

struct Json_Object {
	char *key;
	struct Json_Node *value;
};

typedef struct Json_Node Json_Node_Type;
typedef struct Json_Object Json_Object_Type;



class Json_RW {

protected:
	Json_Node *allocate_node(void);


	Json_Node *first_node;

public:
	void init(void);

};





} /* End Namespace JSON_RW */

extern Json_RW::Json_RW Json_rw;

