#include "top.h"
#include "file_io.h"
#include "logging.h"
#include "err_handler.h"
#include "file_io.h"
#include "config_rw.h"
#include "util.h"
#include "pool_alloc.h"
#include "tone_plant.h"
#include "connector.h"
#include "sub_line.h"
#include "trunk.h"


Config_RW::Config_RW Config_rw;

namespace Config_RW {


enum {RL_OK = 0, RL_EOF = -1, RL_FS_ERR = -2, RL_TRUNC_LINE = -3};


static const char *TAG = "configrw";
static const char *SWITCH_CONF_FILE = "/config/switch.conf";
const char *types[] = {"ringing", "receiver_lifted", "dial_tone", "digits_recognized", "trunk_signaling", "called_party_busy", "congestion", NULL};


/*
 * Function forward declarations
 */

static bool _routing_table_callback(const char *section, const char *key, const char *value, uint32_t line_number, void *data);
static bool _phys_trunk_callback(const char *section, const char *key, const char *value, uint32_t line_number, void *data);

/*
 * Validate a trunk group
 */

static bool _outgoing_trunk_group(const char *section, const char *key, const char *value, uint32_t line_number, void *data) {
	static const char *keywords[] = {"trunk_list", "start_index", NULL};

	uint32_t *caller_keyword_bits = (uint32_t *) data;
	int32_t res = Utility.keyword_match(key, keywords);

	if(!data) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	switch(res) {
	case 0: { /* trunk_list */
		/* Value must contain something */
		if(!value[0]) {
			Config_rw.syntax_error(line_number, "No trunks defined");
		}
		/* Split the value into substrings */
		/* These are the trunk section names */

		char *substrings[4];
		uint32_t substring_count = 4;
		char *alloc_mem = Utility.str_split(value, substrings, substring_count, ',');
		uint32_t keyword_bits = 0;

		for(uint32_t index = 0; index < substring_count; index++) {

			int32_t res = Config_rw.traverse_nodes(substrings[index], _phys_trunk_callback, (void *) &keyword_bits);
			if(res == false) {
				Utility.deallocate_long_string(alloc_mem);
				Config_rw.syntax_error(0, "No physical trunks found");
			}
			else if(keyword_bits != 7) {
				Utility.deallocate_long_string(alloc_mem);
				Config_rw.syntax_error(0, "Missing required keywords");

			}

		}
		Utility.deallocate_long_string(alloc_mem);
		*caller_keyword_bits |= 0x8000; /* Indicate we saw the mandatory keyword */
	}
		break;


	case 1: /* start_index (optional) */
		if(!value[0]) {
			Config_rw.syntax_error(line_number, "Missing start index");
		}
		break;

	default:
		Config_rw.syntax_error(line_number, "Bad key");
		break;


	}

	return true;
}




/*
 * Validate outgoing trunk groups
 */

static bool _outgoing_trunk_groups_callback(const char *section, const char *key, const char *value, uint32_t line_number, void *data) {

	 uint32_t keyword_bits = 0;

	/* key must be trunk_list */
	if(strcmp(key, "group_list")) {
		Config_rw.syntax_error(line_number, "Trunk list not defined");
	}

	/* Value must contain something */
	if(!value[0]) {
		Config_rw.syntax_error(line_number, "No trunk groups");
	}

	/* Split the value into substrings */
	/* These are the trunk section names */

	char *substrings[8];
	uint32_t substring_count = 8;
	char *alloc_mem = Utility.str_split(value, substrings, substring_count, ',');

	for(uint32_t index = 0; index < substring_count; index++) {

		int32_t res = Config_rw.traverse_nodes(substrings[index], _outgoing_trunk_group, &keyword_bits);
		if(!res) {
			Utility.deallocate_long_string(alloc_mem);
			Config_rw.syntax_error(0, "No trunk groups found");
		}

		if(keyword_bits != 0x8000) {
			Utility.deallocate_long_string(alloc_mem);
			Config_rw.syntax_error(0, "Missing required keywords");
		}

	}
	Utility.deallocate_long_string(alloc_mem);
	return true;
}


/*
 * Validates physical trunk sections
 */

static bool _phys_trunk_callback(const char *section, const char *key, const char *value, uint32_t line_number, void *data) {

	static const char *permitted_keywords[] = {"type", "phys_trunk", "routing_table", NULL};

	if(!data) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	uint32_t *keyword_bits = (uint32_t *) data;

	/* Check for permitted keywords */
	int32_t keyword_index = Utility.keyword_match(key, permitted_keywords);
	if(keyword_index == -1) {
		Config_rw.syntax_error(line_number, "Bad key");
	}
	/* For each permitted keyword, set a bit to be checked by the caller */
	switch(keyword_index) {
	case 0: /* type */
		if(strcmp(value, "e&m")) {
			Config_rw.syntax_error(line_number, "Trunk type must be e&m");
		}

		*keyword_bits |= 1;
		break;

	case 1: { /* phys_trunk */
		unsigned trunk_num;
		int res = sscanf(value,"%u", &trunk_num);
			if(res != 1) {
				Config_rw.syntax_error(line_number, "Physical trunk not a number");
			}
		/* Must not exceed the maximum number of trunk cards */
		if(trunk_num > Trunk::MAX_TRUNK_CARDS) {
			Config_rw.syntax_error(line_number, "Trunk number exceeds maximum");
		}

		*keyword_bits |= 2;
		break;
	}


	case 2:{ /* routing_table */
		uint32_t equip_type = 2;
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
 * Validates incoming trunks
 */


static bool _incoming_trunks_callback(const char *section, const char *key, const char *value, uint32_t line_number, void *data) {
	unsigned key_num;
	uint32_t keyword_bits = 0;
	/* Key must be a number */
	int res = sscanf(key,"%u", &key_num);
		if(res != 1) {
			Config_rw.syntax_error(line_number, "Physical trunk not a number");
		}
	/* Must not exceed the maximum number of trunk cards */
	if(key_num > Trunk::MAX_TRUNK_CARDS) {
		Config_rw.syntax_error(line_number, "Trunk number exceeds maximum");
	}
	/* Value check */

	res = Config_rw.traverse_nodes(value, _phys_trunk_callback, (void *) &keyword_bits);
	if(res == false) {
		Config_rw.syntax_error(line_number, "No physical trunks found");
	}
	if(keyword_bits != 7) {
		Config_rw.syntax_error(0, "Missing required keywords");
	}

	return true;

}

/*
 * Checks to see that the indications defined are valid
 */

static bool _indications_callback(const char *section, const char *key, const char *value, uint32_t line_number, void *data) {
	const char *methods[] = {"none", "precise", "sample", NULL};

	if(!data) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	uint32_t *keyword_bits = (uint32_t *) data;

	/* Split value into substrings */
	uint32_t substring_count = 3;
	char *indications_substrings[3];

	if(!value[0]) {
		Config_rw.syntax_error(line_number, "Missing value(s)");
	}

	char *alloc_mem = Utility.str_split(value, indications_substrings, substring_count, ',');

	/* Must have one or two substrings only */
	if((substring_count > 2)) {
		Utility.deallocate_long_string(alloc_mem);

	}


	/* Attempt to match a type keyword */
	int32_t indication_type = Utility.keyword_match(key, types);

	/* If no type match */
	if(indication_type == -1) {
		Utility.deallocate_long_string(alloc_mem);
		Config_rw.syntax_error(line_number, "Invalid type");

	}

	/* Attempt to match a method keyword */
	int32_t method = Utility.keyword_match(indications_substrings[0], methods);

	/* If no method match */
	if(method  == -1) {
		Utility.deallocate_long_string(alloc_mem);
		Config_rw.syntax_error(line_number, "Invalid method");
	}

	/* Verify that the various combinations of type, method, and audio file are valid */
	bool is_invalid = false;

	switch(indication_type) {
	case PT_RINGING: /* ringing */
		/* Valid methods: precise, sample */
		if(method == 0)
			is_invalid = true;
		else if((method == 2) && (substring_count != 2)) {
			/* No file provided */
			is_invalid = true;
		}
		else {
			/* Stat and load the file */
			if((method == 2) &&(!Config_rw.stat_and_load_audio_sample(types[0], indications_substrings[1]))) {
				is_invalid = true;
			}
			else if((method == 0) && substring_count != 1) {
				is_invalid = true;
			}
			else {
				/* All good */
				*keyword_bits |= 1;
			}

		}
		break;

	case PT_RECEIVER_LIFTED: /* receiver_lifted */
		/* Valid methods: none, sample */
		if(method == 1) {
			is_invalid = true;
		}
		else if((method == 2) && (substring_count != 2)) {
			/* No file provided */
			is_invalid = true;
		}
		else {
			/* Stat and load the file */
			if((method == 2) && (!Config_rw.stat_and_load_audio_sample(types[1], indications_substrings[1]))) {
					is_invalid = true;
			}
			else if((method == 0) && substring_count != 1) {
				is_invalid = true;
			}
			else {
				/* All good */
				*keyword_bits |= 2;
			}
		}
		break;

	case PT_DIAL_TONE: /* dial tone */
		/* Valid methods: precise */
		if((method != 1) || (substring_count != 1)) {
			is_invalid = true;
		}
		else {
			/* All good */
			*keyword_bits |= 4;
		}
		break;

	case PT_DIGITS_RECOGNIZED: /* digits_recognized */
		/* Valid methods: none, sample */
		if(method == 1) {
			is_invalid = true;
		}
		else if((method == 2) && (substring_count != 2)) {
			/* No file provided */
			is_invalid = true;
		}
		else {
			/* Stat and load the file */
			if((method == 2) && (!Config_rw.stat_and_load_audio_sample(types[3], indications_substrings[1]))) {
					is_invalid = true;
			}
			else if((method == 0) && substring_count != 1) {
				is_invalid = true;
			}
			else {
				/* All good */
				*keyword_bits |= 8;
			}
		}
		break;

	case PT_TRUNK_SIGNALING: /* trunk signaling */
		/* Valid methods: none, sample */
		if(method == 1) {
			is_invalid = true;
		}
		else if((method == 2) && (substring_count != 2)) {
			/* No file provided */
			is_invalid = true;
		}
		else {
			/* Stat and load the file */
			if((method == 2) &&(!Config_rw.stat_and_load_audio_sample(types[4], indications_substrings[1]))) {
					is_invalid = true;
			}
			else if((method == 0) && substring_count != 1) {
				is_invalid = true;
			}
			else {
				/* All good */
				*keyword_bits |= 0x10;
			}
		}
		break;

	case PT_CALLED_PARTY_BUSY: /* called party busy */
		/* Valid methods: precise */
		if((method != 1) || (substring_count != 1)) {
			is_invalid = true;
		}
		else {
			/* All good */
			*keyword_bits |= 0x20;
		}
		break;


	case PT_CONGESTION: /* congestion */
		/* Valid methods: precise */
		if((method != 1) || (substring_count != 1)) {
			is_invalid = true;
		}
		else {
			/* All good */
			*keyword_bits |= 0x40;
		}
		break;


	default:
		break;
	}

	Utility.deallocate_long_string(alloc_mem);
	if(is_invalid) {
		Config_rw.syntax_error(line_number, "Invalid method, type, or missing audio file");
	}

	return true;
}

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
	/* Must be sub only if equipment type is 2, and sub or tg if equipment type is 1 */
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
			Config_rw.syntax_error(line_number, "Missing required keywords");
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
/*
 * Attempt to find the section name supplied.
 * If found, then return a pointer to it's section data
 * if not found, then return NULL
 */

Config_Section_Type *Config_RW::find_section(const char *section_name) {
	Config_Section_Type *config_section;
	for(config_section = this->_section_head; config_section; config_section = config_section->next) {
		if(!strcmp(config_section->section, section_name)) {
			break;
		}
	}
	return config_section;
}

/*
 * Attempt to find the node name supplied.
 * If found, then return a pointer to it's section data
 * if not found, then return NULL
 */

Config_Node_Type *Config_RW::find_node(const char *node_name, Config_Node_Type *node_head) {
	Config_Node_Type *config_node;
	for(config_node = node_head; config_node; config_node = config_node->next) {
		if(!strcmp(config_node->key, node_name)) {
			break;
		}
	}
	return config_node;

}

/*
 * Attempt to find the node key as a number supplied.
 * If found, then return a pointer to it's section data
 * if not found, then return NULL
 */

Config_Node_Type *Config_RW::find_node(unsigned num, Config_Node_Type *node_head) {
	Config_Node_Type *config_node;
	unsigned sl_num;
	for(config_node = node_head; config_node; config_node = config_node->next) {
		if(sscanf(config_node->key, "%u", &sl_num) != 1) {
				POST_ERROR(Err_Handler::EH_IPLN);
			}
			/* Check to see if we found the node */
			if(sl_num == num) {
				break;
			}

	}
	return config_node;

}

/*
 * Helper function for find_node_by_path
 */

Config_Node_Type *Config_RW::_find_node_by_path_helper(const char *section, char **substrings, uint32_t num_substrings, uint32_t index) {


	/* Look up the section */
	Config_Section_Type *section_info = this->find_section(section);
	if(!section_info) {
		return NULL; /* Section not found */
	}
	Config_Node_Type *res = NULL;

	/* Get the node key referenced by the substring */
	Config_Node_Type *node = this->find_node(substrings[index], section_info->head);
	if(!node) {
		return res; /* Node not found */

	}

	if(index < num_substrings - 1) {
		/* Not the last substring */
		res = this->_find_node_by_path_helper(node->value, substrings, num_substrings, ++index);
	}
	else {
		/* Was the last substring */
		res = node;
	}
	return res;
}

/*
 * Recursively search for a node from a starting section when given a path
 * The path name made up of section keys separated by forward slashes.
 *
 * If the path can't be found, return NULL, otherwise return a pointer to the node
 * in the configuration tree.
 */

Config_Node_Type *Config_RW::find_node_by_path(const char *starting_section, const char *path) {

	if((!starting_section) || (!path)) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}

	if(!path[0]) {
		return NULL;
	}

	/* Look up the starting section */
	Config_Section_Type *section_info = this->find_section(starting_section);

	if(!section_info) {
		return NULL; /* Starting section not found */
	}

	/* Split path on forward slash boundaries */
	uint32_t num_path_components = 8; /* Maximum number of substrings in the path */
	uint32_t index = 0;
	char *substrings[8];
	char *alloc_str = Utility.str_split(path, substrings, num_path_components, '/');
	Config_Node_Type *res = NULL;
	/* First substring is the node key in the section */
	Config_Node_Type *node = this->find_node(substrings[index], section_info->head);
	if(node) {
		/* Desired node found in starting section */
		if(index < num_path_components - 1) {
			res = this->_find_node_by_path_helper(node->value, substrings, num_path_components, ++index);
		}
		else {
			/* Did not need to recurse */
			res = node;
		}
	}



	Utility.deallocate_long_string(alloc_str);
	return res;

}

/*
 * Check to see that a sample file exists. If it doesn't then return false.
 * If it exists, then load it into a named sample buffer and tag it with the sample name.
 */

bool Config_RW::stat_and_load_audio_sample(const char *sample_name, const char *sample_path) {
	LOG_DEBUG(TAG, "Opening audio file: %s", sample_path);
	int fd = File_io.open(sample_path, File_Io::O_RDONLY);
	if(fd >= 0) {
		/* Get file size */
		uint32_t audio_sample_size = File_io.fsize(fd);
		/* Attempt buffer allocaiton */
		uint8_t *buffer = Tone_plant.allocate_audio_buffer(audio_sample_size, sample_name);
		/* If buffer successfully allocated */
		if(buffer) {
			if(File_io.read(fd, buffer, audio_sample_size) != -1) {
				LOG_DEBUG(TAG,"Audio sample file loaded successfully");
			}
		}
		else {
			/* Buffer allocation failed */
			LOG_ERROR(TAG, "No more space left in audio sample buffer");
			POST_ERROR(Err_Handler::EH_NMA);
		}
		/* Close the file */
		LOG_DEBUG(TAG, "Closing the audio file");
		File_io.close(fd);
	}
	else {
		LOG_ERROR(TAG, "Could not open audio file");
		POST_ERROR(Err_Handler::EH_NSFL);
	}



	return true;
}

/*
 * Return the name of a sample buffer if it has been preloaded into memory
 * otherwise return NULL.
 */

const char *Config_RW::get_progress_tone_buffer_name(uint32_t pt_type) {
	if(pt_type >= MAX_PT_TYPE) {
		POST_ERROR(Err_Handler::EH_INVP);
	}
	const char *pt_name = types[pt_type];

	/* See if the buffer name has been preloaded */
	if(!Tone_plant.audio_buffer_exists(pt_name)){
		pt_name = NULL;
	}
	return pt_name;
}



/*
 * Called after RTOS is up and running
 */

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
	 * Check for mandatory sections, then validate the sections.
	 */

	/* Subscribers section */
	LOG_INFO(TAG, "Validating subscribers section");
	if(!this->traverse_nodes("subscribers", _subscriber_callback)) {
		this->syntax_error(0,"Subscriber section is missing");
	}

	/* Incoming trunks section */
	LOG_INFO(TAG, "Validating incoming trunks section");
	if(!this->traverse_nodes("incoming_trunks", _incoming_trunks_callback)) {
		this->syntax_error(0,"Incoming trunks section is missing");
	}

	/* Outgoing trunk groups section */
	LOG_INFO(TAG, "Validating outgoing trunks section");
	if(!this->traverse_nodes("outgoing_trunk_groups", _outgoing_trunk_groups_callback)) {
		this->syntax_error(0,"Outgoing trunk groups section is missing");
	}


	/* Indications */

	uint32_t keyword_bits = 0;
	LOG_INFO(TAG, "Validating indications section");

	if(!this->traverse_nodes("indications", _indications_callback, &keyword_bits)) {
		this->syntax_error(0,"Indications section is missing");
	}
	if(keyword_bits != 0x7F) {
		this->syntax_error(0,"Not all indication types were defined");
	}

	LOG_INFO(TAG, "Validation complete");

	/*
	 * Log config tree usage statistics
	 */
	LOG_INFO(TAG, "Used %u sections out of %u available", this->_section_pool.get_num_allocated_objects(), MAX_SECTION_BLOCKS );
	LOG_INFO(TAG, "Used %u nodes out of %u available", this->_node_pool.get_num_allocated_objects(), MAX_NODE_BLOCKS );

	/*
	 * Log audio buffer bytes available
	 */
	uint32_t usage = Tone_Plant::AUDIO_SAMPLE_BUFFER_POOL_SIZE - Tone_plant.get_audio_buffer_bytes_available();
	uint32_t usage_percent = (usage * 100)/Tone_Plant::AUDIO_SAMPLE_BUFFER_POOL_SIZE;
	LOG_INFO(TAG, "Audio buffer bytes used: %u out of %u (%u%%)",
			usage,
			Tone_Plant::AUDIO_SAMPLE_BUFFER_POOL_SIZE,
			usage_percent);



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


