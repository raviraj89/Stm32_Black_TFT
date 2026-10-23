/**
  ******************************************************************************
  * File Name		: flash_wq25xx.h
  * Description		: This file provides code to interface the SPI based Flash driver
  ******************************************************************************
  */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FLASH_WQ25XX_H
#define __FLASH_WQ25XX_H


/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include "stm32f4xx_hal.h"

/* Constants -----------------------------------------------------------------*/
#define NB_LED	2
#define LED2	0
#define LED3	1

#define NB_KEY	2
#define KEY0	0
#define KEY1 	1

/* Instruction Set-------------------------------------------------------------*/
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_DISABLE                              0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_ENABLE                               0x06


/* Structures -----------------------------------------------------------------*/


/* Functions -----------------------------------------------------------------*/
void LedSet(unsigned int Led, GPIO_PinState state);
void LedOn(unsigned int Led);
void LedOff(unsigned int Led);
void LedToggle(unsigned int Led);

int KeyTest(unsigned int key);

extern __IO uint32_t uwTick;
void Delay(uint32_t delay);
#endif // /*__FLASH_WQ25XX_H */