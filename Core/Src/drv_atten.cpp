#include "top.h"
#include "logging.h"
#include "util.h"
#include "drv_atten.h"



namespace Atten {

static const char *TAG = "drvatten";

static const uint8_t lookup[MAX_NUM_CARDS] = {ATTEN0, ATTEN1, ATTEN2, ATTEN3, ATTEN4, ATTEN5, ATTEN6};

bool Atten::get_state(uint32_t card) {

	if(card >= MAX_NUM_CARDS) {
		LOG_PANIC(TAG, "Card number out of range");
	}

	return Utility.get_gpio_pin_state(lookup[card]);

}


} /* End namespace atten */

Atten::Atten Attention;
