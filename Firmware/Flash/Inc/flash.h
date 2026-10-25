/**
 * @file flash.h
 * @brief Device-independent asynchronous serial-flash API.
 */
#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>

/**
 * @brief Result of a flash API call.
 */
typedef enum {
    E_OK = 0,
    E_NOT_OK = 1
} STD_ReturnTypeDef;

/**
 * @brief State of the flash worker and active job.
 */
typedef enum {
    FLS_UNINITIALIZED = 0,
    FLS_IDLE = 1,
    FLS_BUSY = 2
} FLS_StatusTypeDef;

/**
 * @brief Result of the most recently submitted flash job.
 */
typedef enum {
    FLS_JOB_OK = 0,
    FLS_JOB_FAILED = 1,
    FLS_JOB_PENDING = 2
} FLS_JobStatusTypeDef;

/**
 * @brief Bus mode requested for the selected flash backend.
 */
typedef enum {
    NORMAL_MODE = 0,
    QUAD_MODE = 1
} FLS_ModeTypeDef;

/**
 * @brief Flash geometry, detected device information, and driver state.
 */
typedef struct {
    uint32_t FlashSize;             /**< Total device capacity in bytes. */
    uint32_t PageSize;              /**< Maximum page-program size in bytes. */
    uint32_t SectorSize;            /**< Smallest erasable sector size in bytes. */
    uint32_t BlockSize;             /**< Device block size in bytes. */
    uint32_t StartAddress;          /**< First valid address. */
    uint32_t EndAddress;            /**< Last valid address, inclusive. */
    FLS_StatusTypeDef State;        /**< Current worker state. */
    FLS_JobStatusTypeDef JobStatus; /**< Current or last job result. */
    FLS_ModeTypeDef Mode;           /**< Configured bus mode. */
    struct {
        uint8_t ManufacturerID; /**< JEDEC manufacturer identifier. */
        uint16_t MemoryType;     /**< JEDEC memory type. */
        uint8_t Capacity;        /**< JEDEC capacity code. */
    } FlashInfo;
} Fls_ConfigTypeDef;

/**
 * @brief Flash driver and detected-device version information.
 */
typedef struct {
    uint8_t vendorID;         /**< Vendor identifier. */
    uint16_t moduleID;        /**< Device/module identifier. */
    uint16_t module_Size;     /**< Module size as defined by the implementation. */
    uint8_t sw_major_version; /**< Driver major version. */
    uint8_t sw_minor_version; /**< Driver minor version. */
    uint8_t sw_patch_version; /**< Driver patch version. */
} Fls_VersionInfoTypeDef;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the selected flash backend and detect device geometry.
 * @note Call before starting the RTOS.
 * @return E_OK on success; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_Init(void);
/**
 * @brief Create the asynchronous flash worker task.
 * @note Call after osKernelInitialize() and fls_Init().
 * @return E_OK when the worker is created; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_StartWorker(void);
/**
 * @brief Get the current worker state.
 * @return FLS_UNINITIALIZED, FLS_IDLE, or FLS_BUSY.
 */
FLS_StatusTypeDef fls_GetStatus(void);
/**
 * @brief Get the active or most recently completed job result.
 * @return FLS_JOB_PENDING while a job is active, otherwise its result.
 */
FLS_JobStatusTypeDef fls_GetJobResult(void);
/**
 * @brief Copy detected device and driver version information.
 * @param versionInfo Destination for the version information.
 * @return E_OK on success; E_NOT_OK for a null destination or uninitialized driver.
 */
STD_ReturnTypeDef fls_GetVersionInfo(Fls_VersionInfoTypeDef *versionInfo);
/**
 * @brief Set the selected backend's bus mode.
 * @param mode Requested FLS_ModeTypeDef value.
 * @return E_OK on success; otherwise E_NOT_OK.
 */
STD_ReturnTypeDef fls_setMode(uint8_t mode);
/**
 * @brief Cancel an active job.
 * @note The current implementation does not cancel accepted jobs.
 * @return E_NOT_OK when the operation cannot be cancelled.
 */
STD_ReturnTypeDef fls_Cancel_Job(void);

/**
 * @brief Queue an asynchronous read.
 * @param address First flash address to read.
 * @param data Destination buffer; must remain valid until job completion.
 * @param length Number of bytes to read.
 * @return E_OK if accepted; E_NOT_OK if invalid, unavailable, or busy.
 * @note Only one job may be active. Check fls_GetStatus() and then
 * fls_GetJobResult() to determine completion and outcome.
 */
STD_ReturnTypeDef fls_Read(uint32_t address, uint8_t *data, uint16_t length);
/**
 * @brief Queue an asynchronous sector-preserving write.
 * @param address First flash address to write.
 * @param data Source buffer; must remain valid until job completion.
 * @param length Number of bytes to write.
 * @return E_OK if accepted; E_NOT_OK if invalid, unavailable, or busy.
 * @note Untouched bytes in each affected sector are read, buffered, and
 * rewritten after sector erase. Only one job may be active.
 */
STD_ReturnTypeDef fls_Write(uint32_t address, uint8_t *data, uint16_t length);
/**
 * @brief Queue erase of every sector intersecting a byte range.
 * @param address First address in the range.
 * @param length Number of bytes in the range.
 * @return E_OK if accepted; E_NOT_OK if invalid, unavailable, or busy.
 * @warning This erases entire sectors; data outside the range but within
 * those sectors is also erased.
 */
STD_ReturnTypeDef fls_Erase(uint32_t address, uint16_t length);
/**
 * @brief Queue comparison of flash contents with a caller buffer.
 * @param address First flash address to compare.
 * @param data Expected-data buffer; must remain valid until job completion.
 * @param length Number of bytes to compare.
 * @return E_OK if accepted; E_NOT_OK if invalid, unavailable, or busy.
 * @note Inspect fls_GetJobResult() after completion for the comparison result.
 */
STD_ReturnTypeDef fls_Compare(uint32_t address, uint8_t *data, uint16_t length);
/**
 * @brief Queue a check that every byte in a range is erased (0xFF).
 * @param address First flash address to check.
 * @param length Number of bytes to check.
 * @return E_OK if accepted; E_NOT_OK if invalid, unavailable, or busy.
 * @note Inspect fls_GetJobResult() after completion for the check result.
 */
STD_ReturnTypeDef fls_BlankCheck(uint32_t address, uint16_t length);
/**
 * @brief Queue erase of the sector containing an address.
 * @param address Any valid address within the sector to erase.
 * @return E_OK if accepted; E_NOT_OK if invalid, unavailable, or busy.
 * @warning All data in the selected sector is erased.
 */
STD_ReturnTypeDef fls_Erase_Sector(uint32_t address);
/**
 * @brief Queue erase of the entire flash device.
 * @return E_OK if accepted; E_NOT_OK if unavailable or busy.
 * @warning All data on the device is erased.
 */
STD_ReturnTypeDef fls_Erase_Chip(void);

#ifdef __cplusplus
}
#endif

#endif
