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
#define SPI_FLASH_CS_LOW()                               HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET)
#define SPI_FLASH_CS_HIGH()                              HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET)
#define SPI_MODE_USED                                    0 // SPI Clock Mode 0 (CPOL = 0, CPHA = 0)



#define FLASH_W25QXX_TIMEOUT                             1000
#define FLASH_W25QXX_PAGE_SIZE                           256
#define FLASH_W25QXX_SECTOR_SIZE                         4096
#define FLASH_W25QXX_BLOCK_SIZE                          65536
#define FLASH_W25QXX_FLASH_SIZE                          2097152
#define FLASH_W25QXX_FLASH_SIZE_KB                       2048
#define FLASH_W25QXX_START_ADDRESS                       0x000000
#define FLASH_W25QXX_END_ADDRESS                         0x1FFFFF
#define FLASH_STATE_IDLE                                 0
#define FLASH_STATE_BUSY                                 1


#define NB_LED	2
#define LED2	0
#define LED3	1

#define NB_KEY	2
#define KEY0	0
#define KEY1 	1

/* Instruction Set-------------------------------------------------------------*/
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_DISABLE                              0x04
#define FLASH_READ_STATUS_REG1                           0x05
#define FLASH_READ_STATUS_REG2                           0x35
#define FLASH_WRT_STATUS_PROG                            0x01
#define FLASH_PAGE_PROGRAM                               0x02
#define FLASH_QUAD_PAGE_PROGRAM                          0x32
#define FLASH_BLK_ERASE_64                               0xd8
#define FLASH_BLK_ERASE_32                               0x52
#define FLASH_SECTOR_ERASE                               0x20
#define FLASH_CHIP_ERASE                                 0xc7
#define FLASH_ERASE_SUSPEND                              0x75
#define FLASH_ERASE_RESUME                               0x7a
#define FLASH_POWER_DOWN                                 0xB9
#define FLASH_HIGH_PERFORMANCE_MODE                      0xa3
#define FLASH_MODE_BIT_RESET                             0xff
#define FLASH_RELEASE_POWER_DOWN                         0xab
#define FLASH_MANUFACTURER_ID                            0x90
#define FLASH_READ_UNIQUE_ID                             0x4b
#define FLASH_JEDEC_ID                                   0x9f
#define FLASH_READ_DATA                                  0x03
#define FLASH_FAST_READ                                  0x0b
#define FLASH_FAST_READ_DUAL_OUTPUT                      0x3b
#define FLASH_READ_DUAL_IO                               0xbb
#define FLASH_FAST_READ_QUAD_OUTPUT                      0x6b
#define FLASH_READ_QUAD_IO                               0xeb

/* REGISTERS ------------------------------------------------------------------*/
#define STATUS_REG1_BUSY_MASK                            0x01   // 1 = device is busy with a write/erase cycle
#define STATUS_REG1_WEL_MASK                             0x02   // 1 = device is write enabled
#define STATUS_REG1_BP_MASK                              0x1c   // Block Protect bits
#define STATUS_REG1_TB_MASK                              0x20   // 1 = Top/Bottom bit
#define STATUS_REG1_SEC_MASK                             0x40   // 1 = Sector/Block Protect bit
#define STATUS_REG1_SRP1_MASK                            0x80   // 1 = Status Register Protect1 bit  
#define STATUS_REG2_SRP2_MASK                            0x01   // 1 = Status Register Protect2 bit
#define STATUS_REG2_QE_MASK                              0x02   // 1 = Quad Enable bit




/* Structures -----------------------------------------------------------------*/


/* Functions -----------------------------------------------------------------*/
void flash_wq25xx_init(void);
void flash_wq25xx_deinit(void);
void flash_wq25xx_setMode(uint8_t mode);
void flash_wq25xx_Cancel_Job(void);
void flash_wq25xx_Control_Power(bool enable);

uint8_t flash_wq25xx_get_status(void);
uint8_t flash_wq25xx_get_Job_Result(void);




void flash_wq25xx_write_enable(void);
void flash_wq25xx_write_disable(void);
void flash_wq25xx_sector_erase(uint32_t address);
void flash_wq25xx_chip_erase(void);
void flash_wq25xx_erase_suspend(void);
void flash_wq25xx_erase_resume(void);
void flash_wq25xx_power_down(void);
void flash_wq25xx_high_performance_mode(void);
void flash_wq25xx_mode_bit_reset(void);
void flash_wq25xx_release_power_down(void);
uint8_t flash_wq25xx_read_manufacturer_id(void);
uint8_t flash_wq25xx_read_jedec_id(void);
void flash_wq25xx_read_unique_id(uint8_t *id);
void flash_wq25xx_read_data(uint32_t address, uint8_t *data, uint16_t length);
void flash_wq25xx_fast_read(uint32_t address, uint8_t *data, uint16_t length);
void flash_wq25xx_fast_read_dual_output(uint32_t address, uint8_t *data, uint16_t length);
void flash_wq25xx_read_dual_io(uint32_t address, uint8_t *data, uint16_t length);
void flash_wq25xx_fast_read_quad_output(uint32_t address, uint8_t *data, uint16_t length);
void flash_wq25xx_read_quad_io(uint32_t address, uint8_t *data, uint16_t length);




#endif // /*__FLASH_WQ25XX_H */