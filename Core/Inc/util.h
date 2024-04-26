
#pragma once

#include "top.h"


/* Logical pin names */
enum {OSCOPE_TRIG1=0, OSCOPE_TRIG2, OSCOPE_TRIG3, DTMF_TOE0, DTMF_TOE1, DTMF_STB0, DTMF_STB1,
	DTMF_3, DTMF_2, DTMF_1, DTMF_0, ATTEN0, ATTEN1, ATTEN2, ATTEN3, ATTEN4, ATTEN5, ATTEN6, XB_SW_DATA, XB_SW_RESET,
	XB_SW_STB, XB_SW_CS0, XB_SW_CS1, XB_SW_X3, XB_SW_X2, XB_SW_X1, XB_SW_X0,
	XB_SW_Y2,XB_SW_Y1,XB_SW_Y0,

	NUM_LOGICAL_PIN_MAPPINGS
};

enum {SCOPE_TP1=1, SCOPE_TP2, SCOPE_TP3};

/*
 * Macro to update a scope test point state
 */

#define UPDATE_SCOPE_TEST_POINT(test_point, state) Utility.update_scope_test_point(test_point, state, TAG, __LINE__)
#define TOGGLE_SCOPE_TEST_POINT(test_point) Utility.toggle_scope_test_point(test_point, TAG, __LINE__)



namespace Util {



class Util {
public:
	char *strncpy_term(char *dest, const char *source, size_t len);
	int32_t strcasecmp(char const *a, char const *b);
	bool get_gpio_pin_state(uint32_t logical_pin);
	void set_gpio_pin_state(uint32_t logical_pin, bool state);
	void pulse_gpio_pin(uint32_t logical_pin);
	void toggle_gpio_pin(uint32_t logical_pin);
	bool update_scope_test_point(uint32_t test_point, bool state, const char *tag="nomodule", uint32_t line=0);
	bool toggle_scope_test_point(uint32_t test_point, const char *tag="nomodule", uint32_t line=0);
protected:

};



} /* End namespace Util */

extern Util::Util Utility;
