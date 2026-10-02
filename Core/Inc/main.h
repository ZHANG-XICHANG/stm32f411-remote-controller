/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "stm32f4xx_hal.h"

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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define THR_Pin GPIO_PIN_4
#define THR_GPIO_Port GPIOA
#define YAW_Pin GPIO_PIN_5
#define YAW_GPIO_Port GPIOA
#define PITCH_Pin GPIO_PIN_6
#define PITCH_GPIO_Port GPIOA
#define ROLL_Pin GPIO_PIN_7
#define ROLL_GPIO_Port GPIOA
#define nRF24_CSN_Pin GPIO_PIN_12
#define nRF24_CSN_GPIO_Port GPIOB
#define nRF24_CE_Pin GPIO_PIN_8
#define nRF24_CE_GPIO_Port GPIOA
#define nRF24_IRQ_Pin GPIO_PIN_10
#define nRF24_IRQ_GPIO_Port GPIOA
#define Button_1_Pin GPIO_PIN_15
#define Button_1_GPIO_Port GPIOA
#define Button_2_Pin GPIO_PIN_3
#define Button_2_GPIO_Port GPIOB
#define Button_3_Pin GPIO_PIN_4
#define Button_3_GPIO_Port GPIOB
#define Button_4_Pin GPIO_PIN_5
#define Button_4_GPIO_Port GPIOB
#define Button_5_Pin GPIO_PIN_8
#define Button_5_GPIO_Port GPIOB
#define Button_6_Pin GPIO_PIN_9
#define Button_6_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
