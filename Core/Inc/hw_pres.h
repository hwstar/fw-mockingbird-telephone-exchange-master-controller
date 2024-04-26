#pragma once

#include "top.h"
#include "sub_line.h"
#include "trunk.h"

namespace Sub_Line {


} /* End namespace Sub_Line */


namespace HW_Pres {


class HW_Pres {
protected:
	uint8_t _num_installed_dual_line_cards;
	uint8_t _num_installed_trunk_cards;
	uint8_t _installed_dual_line_cards;
	uint8_t _installed_trunk_cards;
	uint8_t _response_status;
	bool _response_received;

public:
	void probe(void);
	void probe_callback(uint8_t status, uint32_t trans_id);
	uint8_t get_count_dual_line_cards(void) {return this->_num_installed_dual_line_cards;};
	uint8_t get_count_trunk_cards(void) {return this->_num_installed_trunk_cards;};
	uint8_t get_dual_line_card_positions(void) {return this->_installed_dual_line_cards;};
	uint8_t get_trunk_card_positions(void) {return this->_installed_trunk_cards;};

};


} /* End namespace HW_Pres */

extern HW_Pres::HW_Pres HW_pres;
