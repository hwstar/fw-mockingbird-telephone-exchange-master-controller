#include "top.h"
#include "file_io.h"
#include "logging.h"
#include "err_handler.h"
#include "config_rw.h"
#include "util.h"
#include "pool_alloc.h"
#include "connector.h"
#include "sub_line.h"
#include "trunk.h"


Config_RW::Config_RW Config_rw;

namespace Config_RW {


enum {RL_OK = 0, RL_EOF = -1, RL_FS_ERR = -2, RL_TRUNC_LINE = -3};

static const char *TAG = "configrw";
static const char *SWITCH_CONF_FILE = "/config/switch.conf";


/*
 * Checks to see that all of the keys and values in the routing table are valid
 */

static bool _routing_table_callback(const char *section, const char *key, const char *value, uint32_t line_number, void *data) {
	if(!data) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	uint32_t equip_type = *(uint32_t *) data;

	if(!Utility.is_routing_table_entry(key)) {
		Config_rw.syntax_error(line_number, "Not a routing table entry");
	}
	/* Split the value into substrings */
	char *routing_table_substrings[3];
	uint32_t substring_count = 3;
	char *alloc_mem = Utility.str_split(value, routing_table_substrings, substring_count, ',');
	/* Must be exactly 2 substrings */
	if(substring_count != 2){
		Utility.deallocate_long_string(alloc_mem);
		Config_rw.syntax_error(line_number, "Incorrect number of arguments");
	}
	/* Must be sub only if equipment type is ET_LINE, and sub or tg if equipment type is ET_TRUNK */
	if(equip_type == 2) {
		if(strcmp(routing_table_substrings[0],"sub")) {
			Utility.deallocate_long_string(alloc_mem);
			Config_rw.syntax_error(line_number, "Invalid equipment type");
		}
	}
	else if(equip_type == 1) {
		const char *eqt_strings[] = {"sub","tg", NULL};
		if(Utility.keyword_match(routing_table_substrings[0], eqt_strings) == -1) {
			Utility.deallocate_long_string(alloc_mem);
			Config_rw.syntax_error(line_number, "Invalid equipment type");

		}
	}

	/* Must be able to look up the physical subscriber line */
	int32_t res = Config_rw.traverse_nodes(routing_table_substrings[1]);
	if(res == false) {
		Utility.deallocate_long_string(alloc_mem);
		Config_rw.syntax_error(line_number, "Invalid physical subscriber line section");
	}

	Utility.deallocate_long_string(alloc_mem);
	return true;
}



/*
 * Checks to see that a physical subscriber line has the required keys
 * Level 2
 */


static bool _phys_subscriber_callback(const char *section, const char *key, const char *value, uint32_t line_number, void *data) {
	if(!data) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	uint32_t *keyword_bits = (uint32_t *) data;
	uint32_t equip_type = 1;

	/* Check for permitted keys */
	const static char *permitted_keywords[] = {"type", "phys_line", "phone_number", "routing_table", NULL};
	int32_t match = Utility.keyword_match(key, permitted_keywords);
	if(match == -1) {
		Config_rw.syntax_error(line_number, "Bad key");
	}

	switch(match) {
	case 0: /* type */
		/* fxs is the only valid value for now */
		if(!strcmp(value,"fxs")) {
		*keyword_bits |= 0x8000;
		}
		break;

	case 1: { /* phys_line */
		unsigned phys_line;
		/* must be a number from 0 to 7 */
		int res = sscanf(value,"%u", &phys_line);
		if((res != 1) || (phys_line > 7)) {
			Config_rw.syntax_error(line_number, "Bad physical line number");
		}
		*keyword_bits |= 1;
		break;
	}


	case 2: /* phone number */
		if(!Utility.is_digits(value)) {
			Config_rw.syntax_error(line_number, "Not digits");
		}
		*keyword_bits |= 2;
		break;

	case 3: { /* routing table */
		/* Check the routing table section supplied */
		int32_t res = Config_rw.traverse_nodes(value, _routing_table_callback, (void *) &equip_type);
		if(res == false) {
			Config_rw.syntax_error(line_number, "Routing table not found");
		}
		*keyword_bits |= 4;
		break;
	}

	default:
		break;
	}

	return true;

}
/*
 * Checks to see that a subscribers record is valid
 * Level 1
 */
static bool _subscriber_callback(const char *section, const char *key, const char *value, uint32_t line_number, void *data){
	/* Check key */
	unsigned key_num;
	uint32_t keyword_bits = 0;
	int res = sscanf(key,"%u", &key_num);
	if(res != 1) {
		Config_rw.syntax_error(line_number, "Physical line not a number");
	}
	if(key_num > Sub_Line::MAX_DUAL_LINE_CARDS *2) {
		Config_rw.syntax_error(line_number, "Physical line number out of range");
	}
	/* Check Value by traversing the physical subscriber line sections */
	res = Config_rw.traverse_nodes(value, _phys_subscriber_callback, (void *) &keyword_bits);
	if(res) {
		if(keyword_bits != 0x8007) {
			/* Did not see all the required keywords */
			Config_rw.syntax_error(line_number, "Missing keywords in physical line section");
		}
	}
	else {
		Config_rw.syntax_error(line_number, "Physical line section missing");
	}

	return true;

}



/*
 * Check for valid label characters in string
 */

bool Config_RW::_is_valid_label(const char *str) {
	bool res = true;

	for(;*str; str++) {
		if((*str >= 'A') && (*str <= 'Z')) {
			continue;
		}
		if((*str >= 'a') && (*str <= 'z')) {
			continue;
		}
		if((*str >= '0') && (*str <= '9')) {
			continue;
		}
		if(*str == '_') {
			continue;
		}
		else {
			res = false;
			break;
		}
	}
	return res;
}


/*
 * Add a new section
 *
 * Adds the new section to the linked list of sections.
 * Copies the section keyword into the section.
 * Returns a pointer to the new section.
 */


Config_Section_Type *Config_RW::_add_section(char *section_keyword) {

	if(!section_keyword) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	Config_Section_Type *new_section = (Config_Section_Type *) this->_section_pool.allocate_object();

	if(this->_section_head == NULL) {
		/* First section in list */
		this->_section_head = this->_section_tail = new_section;
	}
	else {
		/* Append to section to end of list */
		this->_section_tail->next = new_section;
		new_section->prev = this->_section_tail;
		this->_section_tail = new_section;
	}

	Utility.strncpy_term(new_section->section, section_keyword, sizeof(new_section->section));

	return new_section;
}

/*
 * Add a new node under the last section entry found
 */

Config_Node_Type *Config_RW::_add_node(char *key, char *value) {

	if((!key) || (!value)) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	Config_Section_Type *cur_section = this->_section_tail;

	Config_Node_Type *new_node = (Config_Node_Type *) this->_node_pool.allocate_object();

	/* Initialize the node data */
	new_node->line_number = this->_line_number;
	Utility.strncpy_term(new_node->key, key, sizeof(new_node->key));
	Utility.strncpy_term(new_node->value, value, sizeof(new_node->value));


	if(!cur_section->head) {
		/* Case 1 empty node list */
		cur_section->head = cur_section->tail = new_node;
	}
	else {
		/* Case 2 append to end of node list */
		cur_section->tail->next = new_node;
		new_node->prev = cur_section->tail;
		cur_section->tail = new_node;

	}

	return new_node;
}
/*
 * Process line in the line buffer
 */

void Config_RW::_process_line(void) {
	/* Line must have non zero length */

	if(strlen(this->_line_buffer)) {
		/* If comment, truncate the line at the comment */
		char *comment = strchr(this->_line_buffer, '#');
		if(comment) {
			*comment = 0;
		}
		int32_t length = strlen(this->_line_buffer);
		if(length) {
			/* Remove spaces and tabs from line */
			Utility.trim(this->_line_buffer);
			/* A section header will have an opening brace at the first character position*/
			if(this->_line_buffer[0] == '[') {
				/* Section header? */
				char *section_keyword = Utility.strdup_until(this->_line_buffer + 1, ']', LINE_BUFFER_SIZE);
				if(section_keyword) {
					/* Test for valid label */
					if(!this->_is_valid_label(section_keyword)){
						this->syntax_error(this->_line_number, "Bad character in name");
					}
					else {
						/* Create a new section */
						this->_add_section(section_keyword);
					}
					/* Free the section keyword storage */
					Utility.deallocate_long_string(section_keyword);
				}
				else {
					this->syntax_error(this->_line_number, "Bad section label");
				}

			}
			else {
				/* Most a likely key/value, test for correct syntax */
				uint32_t num_substrings = 2;
				char *substrings[2];
				char *alloc_str = Utility.str_split(this->_line_buffer, substrings, num_substrings, ':');

				if((num_substrings != 2) ||
						(!strlen(substrings[0])) ||
						(!strlen(substrings[1])) ||
						(!this->_is_valid_label(substrings[0]))) {
					this->syntax_error(this->_line_number,"Bad key:value");

				}
				else {
					this->_add_node(substrings[0], substrings[1]);
				}

				Utility.deallocate_long_string(alloc_str);
			}


		}


	}

}

/*
 * Read one line into the line buffer
 */

int32_t Config_RW::_read_line(void) {
	uint8_t c;
	int32_t res = RL_OK;
	uint32_t i;

	this->_line_buffer[0] = 0;

	for(i = 0; (i <  LINE_BUFFER_SIZE) && ((this->_file_status = File_io.read(this->_fd, &c, 1)) == 1); i++) {
		if(c == 0x0d) { /* Skip CR */
			continue;
		}
		else if(c == 0x0a) { /* Stop on LF */
			break;
		}
		else if(c == 0) { /* Stop on zero */
			break;
		}
		else {
			this->_line_buffer[i] = c;
		}
	}

	this->_line_buffer[i] = 0;

	/* Test for EOF or error */
	if(this->_file_status == 0) {
		res = RL_EOF;
	}
	else if(this->_file_status < 0) {
		res = RL_FS_ERR;
	}
	 /* Test for line length too long */
	else if(i >= LINE_BUFFER_SIZE) {
		res = RL_TRUNC_LINE;
	}

	return res;

}


void Config_RW::init(void) {



	/* Mutex attributes */
	static const osMutexAttr_t configrw_allocator_mutex_attr = {
		"ConfigRWAllocatorMutex",
		osMutexRecursive | osMutexPrioInherit,
		NULL,
		0U
	};


	/* Create the lock mutex */

	this->_lock = osMutexNew(&configrw_allocator_mutex_attr);
	if (this->_lock == NULL) {
		POST_ERROR(Err_Handler::EH_LCE);
	}

	/* Initialize memory pools */
	this->_section_pool.pool_init(this->_section_memory_pool, sizeof(Config_Section_Type), MAX_SECTION_BLOCKS);
	this->_node_pool.pool_init(this->_node_memory_pool, sizeof(Config_Node_Type), MAX_NODE_BLOCKS);

	/* Open the config file */
	if((this->_fd = File_io.open(SWITCH_CONF_FILE, File_Io::O_RDONLY)) < 0) {
		LOG_ERROR(TAG, "File system error: %s", File_io.error_string(this->_fd));
		POST_ERROR(Err_Handler::EH_NOCF);
	}

	LOG_INFO(TAG,"Switch config file: %s opened successfully", SWITCH_CONF_FILE);

	/* Read the configuration into the configuration tree */
	this->_section_head = NULL;
	this->_section_tail = NULL;
	bool done = false;
	this->_line_number = 1;
	while(!done) {
		int32_t res = this->_read_line();
		switch(res) {
		case RL_OK:
			/* Process line */
			this->_process_line();
			break;

		case RL_EOF:
			/* Process line, then exit */
			this->_process_line();
			done = true;
			break;

		case RL_FS_ERR:
			LOG_ERROR(TAG, "File system error: %s", File_io.error_string(res));
			POST_ERROR(Err_Handler::EH_FSER);
			break;

		case RL_TRUNC_LINE:
			LOG_ERROR(TAG, "Line %u is too long, max is %u characters", this->_line_number, LINE_BUFFER_SIZE);
			POST_ERROR(Err_Handler::EH_CFER);
			break;
		}
		this->_line_number++;
	}
	/* Close the config file */
	File_io.close(this->_fd);
	LOG_INFO(TAG, "Switch config file closed");
	/*
	 * Log config tree usage statistics
	 */
	LOG_INFO(TAG, "Used %u sections out of %u available", this->_section_pool.get_num_allocated_objects(), MAX_SECTION_BLOCKS );
	LOG_INFO(TAG, "Used %u nodes out of %u available", this->_node_pool.get_num_allocated_objects(), MAX_NODE_BLOCKS );


	/*
	 * Check for mandatory sections, then check to see that the sections in them exist
	 */

	if(!this->traverse_nodes("subscribers", _subscriber_callback)) {
		this->syntax_error(0,"Subscriber section is missing");
	}


}

/*
 * Get a value from the node tree
 *
 * Used by the "config" methods to while configuring lines, routing tables, etc.
 *
 * Returns the configuration value, or NULL if it wasn't found in the configuration tree
 * Populates the line number variable with the configuration file line number if a non-NULL value
 * is returned.
 */

const char *Config_RW::get_value(const char *section, const char *key, uint32_t &line_number) {



	/* Find section */
	Config_Section_Type *section_data;
	Config_Node_Type *node_data;

	if((!section) || (!key)) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	for(section_data = this->_section_head; (section_data); section_data = section_data->next) {
		if(!strcmp(section_data->section, section)) {
			break;
		}
	}
	if(!section_data) {
		return NULL; /* Section not found */
	}

	/* Find Node */

	for(node_data = section_data->head; node_data; node_data = node_data->next) {
		if(!strcmp(node_data->key, key)) {
			break;
		}
	}

	if(!node_data) {
		return NULL; /* Node not found */
	}

	line_number = node_data->line_number;
	return node_data->value;
}

/*
 * Traverse a the nodes in a section from head to tail.
 *
 * Returns true if the section was found, otherwise false.
 *
 * The callback argument has a default type of NULL. If not supplied,
 * then this function will only test for the existence of the section
 * and nothing else and return true of the section exists, or false if not.
 */

bool Config_RW::traverse_nodes(const char *section, Traverse_Nodes_Callback_Type callback, void *data) {

	if(!section) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	/* Attempt to find the section */
	Config_Section_Type *section_data = this->_section_head;

	while(section_data) {
		if(!strcmp(section, section_data->section)) {
			break;
		}
		section_data = section_data->next;
	}

	if(!section_data) {
		return false; /* No sections defined */
	}

	if(!callback) {
		return true;
	}

	Config_Node_Type *node_data = section_data->head;

	while(node_data) {
		bool res = (*callback)(section_data->section, node_data->key, node_data->value, node_data->line_number, data);
		/*
		 * If the callback returned false, it found what it was looking for, and
		 * the iteration can be stopped.
		 */
		if(res == false) {
			break;
		}
		node_data = node_data->next;
	}

	return true;
}

/*
 * Print syntax error message and panic
 */

void Config_RW::syntax_error(uint32_t line_num, const char *message) {
	if(!message) {
		LOG_ERROR(TAG,"Syntax error on line %u", line_num);
	}
	else {
		LOG_ERROR(TAG," Syntax error on line %u: %s", line_num, message);
	}
	POST_ERROR(Err_Handler::EH_CFSE);
}


/*
 * Return arguments in value looked up.
 *
 * Uses sscanf to parse the arguments in the value field
 *
 * Function argmuments:

 * value string - string with arguments embedded in it.
 * line number - will be updated with the line number of the matched section/key on return
 * format - sscanf format string
 * ... Pointers to sscanf variables
 *
 * Returns
 *
 * n - the number of arguments found by sscanf.
 *
 */


int32_t Config_RW::get_arguments(const char *value, const char *format, ...) {
	va_list alp;
	va_start(alp, format);
	struct _reent reent;
	if((!value) || (!format)) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	int32_t res = _vsscanf_r(&reent, value, format, alp);
	va_end(alp);
	return res;
}


/*
 * Count the number of comma-separated arguments
 */


uint32_t Config_RW::get_arg_count(const char *value) {

	uint32_t arg_count = 0;
	bool look_for_comma = false;
	for(uint32_t index = 0; value[index];index++) {
		if(look_for_comma) {
			if(value[index] == ',') {
				look_for_comma = false;
			}
		}
		else {
			/* Test for zero length arg */
			arg_count++;
			if(value[index] != ',') {
				look_for_comma = true;
			}
		}
	}
	return arg_count;
}




} /* End Namespace Config_RW */


