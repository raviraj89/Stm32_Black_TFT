/**
 ******************************************************************************
 * @file wq25xx.h
 * @brief W25Qxx command set and device-specific operations.
 ******************************************************************************
 */
#ifndef WQ25XX_H
#define WQ25XX_H

#include "flash.h"

#define WQ25XX_PAGE_SIZE       256U
#define WQ25XX_SECTOR_SIZE     4096U
#define WQ25XX_BLOCK_SIZE      65536U
#define WQ25XX_FLASH_SIZE      2097152U
#define WQ25XX_CAPACITY_CODE   0x15U
#define WQ25XX_SPI_TIMEOUT     1000U

#define FLASH_WRITE_ENABLE          0x06U
#define FLASH_WRITE_DISABLE         0x04U
#define FLASH_READ_STATUS_REG1      0x05U
#define FLASH_READ_STATUS_REG2      0x35U
#define FLASH_WRT_STATUS_PROG       0x01U
#define FLASH_PAGE_PROGRAM          0x02U
#define FLASH_QUAD_PAGE_PROGRAM     0x32U
#define FLASH_BLK_ERASE_64          0xD8U
#define FLASH_BLK_ERASE_32          0x52U
#define FLASH_SECTOR_ERASE          0x20U
#define FLASH_CHIP_ERASE            0xC7U
#define FLASH_ERASE_SUSPEND         0x75U
#define FLASH_ERASE_RESUME          0x7AU
#define FLASH_POWER_DOWN            0xB9U
#define FLASH_HIGH_PERFORMANCE_MODE 0xA3U
#define FLASH_MODE_BIT_RESET        0xFFU
#define FLASH_RELEASE_POWER_DOWN    0xABU
#define FLASH_MANUFACTURER_ID       0x90U
#define FLASH_READ_UNIQUE_ID        0x4BU
#define FLASH_JEDEC_ID              0x9FU
#define FLASH_READ_DATA             0x03U
#define FLASH_FAST_READ             0x0BU
#define FLASH_FAST_READ_DUAL_OUTPUT 0x3BU
#define FLASH_READ_DUAL_IO          0xBBU
#define FLASH_FAST_READ_QUAD_OUTPUT 0x6BU
#define FLASH_READ_QUAD_IO          0xEBU

#define STATUS_REG1_BUSY_MASK 0x01U
#define STATUS_REG1_WEL_MASK  0x02U
#define STATUS_REG1_BP_MASK   0x1CU
#define STATUS_REG1_TB_MASK   0x20U
#define STATUS_REG1_SEC_MASK  0x40U
#define STATUS_REG1_SRP1_MASK 0x80U
#define STATUS_REG2_SRP2_MASK 0x01U
#define STATUS_REG2_QE_MASK   0x02U

#define SPI_MODE_USED 0
#define NB_LED 2
#define LED2   0
#define LED3   1
#define NB_KEY 2
#define KEY0   0
#define KEY1   1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Device-specific raw commands perform synchronous SPI transfers. Call them
 * only when no asynchronous generic flash job is active.
 */
/** Read and cache the device JEDEC identifier. */
STD_ReturnTypeDef fls_wq25xx_Read_JEDEC_ID(void);
/** Read status register 1; SPI failures are reported as BUSY. */
uint8_t fls_wq25xx_Read_Status_Register1(void);
/** Read status register 2; SPI failures are reported as 0xFF. */
uint8_t fls_wq25xx_Read_Status_Register2(void);
/** Write the status registers and wait until programming completes. */
STD_ReturnTypeDef fls_wq25xx_Write_Status_Register(uint8_t reg1, uint8_t reg2);
/** Start erasing the sector containing address; does not wait for completion. */
STD_ReturnTypeDef fls_wq25xx_Sector_Erase(uint32_t address);
/** Set and verify the write-enable latch. */
STD_ReturnTypeDef fls_wq25xx_Write_Enable(void);
/** Clear the write-enable latch. */
STD_ReturnTypeDef fls_wq25xx_Write_Disable(void);
/** Read bytes directly through the W25Qxx SPI command set. */
STD_ReturnTypeDef fls_wq25xx_Read_Data(uint32_t address, uint8_t *data, uint16_t length);
/** Queue a generic preserving write. */
STD_ReturnTypeDef fls_wq25xx_Write_Data(uint32_t address, uint8_t *data, uint16_t length);
/** Read the eight-byte W25Qxx unique identifier. */
STD_ReturnTypeDef fls_wq25xx_Read_Unique_ID(uint8_t *unique_id);
/**
 * Send W25Qxx erase suspend/resume and power commands synchronously.
 * Do not use concurrently with a queued worker operation.
 */
STD_ReturnTypeDef fls_Erase_Suspend(void);
STD_ReturnTypeDef fls_Erase_Resume(void);
STD_ReturnTypeDef fls_PowerDown(void);
STD_ReturnTypeDef fls_ReleasePowerDown(void);
/** The device's standard SPI mode is supported; quad mode is not. */
STD_ReturnTypeDef fls_SetNormalMode(void);
STD_ReturnTypeDef fls_SetQuadMode(void);
/** Report whether the W25Qxx write-enable latch is set. */
STD_ReturnTypeDef fls_wq25xx_Is_Writable(void);
/** Retained compatibility hooks; the worker uses blocking HAL transfers. */
void fls_SPI_TransmitCompleteCallback(void);
void fls_SPI_ReceiveCompleteCallback(void);

#ifdef __cplusplus
}
#endif

#endif
