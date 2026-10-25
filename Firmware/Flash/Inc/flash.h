/**
 ******************************************************************************
 * @file flash.h
 * @brief Generic asynchronous serial-flash API.
 ******************************************************************************
 */
#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>

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

/** Initialize the configured flash backend. Call before starting the RTOS. */
STD_ReturnTypeDef fls_Init(void);
/** Create the flash worker after osKernelInitialize() and fls_Init(). */
STD_ReturnTypeDef fls_StartWorker(void);
/** Return the current worker/job state. */
FLS_StatusTypeDef fls_GetStatus(void);
/** Return the current or most recently completed job result. */
FLS_JobStatusTypeDef fls_GetJobResult(void);
/** Copy the initialized device and driver version information. */
STD_ReturnTypeDef fls_GetVersionInfo(Fls_VersionInfoTypeDef *versionInfo);
/** Set the backend mode, if supported. */
STD_ReturnTypeDef fls_setMode(uint8_t mode);
/** Active flash jobs cannot be cancelled once accepted. */
STD_ReturnTypeDef fls_Cancel_Job(void);

/**
 * Queue an asynchronous read. Keep data valid until the job is complete.
 * E_OK means accepted; only one job may be active at a time.
 */
STD_ReturnTypeDef fls_Read(uint32_t address, uint8_t *data, uint16_t length);
/**
 * Queue an asynchronous preserving write. Keep data valid until completion.
 * Preserves bytes outside the write range in each touched sector.
 */
STD_ReturnTypeDef fls_Write(uint32_t address, uint8_t *data, uint16_t length);
/**
 * Queue asynchronous erase of all sectors intersecting the range.
 * Data elsewhere in those sectors is erased.
 */
STD_ReturnTypeDef fls_Erase(uint32_t address, uint16_t length);
/** Queue an asynchronous compare; keep data valid until completion. */
STD_ReturnTypeDef fls_Compare(uint32_t address, uint8_t *data, uint16_t length);
/** Queue an asynchronous check that the range contains only erased bytes. */
STD_ReturnTypeDef fls_BlankCheck(uint32_t address, uint16_t length);
/** Queue erase of the sector containing address. */
STD_ReturnTypeDef fls_Erase_Sector(uint32_t address);
/** Queue whole-chip erase. */
STD_ReturnTypeDef fls_Erase_Chip(void);

#ifdef __cplusplus
}
#endif

#endif
