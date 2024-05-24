
#include "top.h"
#include "util.h"
#include "logging.h"
#include "err_handler.h"
#include "pool_alloc.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "util";


namespace Util {

typedef struct Pin_Map {
	GPIO_TypeDef* port;
	uint16_t pin;
} Pin_Map;

/*
 * Logical to physical pin mapping table
 */

static const Pin_Map pin_map[NUM_LOGICAL_PIN_MAPPINGS] = {
		{OSCOPE_TRIG1_GPIO_Port, OSCOPE_TRIG1_Pin},
		{OSCOPE_TRIG2_GPIO_Port, OSCOPE_TRIG2_Pin},
		{OSCOPE_TRIG3_GPIO_Port, OSCOPE_TRIG3_Pin},
		{DTMF_TOE0_GPIO_Port, DTMF_TOE0_Pin},
		{DTMF_TOE1_GPIO_Port, DTMF_TOE1_Pin},
		{DTMF_STB0_GPIO_Port, DTMF_STB0_Pin},
		{DTMF_STB1_GPIO_Port, DTMF_STB1_Pin},
		{DTMF_3_GPIO_Port, DTMF_3_Pin},
		{DTMF_2_GPIO_Port, DTMF_2_Pin},
		{DTMF_1_GPIO_Port, DTMF_1_Pin},
		{DTMF_0_GPIO_Port, DTMF_0_Pin},
		{ATTEN0_GPIO_Port, ATTEN0_Pin},
		{ATTEN1_GPIO_Port, ATTEN1_Pin},
		{ATTEN2_GPIO_Port, ATTEN2_Pin},
		{ATTEN3_GPIO_Port, ATTEN3_Pin},
		{ATTEN4_GPIO_Port, ATTEN4_Pin},
		{ATTEN5_GPIO_Port, ATTEN5_Pin},
		{ATTEN6_GPIO_Port, ATTEN6_Pin},
		{XB_SW_DATA_GPIO_Port, XB_SW_DATA_Pin},
		{XB_SW_RESET_GPIO_Port, XB_SW_RESET_Pin},
		{XB_SW_STB_GPIO_Port, XB_SW_STB_Pin},
		{XB_SW_CS0_GPIO_Port, XB_SW_CS0_Pin},
		{XB_SW_CS1_GPIO_Port, XB_SW_CS1_Pin},
		{XB_SW_X3_GPIO_Port, XB_SW_X3_Pin},
		{XB_SW_X2_GPIO_Port, XB_SW_X2_Pin},
		{XB_SW_X1_GPIO_Port, XB_SW_X1_Pin},
		{XB_SW_X0_GPIO_Port, XB_SW_X0_Pin},
		{XB_SW_Y2_GPIO_Port, XB_SW_Y2_Pin},
		{XB_SW_Y1_GPIO_Port, XB_SW_Y1_Pin},
		{XB_SW_Y0_GPIO_Port, XB_SW_Y0_Pin},
};

/* Storage for string allocators */
static Pool_Alloc::Pool_Alloc _short_strings_allocator;
static Pool_Alloc::Pool_Alloc _long_strings_allocator;
static char _short_strings_pool[SHORT_STRINGS_SIZE * NUM_SHORT_STRINGS];
static char _long_strings_pool[LONG_STRINGS_SIZE * NUM_LONG_STRINGS];

/*
 * init method. Called once after RTOS is up and running
 */

void Util::init(void) {
	/* Initialize short strings allocator */
	_short_strings_allocator.pool_init(_short_strings_pool, SHORT_STRINGS_SIZE, NUM_SHORT_STRINGS);
	/* Initialize long strings allocator */
	_long_strings_allocator.pool_init(_long_strings_pool, LONG_STRINGS_SIZE, NUM_LONG_STRINGS);
}

/*
 * Allocate a short string from the string pool
 */
char *Util::allocate_short_string(void)  {
	return reinterpret_cast<char *> (_short_strings_allocator.allocate_object());
}

/*
 * Deallocate a short string and return it to the pool
 */

void Util::deallocate_short_string(char *str) {
	if(!str) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	_short_strings_allocator.deallocate_object(str);
}

/*
 * Return the number of short strings allocated
 */

uint32_t Util::get_num_allocated_short_strings(void) {
	return _short_strings_allocator.get_num_allocated_objects();
}

/*
 * Allocate a long string from the string pool
 */
char *Util::allocate_long_string(void)  {
	return reinterpret_cast<char *> (_long_strings_allocator.allocate_object());
}

/*
 * Deallocate a long string and return it to the pool
 */

void Util::deallocate_long_string(char *str) {
	if(!str) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	_long_strings_allocator.deallocate_object(str);
}

/*
 * Return the number of long strings allocated
 */

uint32_t Util::get_num_allocated_long_strings(void) {
	return _long_strings_allocator.get_num_allocated_objects();
}

/*
 * Strncpy with guaranteed termination
 */

char *Util::strncpy_term(char *dest, const char *source, size_t len) {
	if((!dest) || (!source)) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	char *res = strncpy(dest, source, len);
	dest[len-1] = 0;
	return res;
}

/*
 * A string compare function which ignores case
 */

int32_t Util::strcasecmp(char const *a, char const *b)
{
	if((!a) || (!b)) {
		POST_ERROR(Err_Handler::EH_NPFA);

	}
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a)
            return d;
    }
}

/*
 * Make a trunk dial string from a zero terminated string of subscriber dialed digits.
 * Adds KP, then an optional prefix, then a slice of user dialed digits, or all of the user dialed digits, then ST.
 * st_type is an optional parameter, and is set to ST by default, but can be STP (a), ST2P (b) or ST3P (c).
 *
 * Returns the created trunk dial string;
 */

char *Util::make_trunk_dial_string(char *dest, const char *src, uint32_t start, uint32_t end, uint32_t max_len, char *prefix, char st_type) {
	if((!dest) || (!src)) {
		POST_ERROR(Err_Handler::EH_NPFA);

	}

	if(max_len < 3) {
		POST_ERROR(Err_Handler::EH_INVP);
	}

	uint32_t src_len = strlen(src);

	uint32_t prefix_len;
	if(prefix) {
		prefix_len = strlen(prefix);

	}
	else {
		prefix_len = 0;
	}

	uint32_t total_len = 2; /* KP and ST */

	if((start == 0) && ( end == 0)) {
		/* Not a slice */
		total_len += src_len + prefix_len;
	}
	else {
		if(start >= end) {
			POST_ERROR(Err_Handler::EH_INVP);
		}
		total_len += (end - start) + prefix_len;
	}

	if(total_len > max_len) {
		POST_ERROR(Err_Handler::EH_INVP);
	}
	char *p = prefix;
	char *d = dest;
	const char *s = src;
	uint8_t ml = max_len;


	/* Append KP */
	*d++ = '*';
	*d = 0;
	ml--;

	/* Concatenate prefix, if any */
	if(prefix_len) {
		while(*p) {
			*d++ = *p++;
		}
		*d = 0;

	}

	/* Copy address */
	if((start == 0) && (end == 0)) {
		/* Case all dialed digits */
		for(uint8_t i = 0; s[i] && ml ; i++, ml--) {
			*d++ = s[i];
		}


	}
	else {
		uint8_t strt_index = start - 1;
		uint8_t stp_index = end - 1;
		/* Case slice of dialed digits */
		ml = (stp_index - strt_index) + 1;
		for(uint8_t i = strt_index; s[i] && ml; i++, ml-- ) {
			*d++ = s[i];
		}
	}

	/* Append ST */

	*d++ = st_type;
	*d = 0;
	return dest;
}

/*
 * Duplicate a string using long string memory pool
 *
 * String needs to be freed when no longer needed using deallocate_long_string() to avoid no memory fatal errors.
 */

char *Util::strdup(const char *str) {
	if(!str) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}
	char *dup_string = this->allocate_long_string();
	this->strncpy_term(dup_string, str, LONG_STRINGS_SIZE );
	return dup_string;
}

/*
 * Copy string until stop character is seen. Do not include the stop character
 * String copied into a long string pool allocation.
 * Returns NULL if failure.
 *
 * String needs to be freed when no longer needed using deallocate_long_string() to avoid no memory fatal errors.
 */

char *Util::strdup_until(const char *str, char stop_char, uint32_t max_len) {
	uint32_t stop_pos;
	/* Attempt to locate stop character in string */
	for(stop_pos = 0; (( stop_pos < max_len - 1) && (str[stop_pos]) && (str[stop_pos] != stop_char)); stop_pos++);

	if(stop_pos >= max_len) {
		/* Max len reached */
		return NULL;
	}
	else if(!str[stop_pos]) {
		/* End of string reached */
		return NULL;
	}
	else if (stop_pos >= LONG_STRINGS_SIZE - 1) {
		/* Stop position beyond size of long string */
		return NULL;
	}

	/* Allocate a long string */
	char *dup_string = this->allocate_long_string();
	if(dup_string) {
		/* Copy substring to newly allocated long string */
		this->strncpy_term(dup_string, str, stop_pos + 1);
	}

	return dup_string;
}

/*
 * Trim all spaces and tabs from string
 */

char *Util::trim(char *str) {

	char *work_string = this->strdup(str);
	uint32_t s_index;
	uint32_t d_index;
	for(s_index = 0, d_index = 0; work_string[s_index]; s_index++) {
		if((work_string[s_index] == ' ')||(work_string[s_index] == '\t')) {
			continue;
		}
		else {
			str[d_index++] = work_string[s_index];
		}
	}
	str[d_index] = 0;
	this->deallocate_long_string(work_string);
	return str;

}

/*
 * Split a string into one or more substrings
 *
 * Arguments:
 *
 * 4. Set the split character to the character used to delimit the substrings.
 * 3. Substring count (passed by reference) needs to be preset to the maximum size of the substring array of pointers.
 * 2. Substrings is an array of string pointers which needs to be passed in.
 * 1. Str is the composite string to split.
 *
 * Allocates a long string from the string pool as a working string which is returned by the function.
 * This will need to be freed by the caller when the substrings are no longer needed.
 *
 * Note: str is not modified by this function. A copy is made, and that gets modified.
 *
 * Return values:
 *
 * 1. Pointer to the modified split string as a long string from the string pool.
 * 2. The substring count variable passed in (as a pointer) will be modified with the actual number of substrings found.
 *
 *
 *
 *
 */

char *Util::str_split(const char *str, char *substrings[], uint32_t &substring_count, char split_char) {

	if((!str) || (!substrings)) {
		POST_ERROR(Err_Handler::EH_NPFA);
	}


	uint32_t max_substrings = substring_count;

	/* We have to have at least 2 as the maximum substring count */
	if(max_substrings < 2) {
		POST_ERROR(Err_Handler::EH_INVP);

	}

	/* Make copy of caller's string */
	char *m_str = this->strdup(str);

	uint32_t start;
	uint32_t index;
	uint32_t found;

	for(start=0, index=0, found = 0; (m_str[index]) && (found < max_substrings); index++)  {
		if(m_str[index] == split_char) {
			/* Found a delimiter */
			m_str[index] = 0;
			/* Save pointer to substring */
			substrings[found] = m_str + start;
			/* Skip over terminated string */
			start = index + 1;
			/* Bump number of substrings found */
			found++;
		}
	}
	if(!m_str[index]) {
		/* Found the end of the string, so add the last substring to the table */
		substrings[found] = m_str + start;
		found++;
	}


	substring_count = found;
	return m_str;

}

/*
 * Attempt to match a string against a table of strings.
 * Return -1 if no match found, else return the index of
 * the match string in the table if there was a match.
 */

int32_t Util::keyword_match(const char *check_str, const char *match_table[]) {
	int32_t index;
	for(index = 0; match_table[index]; index++) {
		if(!strcmp(check_str, match_table[index])) {
			break;
		}
	}
	if(!match_table[index]) {
		index = -1;
	}
	return index;
}

/*
 * Return true if all of the characters in the string are digits 0-9
 */

bool Util::is_digits(const char *str) {
	while(*str) {
		if((*str < '0') || (*str > '9'))
			return false;
		str++;
	}
	return true;

}

/*
 * Return true if the string supplied is a routing table entry
 */
bool Util::is_routing_table_entry(const char *str) {
	bool is_match_str = (*str == '_');
	if(is_match_str) {
		str++;
	}
	while(*str) {
		if((*str != 'N') && (*str != 'X')) {
			if((*str < '0') || (*str > '9')) {
				return false;
			}
		}
		else {
			if(!is_match_str) {
				return false;
			}
		}
		str++;
	}
	return true;
}


/*
 * Memset
 */

void *Util::memset(void *str, int c, size_t len) {
	uint8_t *stru8 = (uint8_t *) str;
	for(size_t index = 0; index < len; index++) {
		stru8[index] = (uint8_t) c;
	}
	return str;
}

/*
 * Get GPIO pin state
 */

bool Util::get_gpio_pin_state(uint32_t logical_pin) {
	if(logical_pin >= NUM_LOGICAL_PIN_MAPPINGS) {
		POST_ERROR(Err_Handler::EH_LPME);
	}
	GPIO_TypeDef* port = pin_map[logical_pin].port;
	uint16_t pin = pin_map[logical_pin].pin;

	return HAL_GPIO_ReadPin(port, pin);

}


/*
 * Set GPIO pin state
 */

void Util::set_gpio_pin_state(uint32_t logical_pin, bool state) {

	if(logical_pin >= NUM_LOGICAL_PIN_MAPPINGS) {
		POST_ERROR(Err_Handler::EH_LPME);
	}
	GPIO_TypeDef* port = pin_map[logical_pin].port;
	uint16_t pin = pin_map[logical_pin].pin;


	if(state) {
		HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
	} else {
		HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
	}

}

/*
 * Pulse a GPIO pin high then low
 */

void Util::pulse_gpio_pin(uint32_t logical_pin) {
	this->set_gpio_pin_state(logical_pin, true);
	for (volatile uint8_t i=0; i<= 10; i++);
	this->set_gpio_pin_state(logical_pin, false);
}

/*
 * Toggle a gpio pin
 */

void Util::toggle_gpio_pin(uint32_t logical_pin) {

	if(logical_pin >= NUM_LOGICAL_PIN_MAPPINGS) {
		POST_ERROR(Err_Handler::EH_LPME);
	}
	GPIO_TypeDef* port = pin_map[logical_pin].port;
	uint16_t pin = pin_map[logical_pin].pin;

	HAL_GPIO_TogglePin(port, pin);

}

/*
 * Update scope test point
 */

bool Util::update_scope_test_point(uint32_t test_point, bool state, const char *tag, uint32_t line) {
	switch(test_point) {
	case SCOPE_TP1:
		this->set_gpio_pin_state(OSCOPE_TRIG1, state);
		break;

	case SCOPE_TP2:
		this->set_gpio_pin_state(OSCOPE_TRIG2, state);
		break;

	case SCOPE_TP3:
		this->set_gpio_pin_state(OSCOPE_TRIG3, state);
		break;


	default:
		return false;
	}
	return true;
}

/*
 * Toggle scope test point
 */

bool Util::toggle_scope_test_point(uint32_t test_point, const char *tag, uint32_t line) {
	switch(test_point) {
	case SCOPE_TP1:
		this->toggle_gpio_pin(OSCOPE_TRIG1);
		break;

	case SCOPE_TP2:
		this->toggle_gpio_pin(OSCOPE_TRIG2);
		break;

	case SCOPE_TP3:
		this->toggle_gpio_pin(OSCOPE_TRIG3);
		break;


	default:
		return false;
	}
	return true;
}



} // End namespace Util

Util::Util Utility;

