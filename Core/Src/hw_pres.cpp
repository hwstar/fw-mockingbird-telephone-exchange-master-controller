#include "top.h"
#include "logging.h"
#include "err_handler.h"
#include "hw_pres.h"
#include "i2c_engine.h"

HW_Pres::HW_Pres HW_pres;

namespace HW_Pres {


/*
 * Callback to get status of last I2C transaction
 */

static void _probe_callback(uint32_t type, uint32_t status, uint32_t trans_id) {
	HW_pres.probe_callback(type, status, trans_id);
}


void HW_Pres::probe_callback(uint32_t type, uint32_t status, uint32_t trans_id) {
	this->_response_received = true;
	this->_response_status = status;

}

void HW_Pres::probe(void) {

	/*
	 * Test for line cards first
	 */

	this->_num_installed_dual_line_cards = this->_installed_dual_line_cards =
	this->_num_installed_trunk_cards = this->_installed_trunk_cards = 0;

	for(uint8_t index = 0; index < Sub_Line::MAX_DUAL_LINE_CARDS; index++) {
		uint8_t data[2] = {0, 0};
		bool failed = false;
		/*
		 * Attempt power off and look for I2C errors
		 */
		/* first line */
		this->_response_received = false;
		data[0] = 0;
		I2c.queue_transaction(I2C_Engine::I2CT_WRITE_REG8, 0,  index, Sub_Line::LINE_CARD_I2C_ADDRESS,
				Sub_Line::REG_POWER_CTRL, 2, data, _probe_callback);
		while(this->_response_received == false) {
			osDelay(1);
		}
		if(this->_response_status != I2C_Engine::I2CEC_OK) {
			failed = true;
		}

		/* Second line */


		if(!failed) {
			this->_response_received = false;
			data[0] = 1;
			I2c.queue_transaction(I2C_Engine::I2CT_WRITE_REG8, 0,  index, Sub_Line::LINE_CARD_I2C_ADDRESS,
					Sub_Line::REG_POWER_CTRL, 2, data, _probe_callback);
			while(this->_response_received == false)  {
				osDelay(1);
			}

			if(this->_response_status != I2C_Engine::I2CEC_OK) {
				failed = true;
			}
		}

		if(!failed) {
			this->_installed_dual_line_cards |= (1 << index);
			this->_num_installed_dual_line_cards++;
		}

	}

	for(uint8_t index = 0; index < Trunk::MAX_TRUNK_CARDS; index++) {
		uint8_t data[2] = {0, 0};
		bool failed = false;
		/*
		 * Attempt trunk card reset
		 */
		this->_response_received = false;
		data[0] = 0;
		I2c.queue_transaction(I2C_Engine::I2CT_WRITE_REG8, 0,  index + Sub_Line::MAX_DUAL_LINE_CARDS, Trunk::TRUNK_CARD_I2C_ADDRESS,
				Trunk::REG_RESET, 2, data, _probe_callback);
		while(this->_response_received == false) {
			osDelay(1);
		}
		if(this->_response_status != I2C_Engine::I2CEC_OK) {
			failed = true;
		}

		if(!failed) {
			this->_installed_trunk_cards |= (1 << index);
			this->_num_installed_trunk_cards++;
		}

	}


}




} /* End namespace HW_Pres */
