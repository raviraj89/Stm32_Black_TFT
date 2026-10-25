/**
 ******************************************************************************
 * File Name        : flash_wq25xx.h
 * Description      : SPI interface for W25Qxx serial flash devices
 ******************************************************************************
 */
#ifndef FLASH_WQ25XX_H
#define FLASH_WQ25XX_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"

#define SPI_FLASH_CS_LOW()  HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET)
#define SPI_FLASH_CS_HIGH() HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET)
#define SPI_MODE_USED       0

#define FLASH_W25QXX_TIMEOUT        1000U
#define FLASH_W25QXX_PAGE_SIZE      256U
#define FLASH_W25QXX_SECTOR_SIZE    4096U
#define FLASH_W25QXX_BLOCK_SIZE     65536U
#define FLASH_W25QXX_FLASH_SIZE     2097152U
#define FLASH_W25QXX_FLASH_SIZE_KB  2048U
#define FLASH_W25QXX_FLASH_SIZE_MB  2U
#define FLASH_W25QXX_START_ADDRESS  0x000000U
#define FLASH_W25QXX_END_ADDRESS    0x1FFFFFU
#define FLASH_STATE_IDLE            0U
#define FLASH_STATE_BUSY            1U

#define NB_LED 2
#define LED2   0
#define LED3   1
#define NB_KEY 2
#define KEY0   0
#define KEY1   1

#define FLASH_WRITE_ENABLE             0x06U
#define FLASH_WRITE_DISABLE            0x04U
#define FLASH_READ_STATUS_REG1         0x05U
#define FLASH_READ_STATUS_REG2         0x35U
#define FLASH_WRT_STATUS_PROG          0x01U
#define FLASH_PAGE_PROGRAM             0x02U
#define FLASH_QUAD_PAGE_PROGRAM        0x32U
#define FLASH_BLK_ERASE_64             0xD8U
#define FLASH_BLK_ERASE_32             0x52U
#define FLASH_SECTOR_ERASE             0x20U
#define FLASH_CHIP_ERASE               0xC7U
#define FLASH_ERASE_SUSPEND            0x75U
#define FLASH_ERASE_RESUME              0x7AU
#define FLASH_POWER_DOWN               0xB9U
#define FLASH_HIGH_PERFORMANCE_MODE    0xA3U
#define FLASH_MODE_BIT_RESET           0xFFU
#define FLASH_RELEASE_POWER_DOWN       0xABU
#define FLASH_MANUFACTURER_ID          0x90U
#define FLASH_READ_UNIQUE_ID            0x4BU
#define FLASH_JEDEC_ID                  0x9FU
#define FLASH_READ_DATA                 0x03U
#define FLASH_FAST_READ                 0x0BU
#define FLASH_FAST_READ_DUAL_OUTPUT     0x3BU
#define FLASH_READ_DUAL_IO              0xBBU
#define FLASH_FAST_READ_QUAD_OUTPUT     0x6BU
#define FLASH_READ_QUAD_IO              0xEBU

#define STATUS_REG1_BUSY_MASK 0x01U
#define STATUS_REG1_WEL_MASK  0x02U
#define STATUS_REG1_BP_MASK   0x1CU
#define STATUS_REG1_TB_MASK   0x20U
#define STATUS_REG1_SEC_MASK  0x40U
#define STATUS_REG1_SRP1_MASK 0x80U
#define STATUS_REG2_SRP2_MASK 0x01U
#define STATUS_REG2_QE_MASK   0x02U

typedef enum {
    E_OK = 0,
    E_NOT_OK = 1
} STD_ReturnTypeDef;

typedef enum {
    FLS_UNINITIALIZED = 0,
    FLS_IDLE = 1,
    FLS_BUSY = 2
} FLS_StatusTypeDef;

typedef enum {
    FLS_JOB_OK = 0,
    FLS_JOB_FAILED = 1,
    FLS_JOB_PENDING = 2
} FLS_JobStatusTypeDef;

typedef enum {
    NORMAL_MODE = 0,
    QUAD_MODE = 1
} FLS_ModeTypeDef;

typedef struct {
    uint32_t FlashSize;
    uint32_t PageSize;
    uint32_t SectorSize;
    uint32_t BlockSize;
    uint32_t StartAddress;
    uint32_t EndAddress;
    FLS_StatusTypeDef State;
    FLS_JobStatusTypeDef JobStatus;
    FLS_ModeTypeDef Mode;
    struct {
        uint8_t ManufacturerID;
        uint16_t MemoryType;
        uint8_t Capacity;
    } FlashInfo;
} Fls_ConfigTypeDef;

typedef struct {
    uint8_t vendorID;
    uint16_t moduleID;
    uint16_t module_Size;
    uint8_t sw_major_version;
    uint8_t sw_minor_version;
    uint8_t sw_patch_version;
} Fls_VersionInfoTypeDef;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Call job-submission and status APIs from an RTOS task, not an ISR. High-level
 * read/write/erase/check jobs run on the worker; only one job may be active.
 * Direct fls_wq25xx command helpers perform synchronous SPI transfers and must
 * not be used concurrently with a worker job.
 */
/** Initialize the driver and confirm that the connected device is a W25Q16. */
STD_ReturnTypeDef fls_Init(void);
/** Create the FreeRTOS flash worker after osKernelInitialize() and fls_Init(). */
STD_ReturnTypeDef fls_StartWorker(void);
/** Select normal SPI or quad mode; quad mode is unsupported by this SPI1 setup. */
STD_ReturnTypeDef fls_setMode(uint8_t mode);
/** Cancel the active flash operation; accepted jobs cannot be cancelled. */
STD_ReturnTypeDef fls_Cancel_Job(void);
/** Return FLS_BUSY while a worker job is in progress, otherwise the driver state. */
FLS_StatusTypeDef fls_GetStatus(void);
/** Return the result of the most recently completed flash job. */
FLS_JobStatusTypeDef fls_GetJobResult(void);
/** Copy device and driver version information to versionInfo. */
STD_ReturnTypeDef fls_GetVersionInfo(Fls_VersionInfoTypeDef *versionInfo);

/**
 * Queue a read. E_OK means accepted; poll fls_GetJobResult() for completion.
 * The destination buffer must remain valid until the job completes.
 * Only one job can be active; submissions while busy return E_NOT_OK.
 */
STD_ReturnTypeDef fls_Read(uint32_t address, uint8_t *data, uint16_t length);
/**
 * Queue a preserving write. E_OK means accepted; poll fls_GetJobResult().
 * The source buffer must remain valid until the job completes.
 * Only one job can be active; submissions while busy return E_NOT_OK.
 */
STD_ReturnTypeDef fls_Write(uint32_t address, uint8_t *data, uint16_t length);
/**
 * Queue erase of every 4 KiB sector intersecting the range; surrounding data is lost.
 * E_OK means accepted; poll fls_GetJobResult() for completion.
 * Only one job can be active; submissions while busy return E_NOT_OK.
 */
STD_ReturnTypeDef fls_Erase(uint32_t address, uint16_t length);
/** Queue comparison; data must remain valid until completion. Only one job can be active. */
STD_ReturnTypeDef fls_Compare(uint32_t address, uint8_t *data, uint16_t length);
/** Queue a blank check; inspect fls_GetJobResult() when it finishes. Only one job can be active. */
STD_ReturnTypeDef fls_BlankCheck(uint32_t address, uint16_t length);

/** Queue erase of the sector containing address. */
STD_ReturnTypeDef fls_Erase_Sector(uint32_t address);
/** Queue a whole-chip erase. */
STD_ReturnTypeDef fls_Erase_Chip(void);
/** Suspend an active erase operation. */
STD_ReturnTypeDef fls_Erase_Suspend(void);
/** Resume a suspended erase operation. */
STD_ReturnTypeDef fls_Erase_Resume(void);
/** Enter deep power-down mode. */
STD_ReturnTypeDef fls_PowerDown(void);
/** Release deep power-down mode. */
STD_ReturnTypeDef fls_ReleasePowerDown(void);

/** Synchronous JEDEC read; call only while no worker job is active. */
STD_ReturnTypeDef fls_wq25xx_Read_JEDEC_ID(void);
/** Synchronous status read; an SPI error is reported as BUSY. */
uint8_t fls_wq25xx_Read_Status_Register1(void);
/** Synchronous status read; an SPI error is reported as 0xFF. */
uint8_t fls_wq25xx_Read_Status_Register2(void);
/** Write both status registers after enabling writes. */
STD_ReturnTypeDef fls_wq25xx_Write_Status_Register(uint8_t reg1, uint8_t reg2);
/** Issue a sector erase at address; does not wait for erase completion. */
STD_ReturnTypeDef fls_wq25xx_Sector_Erase(uint32_t address);
/** Set the write-enable latch and confirm it was set. */
STD_ReturnTypeDef fls_wq25xx_Write_Enable(void);
/** Clear the write-enable latch. */
STD_ReturnTypeDef fls_wq25xx_Write_Disable(void);
/** Synchronous low-level read; do not call while a worker job is active. */
STD_ReturnTypeDef fls_wq25xx_Read_Data(uint32_t address, uint8_t *data, uint16_t length);
/** Queue the preserving high-level write operation. */
STD_ReturnTypeDef fls_wq25xx_Write_Data(uint32_t address, uint8_t *data, uint16_t length);
/** Read the device's 8-byte unique identifier. */
STD_ReturnTypeDef fls_wq25xx_Read_Unique_ID(uint8_t *unique_id);
/** Select standard single-line SPI operation. */
STD_ReturnTypeDef fls_SetNormalMode(void);
/** Quad SPI mode is not supported by this SPI1 driver. */
STD_ReturnTypeDef fls_SetQuadMode(void);
/** Check whether the device write-enable latch is set. */
STD_ReturnTypeDef fls_wq25xx_Is_Writable(void);

/** Compatibility callback; blocking SPI operations do not use HAL callbacks. */
void fls_SPI_TransmitCompleteCallback(void);
/** Compatibility callback; blocking SPI operations do not use HAL callbacks. */
void fls_SPI_ReceiveCompleteCallback(void);

#ifdef __cplusplus
}
#endif

#endif
