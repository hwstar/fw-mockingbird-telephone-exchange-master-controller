/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define XB_SW_CS2_Pin GPIO_PIN_2
#define XB_SW_CS2_GPIO_Port GPIOE
#define USER_BUTTON_Pin GPIO_PIN_13
#define USER_BUTTON_GPIO_Port GPIOC
#define ATTEN0_Pin GPIO_PIN_2
#define ATTEN0_GPIO_Port GPIOF
#define ATTEN1_Pin GPIO_PIN_3
#define ATTEN1_GPIO_Port GPIOF
#define ATTEN2_Pin GPIO_PIN_4
#define ATTEN2_GPIO_Port GPIOF
#define ATTEN3_Pin GPIO_PIN_5
#define ATTEN3_GPIO_Port GPIOF
#define XB_SW_STB_Pin GPIO_PIN_10
#define XB_SW_STB_GPIO_Port GPIOF
#define XB_SW_DATA_Pin GPIO_PIN_0
#define XB_SW_DATA_GPIO_Port GPIOC
#define OSCOPE_TRIG2_Pin GPIO_PIN_5
#define OSCOPE_TRIG2_GPIO_Port GPIOA
#define OSCOPE_TRIG3_Pin GPIO_PIN_6
#define OSCOPE_TRIG3_GPIO_Port GPIOA
#define GREEN_LED_Pin GPIO_PIN_0
#define GREEN_LED_GPIO_Port GPIOB
#define DTMF_STB0_Pin GPIO_PIN_11
#define DTMF_STB0_GPIO_Port GPIOF
#define DTMF_STB1_Pin GPIO_PIN_12
#define DTMF_STB1_GPIO_Port GPIOF
#define OSCOPE_TRIG1_Pin GPIO_PIN_13
#define OSCOPE_TRIG1_GPIO_Port GPIOF
#define DTMF_TOE0_Pin GPIO_PIN_0
#define DTMF_TOE0_GPIO_Port GPIOG
#define DTMF_TOE1_Pin GPIO_PIN_1
#define DTMF_TOE1_GPIO_Port GPIOG
#define XB_SW_Y0_Pin GPIO_PIN_7
#define XB_SW_Y0_GPIO_Port GPIOE
#define XB_SW_Y1_Pin GPIO_PIN_8
#define XB_SW_Y1_GPIO_Port GPIOE
#define XB_SW_Y2_Pin GPIO_PIN_9
#define XB_SW_Y2_GPIO_Port GPIOE
#define XB_SW_CS0_Pin GPIO_PIN_10
#define XB_SW_CS0_GPIO_Port GPIOE
#define XB_SW_CS1_Pin GPIO_PIN_11
#define XB_SW_CS1_GPIO_Port GPIOE
#define XB_SW_X0_Pin GPIO_PIN_12
#define XB_SW_X0_GPIO_Port GPIOE
#define XB_SW_X1_Pin GPIO_PIN_13
#define XB_SW_X1_GPIO_Port GPIOE
#define XB_SW_X2_Pin GPIO_PIN_14
#define XB_SW_X2_GPIO_Port GPIOE
#define XB_SW_X3_Pin GPIO_PIN_15
#define XB_SW_X3_GPIO_Port GPIOE
#define SD_CS_Pin GPIO_PIN_12
#define SD_CS_GPIO_Port GPIOB
#define RED_LED_Pin GPIO_PIN_14
#define RED_LED_GPIO_Port GPIOB
#define ATTEN4_Pin GPIO_PIN_2
#define ATTEN4_GPIO_Port GPIOG
#define ATTEN5_Pin GPIO_PIN_3
#define ATTEN5_GPIO_Port GPIOG
#define ATTEN6_Pin GPIO_PIN_4
#define ATTEN6_GPIO_Port GPIOG
#define ATTEN7_Pin GPIO_PIN_5
#define ATTEN7_GPIO_Port GPIOG
#define XB_3_PRES_N_Pin GPIO_PIN_15
#define XB_3_PRES_N_GPIO_Port GPIOA
#define SD_PRESENT_N_Pin GPIO_PIN_0
#define SD_PRESENT_N_GPIO_Port GPIOD
#define XB_SW_RESET_Pin GPIO_PIN_6
#define XB_SW_RESET_GPIO_Port GPIOD
#define DTMF_1_Pin GPIO_PIN_9
#define DTMF_1_GPIO_Port GPIOG
#define DTMF_0_Pin GPIO_PIN_12
#define DTMF_0_GPIO_Port GPIOG
#define DTMF_2_Pin GPIO_PIN_14
#define DTMF_2_GPIO_Port GPIOG
#define DTMF_3_Pin GPIO_PIN_15
#define DTMF_3_GPIO_Port GPIOG
#define I2C_EXP_RESET_N_Pin GPIO_PIN_4
#define I2C_EXP_RESET_N_GPIO_Port GPIOB
#define BLUE_LED_Pin GPIO_PIN_7
#define BLUE_LED_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */


extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern DMA_HandleTypeDef hdma_adc2;

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c4;

extern SAI_HandleTypeDef hsai_BlockA1;
extern SAI_HandleTypeDef hsai_BlockB1;
extern DMA_HandleTypeDef hdma_sai1_a;
extern DMA_HandleTypeDef hdma_sai1_b;

extern DMA_HandleTypeDef hdma_sdmmc1_rx;
extern DMA_HandleTypeDef hdma_sdmmc1_tx;

extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi3;

extern TIM_HandleTypeDef htim4;

extern UART_HandleTypeDef huart5;
extern UART_HandleTypeDef huart7;

#define SD_SPI_HANDLE hspi2

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
