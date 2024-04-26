#pragma once


namespace Xps {

const uint8_t NUM_CPS_CHIPS = 2;
const uint8_t MAX_ROWS = 16;
const uint8_t MAX_COLUMNS = 8;

class Xps {
public:
	void init(void);
	void clear(void);
	void modify(uint32_t x, uint32_t y, uint32_t cs_number, bool state);

protected:
	void _set_cs_state(uint32_t cs_number, bool state);
	osMutexId_t _lock;
};


} /* End namespace Xps */

extern Xps::Xps Xps_driver;
