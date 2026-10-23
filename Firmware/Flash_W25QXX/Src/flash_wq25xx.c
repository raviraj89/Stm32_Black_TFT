/**
 ******************************************************************************
 * File Name		: flash_wq25xx.c
 * Description		: This file provides code for the SPI based flash driver
 ******************************************************************************
 */
/* Includes ------------------------------------------------------------------*/
#include "flash_wq25xx.h"



/* Global Variables ----------------------------------------------------------*/
uint16_t        LedPin[NB_LED] = {GPIO_PIN_6, GPIO_PIN_7};
uint16_t 		KeyPin[NB_KEY] = {GPIO_PIN_4, GPIO_PIN_3};


