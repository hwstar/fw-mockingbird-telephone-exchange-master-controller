#pragma once


namespace Atten {

const uint32_t MAX_NUM_CARDS = 7;

class Atten {
public:
	bool get_state(uint32_t card);
};



} /* End namespace atten */

extern Atten::Atten Attention;
