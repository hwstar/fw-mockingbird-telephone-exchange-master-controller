
#pragma once

#include "top.h"
#include "mf_receiver.h"


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

/* 4K of short strings */
const uint32_t SHORT_STRINGS_SIZE = 32;
const uint32_t NUM_SHORT_STRINGS = 128;

/* 2K of long strings */
const uint32_t LONG_STRINGS_SIZE = 128;
const uint32_t NUM_LONG_STRINGS = 16;


class Util {
public:
	void init(void);
	char *allocate_short_string(void);
	void deallocate_short_string(char *str);
	uint32_t get_num_allocated_short_strings(void);
	char *allocate_long_string(void);
	void deallocate_long_string(char *str);
	uint32_t get_num_allocated_long_strings(void);
	char *strncpy_term(char *dest, const char *source, size_t len);
	int32_t strcasecmp(char const *a, char const *b);
	void *memset(void * str, int c, size_t n);
	bool get_gpio_pin_state(uint32_t logical_pin);
	void set_gpio_pin_state(uint32_t logical_pin, bool state);
	void pulse_gpio_pin(uint32_t logical_pin);
	void toggle_gpio_pin(uint32_t logical_pin);
	bool update_scope_test_point(uint32_t test_point, bool state, const char *tag="nomodule", uint32_t line=0);
	bool toggle_scope_test_point(uint32_t test_point, const char *tag="nomodule", uint32_t line=0);
	char *make_trunk_dial_string(char *dest, const char *src, uint32_t start, uint32_t end, uint32_t max_len, char *prefix = NULL, char st_type = '#');



protected:

};



} /* End namespace Util */

extern Util::Util Utility;
