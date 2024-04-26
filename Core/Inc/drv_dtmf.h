#pragma once
#include "top.h"




namespace Dtmf {

enum {DS_WAIT_STB_TRUE=0, DS_WAIT_STB_FALSE};

const uint8_t NUM_DTMF_RECEIVERS = 2;

typedef void (*Dtmf_Callback)(int32_t descriptor, char digit);

class Dtmf {
public:
	void init(void);
	void poll();
	int32_t seize(Dtmf_Callback callback, int32_t receiver=-1);
	void release(int32_t descriptor);

protected:
	/* Low level hardware interface functions */
	bool _read_stb(uint32_t receiver);
	char _read_digit_code(uint32_t receiver);



	osMutexId_t _lock;
	uint32_t _siezed_receivers;
	uint8_t _state[NUM_DTMF_RECEIVERS];
	char _digit[NUM_DTMF_RECEIVERS];
	Dtmf_Callback _callback[NUM_DTMF_RECEIVERS];

};




} /* End namespace Dtmf */

/* Access to DTMF receiver class for anyone who includes this header */

extern Dtmf::Dtmf Dtmf_receivers;
