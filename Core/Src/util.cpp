
#include "top.h"
#include "util.h"
#include "logging.h"
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

/*
 * Strncpy with guaranteed termination
 */

char *Util::strncpy_term(char *dest, const char *source, size_t len) {
	if((!dest) || (!source)) {
		return NULL;
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
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a)
            return d;
    }
}


/*
 * Get GPIO pin state
 */

bool Util::get_gpio_pin_state(uint32_t logical_pin) {
	if(logical_pin >= NUM_LOGICAL_PIN_MAPPINGS) {
		LOG_PANIC(TAG, "Logical pin mapping error");
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
		LOG_PANIC(TAG, "Logical pin mapping error");
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
		LOG_PANIC(TAG, "Logical pin mapping error");
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

