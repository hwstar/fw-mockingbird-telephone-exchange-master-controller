#pragma once
#include "main.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdbool.h>


/* top.cpp Functions which need C-compatiable linker names */
#ifdef __cplusplus
extern "C" {
#endif

extern void Top_setup(void); /* Called before RTOS starts running */
extern void Top_init(void); /* Called once from before the default task loop to do system initialization */
/* extern void Top_process_MF_frame(uint8_t buffer_number); */
/* extern void Top_switch_task(void); */
extern void Top_default_task(void); // Default task loop
/* extern void Top_i2c_task(void); */
/* extern void Top_subscriber_task(void); */
/* extern void Top_send_I2S_Audio_Frame(uint8_t buffer_number); */
extern void Top_uart_handler(UART_HandleTypeDef *uart_handle);
/* Weak HAL default function overrides */
extern void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc);
extern void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc);
extern void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai);
extern void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai);
#ifdef __cplusplus

}
#endif
