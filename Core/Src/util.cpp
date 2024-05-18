
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

