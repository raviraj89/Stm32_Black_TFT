/**
 * @file wq25xx.h
 * @brief W25Qxx geometry, command definitions, and device-specific API.
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

/**
 * @brief Synchronous W25Qxx-specific commands for diagnostics and control.
 * @warning Do not call while a generic asynchronous flash job is active.
 */
/**
 * @brief Read and cache the device JEDEC identifier.
 * @return E_OK if the identifier was read; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_wq25xx_Read_JEDEC_ID(void);
/**
 * @brief Read status register 1.
 * @return Register value; returns the BUSY bit set if the SPI read fails.
 */
uint8_t fls_wq25xx_Read_Status_Register1(void);
/**
 * @brief Read status register 2.
 * @return Register value; returns 0xFF if the SPI read fails.
 */
uint8_t fls_wq25xx_Read_Status_Register2(void);
/**
 * @brief Write status registers and wait for the device to become ready.
 * @param reg1 Value to write to status register 1.
 * @param reg2 Value to write to status register 2.
 * @return E_OK on success; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_wq25xx_Write_Status_Register(uint8_t reg1, uint8_t reg2);
/**
 * @brief Erase the sector containing an address.
 * @param address Valid flash address within the target sector.
 * @return E_OK on success; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_wq25xx_Sector_Erase(uint32_t address);
/**
 * @brief Set and verify the write-enable latch.
 * @return E_OK if the latch is set; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_wq25xx_Write_Enable(void);
/**
 * @brief Clear the write-enable latch.
 * @return E_OK if the command was sent; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_wq25xx_Write_Disable(void);
/**
 * @brief Read bytes directly through the W25Qxx SPI command set.
 * @param address First flash address to read.
 * @param data Destination buffer.
 * @param length Number of bytes to read.
 * @return E_OK on success; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_wq25xx_Read_Data(uint32_t address, uint8_t *data, uint16_t length);
/**
 * @brief Queue a sector-preserving write through the generic flash layer.
 * @param address First flash address to write.
 * @param data Source buffer, which must remain valid until job completion.
 * @param length Number of bytes to write.
 * @return E_OK if accepted; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_wq25xx_Write_Data(uint32_t address, uint8_t *data, uint16_t length);
/**
 * @brief Read the device's eight-byte unique identifier.
 * @param unique_id Destination buffer with room for at least eight bytes.
 * @return E_OK on success; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_wq25xx_Read_Unique_ID(uint8_t *unique_id);
/**
 * @name Synchronous erase-control and power-management commands
 * @brief These commands access the device directly.
 * @warning Do not call while a generic worker job is active.
 * @{
 */
/**
 * @brief Suspend an active erase operation.
 * @return E_OK if the command was sent; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_Erase_Suspend(void);
/**
 * @brief Resume a suspended erase operation.
 * @return E_OK if the command was sent; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_Erase_Resume(void);
/**
 * @brief Enter device power-down mode.
 * @return E_OK if the command was sent; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_PowerDown(void);
/**
 * @brief Release power-down mode and return to standby.
 * @return E_OK if the command was sent; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_ReleasePowerDown(void);
/** @} */
/**
 * @brief Select standard SPI mode.
 * @return E_OK on success; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_SetNormalMode(void);
/**
 * @brief Request quad mode.
 * @return E_NOT_OK because this backend does not support quad mode.
 */
STD_ReturnTypeDef fls_SetQuadMode(void);
/**
 * @brief Check whether the W25Qxx write-enable latch is set.
 * @return E_OK if writable; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_wq25xx_Is_Writable(void);
/**
 * @brief Compatibility callbacks retained for legacy callers.
 * @note The current worker uses blocking HAL SPI transfers.
 */
/** @brief Legacy transmit-completion callback; no operation is performed. */
void fls_SPI_TransmitCompleteCallback(void);
/** @brief Legacy receive-completion callback; no operation is performed. */
void fls_SPI_ReceiveCompleteCallback(void);

#ifdef __cplusplus
}
#endif

#endif
