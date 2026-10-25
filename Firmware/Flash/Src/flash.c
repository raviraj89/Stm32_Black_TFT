/**
 ******************************************************************************
 * @file flash.c
 * @brief Generic asynchronous flash API and worker.
 ******************************************************************************
 */
#include "flash.h"
#include "flash_backend.h"
#include "cmsis_os.h"
#include <stdbool.h>
#include <string.h>

#define FLASH_WORKER_REQUEST_FLAG  0x01U
#define FLASH_COMPARE_BUFFER_SIZE  256U
#define FLASH_SECTOR_BUFFER_SIZE   4096U

/**
 * @brief Operations accepted by the flash worker.
 */
typedef enum {
    FLS_REQUEST_READ,
    FLS_REQUEST_WRITE,
    FLS_REQUEST_ERASE,
    FLS_REQUEST_COMPARE,
    FLS_REQUEST_BLANK_CHECK,
    FLS_REQUEST_ERASE_SECTOR,
    FLS_REQUEST_ERASE_CHIP
} Fls_RequestTypeDef;

/**
 * @brief Snapshot of one queued operation.
 */
typedef struct {
    Fls_RequestTypeDef Type;
    uint32_t Address;
    uint8_t *Data;
    uint16_t Length;
} Fls_RequestType;

Fls_ConfigTypeDef Fls_Config;

/** @brief Scratch space used to preserve one complete sector during writes. */
static uint8_t fls_SectorBuffer[FLASH_SECTOR_BUFFER_SIZE];
/** @brief Bounded scratch buffer used by compare and blank-check operations. */
static uint8_t fls_CompareBuffer[FLASH_COMPARE_BUFFER_SIZE];
/** @brief Single pending request; access is protected by fls_StateMutex. */
static Fls_RequestType fls_Request;
static osMutexId_t fls_StateMutex;
static osThreadId_t fls_WorkerThread;

static void fls_WorkerTask(void *argument);
static STD_ReturnTypeDef fls_SubmitJob(Fls_RequestTypeDef type, uint32_t address,
                                       uint8_t *data, uint16_t length);
static STD_ReturnTypeDef fls_ExecuteRequest(const Fls_RequestType *request);

/**
 * @brief Validate a byte range against the detected device address window.
 * @param address First address in the range.
 * @param length Number of bytes in the range.
 * @return true when the entire range is valid, including an empty range at
 * the end address boundary.
 */
static bool fls_IsRangeValid(uint32_t address, uint32_t length)
{
    uint32_t offset;

    if (address < Fls_Config.StartAddress) {
        return false;
    }
    offset = address - Fls_Config.StartAddress;
    return offset <= Fls_Config.FlashSize &&
           length <= Fls_Config.FlashSize - offset;
}

/**
 * @brief Publish one request to the worker.
 * @param type Operation to execute.
 * @param address First flash address used by the operation.
 * @param data Caller-owned buffer, if required by the operation.
 * @param length Number of bytes used by the operation.
 * @return E_OK if accepted; otherwise E_NOT_OK.
 * @note The mutex prevents concurrent submissions and protects the request
 * snapshot. The caller's buffer is not copied and must remain valid.
 */
static STD_ReturnTypeDef fls_SubmitJob(Fls_RequestTypeDef type, uint32_t address,
                                       uint8_t *data, uint16_t length)
{
    uint32_t flags;

    if (fls_StateMutex == NULL || fls_WorkerThread == NULL ||
        osMutexAcquire(fls_StateMutex, 0U) != osOK) {
        return E_NOT_OK;
    }
    if (Fls_Config.State != FLS_IDLE) {
        (void)osMutexRelease(fls_StateMutex);
        return E_NOT_OK;
    }

    fls_Request.Type = type;
    fls_Request.Address = address;
    fls_Request.Data = data;
    fls_Request.Length = length;
    Fls_Config.State = FLS_BUSY;
    Fls_Config.JobStatus = FLS_JOB_PENDING;

    flags = osThreadFlagsSet(fls_WorkerThread, FLASH_WORKER_REQUEST_FLAG);
    if ((int32_t)flags < 0) {
        Fls_Config.State = FLS_IDLE;
        Fls_Config.JobStatus = FLS_JOB_FAILED;
        (void)osMutexRelease(fls_StateMutex);
        return E_NOT_OK;
    }

    (void)osMutexRelease(fls_StateMutex);
    return E_OK;
}

/** @copydoc fls_Init */
STD_ReturnTypeDef fls_Init(void)
{
    memset(&Fls_Config, 0, sizeof(Fls_Config));
    Fls_Config.State = FLS_UNINITIALIZED;
    Fls_Config.JobStatus = FLS_JOB_FAILED;
    Fls_Config.Mode = NORMAL_MODE;

    if (FlashBackend.Init == NULL ||
        FlashBackend.Init(&Fls_Config) != E_OK) {
        return E_NOT_OK;
    }
    if (Fls_Config.SectorSize == 0U ||
        Fls_Config.SectorSize > sizeof(fls_SectorBuffer) ||
        Fls_Config.PageSize == 0U ||
        Fls_Config.SectorSize % Fls_Config.PageSize != 0U) {
        Fls_Config.State = FLS_UNINITIALIZED;
        return E_NOT_OK;
    }

    Fls_Config.State = FLS_IDLE;
    Fls_Config.JobStatus = FLS_JOB_OK;
    return E_OK;
}

/** @copydoc fls_StartWorker */
STD_ReturnTypeDef fls_StartWorker(void)
{
    static const osThreadAttr_t workerAttributes = {
        .name = "FlashWorker",
        .stack_size = 1024U,
        .priority = osPriorityNormal
    };

    if (osKernelGetState() != osKernelReady ||
        Fls_Config.State != FLS_IDLE ||
        fls_WorkerThread != NULL) {
        return E_NOT_OK;
    }

    fls_StateMutex = osMutexNew(NULL);
    if (fls_StateMutex == NULL) {
        return E_NOT_OK;
    }

    fls_WorkerThread = osThreadNew(fls_WorkerTask, NULL, &workerAttributes);
    if (fls_WorkerThread == NULL) {
        (void)osMutexDelete(fls_StateMutex);
        fls_StateMutex = NULL;
        return E_NOT_OK;
    }
    return E_OK;
}

/** @copydoc fls_GetStatus */
FLS_StatusTypeDef fls_GetStatus(void)
{
    FLS_StatusTypeDef state;

    if (fls_StateMutex == NULL) {
        return Fls_Config.State;
    }
    if (osMutexAcquire(fls_StateMutex, osWaitForever) != osOK) {
        return FLS_BUSY;
    }
    state = Fls_Config.State;
    (void)osMutexRelease(fls_StateMutex);
    return state;
}

/** @copydoc fls_GetJobResult */
FLS_JobStatusTypeDef fls_GetJobResult(void)
{
    FLS_JobStatusTypeDef result;

    if (fls_StateMutex == NULL) {
        return Fls_Config.JobStatus;
    }
    if (osMutexAcquire(fls_StateMutex, osWaitForever) != osOK) {
        return FLS_JOB_FAILED;
    }
    result = Fls_Config.JobStatus;
    (void)osMutexRelease(fls_StateMutex);
    return result;
}

/** @copydoc fls_GetVersionInfo */
STD_ReturnTypeDef fls_GetVersionInfo(Fls_VersionInfoTypeDef *versionInfo)
{
    if (versionInfo == NULL || Fls_Config.State == FLS_UNINITIALIZED) {
        return E_NOT_OK;
    }

    versionInfo->vendorID = Fls_Config.FlashInfo.ManufacturerID;
    versionInfo->moduleID = Fls_Config.FlashInfo.MemoryType;
    versionInfo->module_Size = (uint16_t)(Fls_Config.FlashSize / 1024U);
    versionInfo->sw_major_version = 1U;
    versionInfo->sw_minor_version = 0U;
    versionInfo->sw_patch_version = 0U;
    return E_OK;
}

/** @copydoc fls_setMode */
STD_ReturnTypeDef fls_setMode(uint8_t mode)
{
    if (mode > QUAD_MODE || Fls_Config.State == FLS_UNINITIALIZED ||
        FlashBackend.SetMode == NULL) {
        return E_NOT_OK;
    }
    if (fls_StateMutex != NULL &&
        osMutexAcquire(fls_StateMutex, 0U) != osOK) {
        return E_NOT_OK;
    }
    if (Fls_Config.State != FLS_IDLE ||
        FlashBackend.SetMode(mode) != E_OK) {
        if (fls_StateMutex != NULL) {
            (void)osMutexRelease(fls_StateMutex);
        }
        return E_NOT_OK;
    }
    Fls_Config.Mode = (FLS_ModeTypeDef)mode;
    if (fls_StateMutex != NULL) {
        (void)osMutexRelease(fls_StateMutex);
    }
    return E_OK;
}

/** @copydoc fls_Cancel_Job */
STD_ReturnTypeDef fls_Cancel_Job(void)
{
    /* Accepted requests run to completion and cannot be safely interrupted. */
    return E_NOT_OK;
}

/** @copydoc fls_Read */
STD_ReturnTypeDef fls_Read(uint32_t address, uint8_t *data, uint16_t length)
{
    if ((length > 0U && data == NULL) || !fls_IsRangeValid(address, length)) {
        return E_NOT_OK;
    }
    return fls_SubmitJob(FLS_REQUEST_READ, address, data, length);
}

/** @copydoc fls_Write */
STD_ReturnTypeDef fls_Write(uint32_t address, uint8_t *data, uint16_t length)
{
    if ((length > 0U && data == NULL) || !fls_IsRangeValid(address, length)) {
        return E_NOT_OK;
    }
    return fls_SubmitJob(FLS_REQUEST_WRITE, address, data, length);
}

/** @copydoc fls_Erase */
STD_ReturnTypeDef fls_Erase(uint32_t address, uint16_t length)
{
    if (!fls_IsRangeValid(address, length)) {
        return E_NOT_OK;
    }
    return fls_SubmitJob(FLS_REQUEST_ERASE, address, NULL, length);
}

/** @copydoc fls_Compare */
STD_ReturnTypeDef fls_Compare(uint32_t address, uint8_t *data, uint16_t length)
{
    if ((length > 0U && data == NULL) || !fls_IsRangeValid(address, length)) {
        return E_NOT_OK;
    }
    return fls_SubmitJob(FLS_REQUEST_COMPARE, address, data, length);
}

/** @copydoc fls_BlankCheck */
STD_ReturnTypeDef fls_BlankCheck(uint32_t address, uint16_t length)
{
    if (!fls_IsRangeValid(address, length)) {
        return E_NOT_OK;
    }
    return fls_SubmitJob(FLS_REQUEST_BLANK_CHECK, address, NULL, length);
}

/** @copydoc fls_Erase_Sector */
STD_ReturnTypeDef fls_Erase_Sector(uint32_t address)
{
    if (!fls_IsRangeValid(address, 1U)) {
        return E_NOT_OK;
    }
    return fls_SubmitJob(FLS_REQUEST_ERASE_SECTOR, address, NULL, 0U);
}

/** @copydoc fls_Erase_Chip */
STD_ReturnTypeDef fls_Erase_Chip(void)
{
    return fls_SubmitJob(FLS_REQUEST_ERASE_CHIP, 0U, NULL, 0U);
}

/**
 * @brief Synchronously read bytes through the selected backend.
 * @param address First flash address.
 * @param data Destination buffer.
 * @param length Number of bytes to read.
 * @return Backend operation result.
 */
static STD_ReturnTypeDef fls_ReadSync(uint32_t address, uint8_t *data,
                                      uint16_t length)
{
    return FlashBackend.Read(address, data, length);
}

/**
 * @brief Execute a write while preserving all other data in touched sectors.
 * @param address First address to replace.
 * @param data Replacement bytes.
 * @param length Number of bytes to write.
 * @return E_OK on complete success; otherwise E_NOT_OK.
 * @note Each affected sector is read into RAM, patched, erased, then
 * reprogrammed page-by-page. Fully erased pages are skipped.
 */
static STD_ReturnTypeDef fls_WriteSync(uint32_t address, uint8_t *data,
                                       uint16_t length)
{
    uint32_t currentAddress = address;
    uint32_t requestEnd = address + length;
    uint16_t remaining = length;

    while (remaining > 0U) {
        uint32_t sectorAddress =
            currentAddress - (currentAddress % Fls_Config.SectorSize);
        uint32_t sectorEnd = sectorAddress + Fls_Config.SectorSize;
        uint32_t copyEnd = requestEnd < sectorEnd ? requestEnd : sectorEnd;
        uint16_t sectorOffset = (uint16_t)(currentAddress - sectorAddress);
        uint16_t copyLength = (uint16_t)(copyEnd - currentAddress);
        uint16_t dataOffset = (uint16_t)(currentAddress - address);

        if (FlashBackend.Read(sectorAddress, fls_SectorBuffer,
                              (uint16_t)Fls_Config.SectorSize) != E_OK) {
            return E_NOT_OK;
        }
        memcpy(&fls_SectorBuffer[sectorOffset], &data[dataOffset], copyLength);

        if (FlashBackend.EraseSector(sectorAddress) != E_OK) {
            return E_NOT_OK;
        }

        for (uint16_t pageOffset = 0U;
             pageOffset < Fls_Config.SectorSize;
             pageOffset = (uint16_t)(pageOffset + Fls_Config.PageSize)) {
            bool erased = true;
            for (uint16_t i = 0U; i < Fls_Config.PageSize; i++) {
                if (fls_SectorBuffer[pageOffset + i] != 0xFFU) {
                    erased = false;
                    break;
                }
            }
            if (!erased &&
                FlashBackend.ProgramPage(sectorAddress + pageOffset,
                                         &fls_SectorBuffer[pageOffset],
                                         (uint16_t)Fls_Config.PageSize) != E_OK) {
                return E_NOT_OK;
            }
        }

        currentAddress = copyEnd;
        remaining = (uint16_t)(requestEnd - currentAddress);
    }
    return E_OK;
}

/**
 * @brief Erase each sector intersecting a byte range.
 * @param address First address in the requested range.
 * @param length Number of bytes in the requested range.
 * @return E_OK on success; otherwise E_NOT_OK.
 */
static STD_ReturnTypeDef fls_EraseSync(uint32_t address, uint16_t length)
{
    uint32_t sectorAddress;
    uint32_t endAddress;

    if (length == 0U) {
        return E_OK;
    }
    sectorAddress = address - (address % Fls_Config.SectorSize);
    endAddress = address + length;
    while (sectorAddress < endAddress) {
        if (FlashBackend.EraseSector(sectorAddress) != E_OK) {
            return E_NOT_OK;
        }
        sectorAddress += Fls_Config.SectorSize;
    }
    return E_OK;
}

/**
 * @brief Compare flash data against a caller-provided buffer in bounded chunks.
 * @param address First address to compare.
 * @param data Expected bytes.
 * @param length Number of bytes to compare.
 * @return E_OK if all bytes match; otherwise E_NOT_OK.
 */
static STD_ReturnTypeDef fls_CompareSync(uint32_t address, uint8_t *data,
                                         uint16_t length)
{
    uint16_t offset = 0U;

    while (offset < length) {
        uint16_t chunk = (uint16_t)(length - offset);
        if (chunk > sizeof(fls_CompareBuffer)) {
            chunk = sizeof(fls_CompareBuffer);
        }
        if (FlashBackend.Read(address + offset, fls_CompareBuffer, chunk) != E_OK ||
            memcmp(fls_CompareBuffer, &data[offset], chunk) != 0) {
            return E_NOT_OK;
        }
        offset = (uint16_t)(offset + chunk);
    }
    return E_OK;
}

/**
 * @brief Check a flash range for the erased value 0xFF.
 * @param address First address to check.
 * @param length Number of bytes to check.
 * @return E_OK if every byte is erased; otherwise E_NOT_OK.
 */
static STD_ReturnTypeDef fls_BlankCheckSync(uint32_t address, uint16_t length)
{
    uint16_t offset = 0U;

    while (offset < length) {
        uint16_t chunk = (uint16_t)(length - offset);
        if (chunk > sizeof(fls_CompareBuffer)) {
            chunk = sizeof(fls_CompareBuffer);
        }
        if (FlashBackend.Read(address + offset, fls_CompareBuffer, chunk) != E_OK) {
            return E_NOT_OK;
        }
        for (uint16_t i = 0U; i < chunk; i++) {
            if (fls_CompareBuffer[i] != 0xFFU) {
                return E_NOT_OK;
            }
        }
        offset = (uint16_t)(offset + chunk);
    }
    return E_OK;
}

/**
 * @brief Dispatch one queued request to its synchronous implementation.
 * @param request Immutable snapshot of the pending worker request.
 * @return E_OK on success; otherwise E_NOT_OK.
 */
static STD_ReturnTypeDef fls_ExecuteRequest(const Fls_RequestType *request)
{
    switch (request->Type) {
    case FLS_REQUEST_READ:
        return fls_ReadSync(request->Address, request->Data, request->Length);
    case FLS_REQUEST_WRITE:
        return fls_WriteSync(request->Address, request->Data, request->Length);
    case FLS_REQUEST_ERASE:
        return fls_EraseSync(request->Address, request->Length);
    case FLS_REQUEST_COMPARE:
        return fls_CompareSync(request->Address, request->Data, request->Length);
    case FLS_REQUEST_BLANK_CHECK:
        return fls_BlankCheckSync(request->Address, request->Length);
    case FLS_REQUEST_ERASE_SECTOR:
        return FlashBackend.EraseSector(request->Address);
    case FLS_REQUEST_ERASE_CHIP:
        return FlashBackend.EraseChip();
    default:
        return E_NOT_OK;
    }
}

/**
 * @brief Wait for queued operations, execute them, and publish their result.
 * @param argument Unused CMSIS-RTOS thread argument.
 *
 * The worker copies the pending request while holding the state mutex, then
 * performs blocking backend I/O without holding that mutex. It reacquires the
 * mutex to publish the result, allowing status queries during flash activity.
 */
static void fls_WorkerTask(void *argument)
{
    (void)argument;

    for (;;) {
        uint32_t flags = osThreadFlagsWait(FLASH_WORKER_REQUEST_FLAG,
                                           osFlagsWaitAny,
                                           osWaitForever);
        Fls_RequestType request;
        STD_ReturnTypeDef result;

        if ((int32_t)flags < 0) {
            osDelay(1U);
            continue;
        }
        if (osMutexAcquire(fls_StateMutex, osWaitForever) != osOK) {
            Fls_Config.State = FLS_IDLE;
            Fls_Config.JobStatus = FLS_JOB_FAILED;
            continue;
        }
        request = fls_Request;
        (void)osMutexRelease(fls_StateMutex);

        result = fls_ExecuteRequest(&request);

        if (osMutexAcquire(fls_StateMutex, osWaitForever) == osOK) {
            Fls_Config.State = FLS_IDLE;
            Fls_Config.JobStatus = result == E_OK ? FLS_JOB_OK : FLS_JOB_FAILED;
            (void)osMutexRelease(fls_StateMutex);
        } else {
            Fls_Config.State = FLS_IDLE;
            Fls_Config.JobStatus = FLS_JOB_FAILED;
        }
    }
}
