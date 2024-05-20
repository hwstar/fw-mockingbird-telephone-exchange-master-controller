#include "mf_receiver.h"
#include "top.h"
#include "util.h"
#include "uart.h"
#include "i2c_engine.h"
#include "console.h"
#include "logging.h"
#include "tone_plant.h"
#include "drv_dtmf.h"
#include "drv_xps.h"
#include "xps_logical.h"
#include "event.h"
#include "sub_line.h"
#include "trunk.h"
#include "card_comm.h"
#include "file_io.h"
#include "hw_pres.h"
#include "json_rw.h"
#include "connector.h"


Sub_Line::Sub_Line Sub_line;

static const char *TAG = "top";


/* Debug code, remove */
void dummy_callback(uint32_t channel_number) {

}


/*
 * Called once before RTOS initialization
 */

void Top_setup(void) {
	System_console.setup();
	Logger.setup();
	MF_decoder.setup();
	Tone_plant.setup();
}


void Top_init(void) {

	osDelay(1000);

	/* Resources */
	Utility.init();
	Logger.init();
	I2c.init();
	MF_decoder.init();
	Dtmf_receivers.init();
	Tone_plant.init();
	Xps_driver.init();
	Xps_logical.init();
	Card_comm.init();




	/* After resources. Depends on resources being initialized */
	File_io.init();
	Json_rw.init();
	HW_pres.probe();
	Trunks.init();
	Sub_line.init();
	Conn.init();
	Event_handler.init();

	/* Test Code Begin */
	Card_comm.send_command(Card_Comm::RT_LINE, 0, Sub_Line::REG_POWER_CTRL, true);
	Card_comm.send_command(Card_Comm::RT_LINE, 1, Sub_Line::REG_POWER_CTRL, true);
	Card_comm.send_command(Card_Comm::RT_LINE, 2, Sub_Line::REG_POWER_CTRL, true);
	Card_comm.send_command(Card_Comm::RT_LINE, 3, Sub_Line::REG_POWER_CTRL, true);
	Card_comm.send_command(Card_Comm::RT_LINE, 4, Sub_Line::REG_POWER_CTRL, true);
	Card_comm.send_command(Card_Comm::RT_LINE, 5, Sub_Line::REG_POWER_CTRL, true);
	Card_comm.send_command(Card_Comm::RT_LINE, 6, Sub_Line::REG_POWER_CTRL, true);
	Card_comm.send_command(Card_Comm::RT_LINE, 7, Sub_Line::REG_POWER_CTRL, true);

	/* Trash code, only for testing */
	LOG_DEBUG(TAG, "Opening audio file city_ring.ulaw");
	int fd = File_io.open("/audio/city_ring.ulaw", File_Io::O_RDONLY);
	if(fd >= 0) {
		uint32_t city_ring_size = File_io.fsize(fd);
		uint8_t *buffer = Tone_plant.allocate_audio_buffer(city_ring_size, "city_ring");
		if(buffer) {
			if(File_io.read(fd, buffer, city_ring_size) != -1) {
				LOG_DEBUG(TAG,"city ring loaded for testing purposes");
			}
		}
		else {
			LOG_ERROR(TAG, "Audio buffer allocation failed");
		}
		File_io.close(fd);
	}
	else {
		LOG_ERROR(TAG, "Could not open audio file");
	}



	/* Test Code End */



	LOG_INFO(TAG, "Initialization completed");
	/* Turn on green LED to signify Initialization complete */
	HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_SET);
}

/*
 * Default task loop
 */
void Top_default_task(void) {
	System_console.loop();

}


/*
 * Uart interrupt handler
 */
void Top_uart_handler(UART_HandleTypeDef *uart_handle) {

	/* Console RX */
	if(uart_handle == &huart5) {
		char c;
		HAL_UART_Receive(uart_handle, (uint8_t *) &c, 1, 0);
		Console_uart.rx_int(c);
	}
}

/*
 * UART Error Callback
 */

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart_handle) {
	if(uart_handle == &huart5) {
		Console_uart.error_handler(uart_handle);
	}

}

/*
 * ADC DMA Half buffer full interrupt
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc) {
	MF_decoder.handle_buffer(hadc, 0);

}


/*
 * ADC DMA buffer full interrupt
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
	MF_decoder.handle_buffer(hadc, 1);
}


/*
 * SAI DMA half buffer completed buffer callback
 */

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
	Tone_plant.handle_buffer(hsai, 0);

}

/*
 * SAI DMA buffer completed buffer callback
 */

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
	Tone_plant.handle_buffer(hsai, 1);

}

/*
 * SAI Error callback
 */

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai) {
	Tone_plant.handle_error(hsai);
}


/*
 * I2C interrupt handlers
 */

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
	I2c.handler(hi2c, I2C_Engine::MSG_I2C_TX);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
	I2c.handler(hi2c, I2C_Engine::MSG_I2C_RX);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
	I2c.handler(hi2c, I2C_Engine::MSG_I2C_ERR);

}




