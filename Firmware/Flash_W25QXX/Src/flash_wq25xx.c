/**
 ******************************************************************************
 * File Name        : flash_wq25xx.c
 * Description      : Blocking SPI driver for 2 MiB W25Q16 serial flash
 ******************************************************************************
 */
#include "flash_wq25xx.h"
#include "main.h"
#include <string.h>

extern SPI_HandleTypeDef hspi1;

Fls_ConfigTypeDef Fls_Config;

#define FLASH_WRITE_TIMEOUT_MS  5000U
#define FLASH_ERASE_TIMEOUT_MS  5000U
#define FLASH_CHIP_TIMEOUT_MS   120000U
#define FLASH_UID_SIZE          8U

/* One sector is buffered at a time for write-time read/modify/erase/rewrite. */
static uint8_t fls_SectorBuffer[FLASH_W25QXX_SECTOR_SIZE];

/* Validate without adding address and length, avoiding unsigned overflow. */
static bool fls_IsRangeValid(uint32_t address, uint32_t length)
{
    return (address <= FLASH_W25QXX_FLASH_SIZE) &&
           (length <= FLASH_W25QXX_FLASH_SIZE - address);
}

/* Use blocking HAL transfers so chip select remains asserted for the full command. */
static STD_ReturnTypeDef fls_Transmit(const uint8_t *data, uint16_t length)
{
    return HAL_SPI_Transmit(&hspi1, (uint8_t *)data, length,
                            FLASH_W25QXX_TIMEOUT) == HAL_OK ? E_OK : E_NOT_OK;
}

/* Receive bytes while the current flash command remains selected. */
static STD_ReturnTypeDef fls_Receive(uint8_t *data, uint16_t length)
{
    return HAL_SPI_Receive(&hspi1, data, length,
                           FLASH_W25QXX_TIMEOUT) == HAL_OK ? E_OK : E_NOT_OK;
}

/* Send a command that has no data or address phase. */
static STD_ReturnTypeDef fls_SendCommand(uint8_t command)
{
    STD_ReturnTypeDef result;

    SPI_FLASH_CS_LOW();
    result = fls_Transmit(&command, 1U);
    SPI_FLASH_CS_HIGH();
    return result;
}

/* Poll BUSY until the device is ready or the operation timeout expires. */
static STD_ReturnTypeDef fls_WaitUntilReady(uint32_t timeout)
{
    uint32_t start = HAL_GetTick();

    while ((uint32_t)(HAL_GetTick() - start) < timeout) {
        if ((fls_wq25xx_Read_Status_Register1() & STATUS_REG1_BUSY_MASK) == 0U) {
            return E_OK;
        }
        HAL_Delay(1U);
    }

    return E_NOT_OK;
}

/* Mark a synchronous public operation as active; reject uninitialized or nested jobs. */
static STD_ReturnTypeDef fls_BeginJob(void)
{
    if (Fls_Config.State == FLS_UNINITIALIZED || Fls_Config.State == FLS_BUSY) {
        return E_NOT_OK;
    }

    Fls_Config.State = FLS_BUSY;
    Fls_Config.JobStatus = FLS_JOB_PENDING;
    return E_OK;
}

/* Publish the final job state and return the operation result. */
static STD_ReturnTypeDef fls_EndJob(STD_ReturnTypeDef result)
{
    Fls_Config.State = FLS_IDLE;
    Fls_Config.JobStatus = result == E_OK ? FLS_JOB_OK : FLS_JOB_FAILED;
    return result;
}

/* Program data in page-bounded chunks; W25Q page programming cannot cross a page. */
static STD_ReturnTypeDef fls_ProgramData(uint32_t address, const uint8_t *data,
                                         uint16_t length)
{
    while (length > 0U) {
        uint16_t pageOffset = (uint16_t)(address % FLASH_W25QXX_PAGE_SIZE);
        uint16_t chunk = (uint16_t)(FLASH_W25QXX_PAGE_SIZE - pageOffset);
        uint8_t command[4] = {
            FLASH_PAGE_PROGRAM,
            (uint8_t)(address >> 16),
            (uint8_t)(address >> 8),
            (uint8_t)address
        };

        if (chunk > length) {
            chunk = length;
        }
        if (fls_wq25xx_Write_Enable() != E_OK) {
            return E_NOT_OK;
        }

        SPI_FLASH_CS_LOW();
        if (fls_Transmit(command, sizeof(command)) != E_OK ||
            fls_Transmit(data, chunk) != E_OK) {
            SPI_FLASH_CS_HIGH();
            return E_NOT_OK;
        }
        SPI_FLASH_CS_HIGH();
        if (fls_WaitUntilReady(FLASH_WRITE_TIMEOUT_MS) != E_OK) {
            return E_NOT_OK;
        }

        address += chunk;
        data += chunk;
        length = (uint16_t)(length - chunk);
    }

    return E_OK;
}

/* Avoid unnecessary page-program commands for erased (all-0xFF) pages. */
static bool fls_IsPageErased(const uint8_t *page)
{
    for (uint16_t i = 0U; i < FLASH_W25QXX_PAGE_SIZE; i++) {
        if (page[i] != 0xFFU) {
            return false;
        }
    }
    return true;
}

/* Configure device geometry and verify the JEDEC capacity identifies a W25Q16. */
STD_ReturnTypeDef fls_Init(void)
{
    Fls_Config.FlashSize = FLASH_W25QXX_FLASH_SIZE;
    Fls_Config.PageSize = FLASH_W25QXX_PAGE_SIZE;
    Fls_Config.SectorSize = FLASH_W25QXX_SECTOR_SIZE;
    Fls_Config.BlockSize = FLASH_W25QXX_BLOCK_SIZE;
    Fls_Config.StartAddress = FLASH_W25QXX_START_ADDRESS;
    Fls_Config.EndAddress = FLASH_W25QXX_END_ADDRESS;
    Fls_Config.State = FLS_UNINITIALIZED;
    Fls_Config.JobStatus = FLS_JOB_FAILED;
    Fls_Config.Mode = NORMAL_MODE;

    if (fls_WaitUntilReady(FLASH_W25QXX_TIMEOUT) != E_OK ||
        fls_wq25xx_Read_JEDEC_ID() != E_OK ||
        Fls_Config.FlashInfo.Capacity != 0x15U) {
        return E_NOT_OK;
    }

    Fls_Config.State = FLS_IDLE;
    Fls_Config.JobStatus = FLS_JOB_OK;
    return E_OK;
}

/* Change the selected operating mode only when the requested mode is supported. */
STD_ReturnTypeDef fls_setMode(uint8_t mode)
{
    STD_ReturnTypeDef result;

    if (mode == NORMAL_MODE) {
        result = fls_SetNormalMode();
    } else if (mode == QUAD_MODE) {
        result = fls_SetQuadMode();
    } else {
        return E_NOT_OK;
    }

    if (result == E_OK) {
        Fls_Config.Mode = (FLS_ModeTypeDef)mode;
    }
    return result;
}

/* Synchronous transfers cannot be interrupted once started. */
STD_ReturnTypeDef fls_Cancel_Job(void)
{
    return E_NOT_OK;
}

/* Return the driver's current initialization or operation state. */
FLS_StatusTypeDef fls_GetStatus(void)
{
    return Fls_Config.State;
}

/* Return the success, failure, or pending result for the last job. */
FLS_JobStatusTypeDef fls_GetJobResult(void)
{
    return Fls_Config.JobStatus;
}

/* Populate caller-provided version information; reject a null output pointer. */
STD_ReturnTypeDef fls_GetVersionInfo(Fls_VersionInfoTypeDef *versionInfo)
{
    if (versionInfo == NULL) {
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

/* Read a range and update the driver's synchronous job status. */
STD_ReturnTypeDef fls_Read(uint32_t address, uint8_t *data, uint16_t length)
{
    STD_ReturnTypeDef result;

    if ((length > 0U && data == NULL) || !fls_IsRangeValid(address, length) ||
        fls_BeginJob() != E_OK) {
        return E_NOT_OK;
    }

    result = fls_wq25xx_Read_Data(address, data, length);
    return fls_EndJob(result);
}

/* Send READ DATA and keep CS low across the command, address, and data phases. */
STD_ReturnTypeDef fls_wq25xx_Read_Data(uint32_t address, uint8_t *data, uint16_t length)
{
    uint8_t command[4] = {
        FLASH_READ_DATA,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)address
    };
    STD_ReturnTypeDef result = E_OK;

    if ((length > 0U && data == NULL) || !fls_IsRangeValid(address, length)) {
        return E_NOT_OK;
    }
    if (length == 0U) {
        return E_OK;
    }

    SPI_FLASH_CS_LOW();
    if (fls_Transmit(command, sizeof(command)) != E_OK ||
        fls_Receive(data, length) != E_OK) {
        result = E_NOT_OK;
    }
    SPI_FLASH_CS_HIGH();
    return result;
}

/*
 * Preserve unaffected bytes by rewriting each touched sector from the RAM
 * snapshot. Power loss or an I/O failure after erase can still lose that sector.
 */
STD_ReturnTypeDef fls_Write(uint32_t address, uint8_t *data, uint16_t length)
{
    STD_ReturnTypeDef result = E_OK;
    uint32_t currentAddress = address;
    uint32_t requestEnd = address + length;
    uint16_t remaining = length;

    if ((length > 0U && data == NULL) || !fls_IsRangeValid(address, length) ||
        fls_BeginJob() != E_OK) {
        return E_NOT_OK;
    }

    while (remaining > 0U) {
        uint32_t sectorAddress =
            currentAddress - (currentAddress % FLASH_W25QXX_SECTOR_SIZE);
        uint32_t sectorEnd = sectorAddress + FLASH_W25QXX_SECTOR_SIZE;
        uint32_t copyEnd = requestEnd < sectorEnd ? requestEnd : sectorEnd;
        uint16_t sectorOffset = (uint16_t)(currentAddress - sectorAddress);
        uint16_t copyLength = (uint16_t)(copyEnd - currentAddress);
        uint16_t dataOffset = (uint16_t)(currentAddress - address);

        /* Snapshot the complete sector before modifying or erasing it. */
        if (fls_wq25xx_Read_Data(sectorAddress, fls_SectorBuffer,
                                 FLASH_W25QXX_SECTOR_SIZE) != E_OK) {
            result = E_NOT_OK;
            break;
        }

        memcpy(&fls_SectorBuffer[sectorOffset], &data[dataOffset], copyLength);

        /* Erase is sector-wide, so restore the modified snapshot afterwards. */
        if (fls_wq25xx_Sector_Erase(sectorAddress) != E_OK ||
            fls_WaitUntilReady(FLASH_ERASE_TIMEOUT_MS) != E_OK) {
            result = E_NOT_OK;
            break;
        }

        for (uint16_t pageOffset = 0U;
             pageOffset < FLASH_W25QXX_SECTOR_SIZE;
             pageOffset += FLASH_W25QXX_PAGE_SIZE) {
            if (!fls_IsPageErased(&fls_SectorBuffer[pageOffset]) &&
                fls_ProgramData(sectorAddress + pageOffset,
                                &fls_SectorBuffer[pageOffset],
                                FLASH_W25QXX_PAGE_SIZE) != E_OK) {
                result = E_NOT_OK;
                break;
            }
        }
        if (result != E_OK) {
            break;
        }

        currentAddress = copyEnd;
        remaining = (uint16_t)(requestEnd - currentAddress);
    }

    return fls_EndJob(result);
}

/* Keep the legacy low-level entry point aligned with preserving write behavior. */
STD_ReturnTypeDef fls_wq25xx_Write_Data(uint32_t address, uint8_t *data, uint16_t length)
{
    return fls_Write(address, data, length);
}

/* Legacy callback hook retained for code that calls it explicitly. */
void fls_SPI_ReceiveCompleteCallback(void)
{
    if (Fls_Config.State == FLS_BUSY) {
        Fls_Config.State = FLS_IDLE;
        Fls_Config.JobStatus = FLS_JOB_OK;
    }
}

/* Legacy callback hook retained for code that calls it explicitly. */
void fls_SPI_TransmitCompleteCallback(void)
{
    if (Fls_Config.State == FLS_BUSY) {
        Fls_Config.State = FLS_IDLE;
        Fls_Config.JobStatus = FLS_JOB_OK;
    }
}

/* Erase every sector intersecting the requested range; this does not preserve data. */
STD_ReturnTypeDef fls_Erase(uint32_t address, uint16_t length)
{
    uint32_t startAddress;
    uint32_t endAddress;
    STD_ReturnTypeDef result = E_OK;

    if (!fls_IsRangeValid(address, length) || fls_BeginJob() != E_OK) {
        return E_NOT_OK;
    }
    if (length == 0U) {
        return fls_EndJob(E_OK);
    }

    startAddress = address - (address % FLASH_W25QXX_SECTOR_SIZE);
    endAddress = address + length;
    while (startAddress < endAddress) {
        if (fls_wq25xx_Sector_Erase(startAddress) != E_OK ||
            fls_WaitUntilReady(FLASH_ERASE_TIMEOUT_MS) != E_OK) {
            result = E_NOT_OK;
            break;
        }
        startAddress += FLASH_W25QXX_SECTOR_SIZE;
    }
    return fls_EndJob(result);
}

/* Start erasing the sector containing address; the caller waits for completion. */
STD_ReturnTypeDef fls_wq25xx_Sector_Erase(uint32_t address)
{
    uint8_t command[4] = {
        FLASH_SECTOR_ERASE,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)address
    };

    if (!fls_IsRangeValid(address, 1U) ||
        fls_wq25xx_Write_Enable() != E_OK) {
        return E_NOT_OK;
    }

    SPI_FLASH_CS_LOW();
    if (fls_Transmit(command, sizeof(command)) != E_OK) {
        SPI_FLASH_CS_HIGH();
        return E_NOT_OK;
    }
    SPI_FLASH_CS_HIGH();
    return E_OK;
}

/* Read and cache manufacturer, memory-type, and capacity bytes from JEDEC ID. */
STD_ReturnTypeDef fls_wq25xx_Read_JEDEC_ID(void)
{
    uint8_t command = FLASH_JEDEC_ID;
    uint8_t jedecID[3];

    SPI_FLASH_CS_LOW();
    if (fls_Transmit(&command, 1U) != E_OK || fls_Receive(jedecID, sizeof(jedecID)) != E_OK) {
        SPI_FLASH_CS_HIGH();
        return E_NOT_OK;
    }
    SPI_FLASH_CS_HIGH();

    Fls_Config.FlashInfo.ManufacturerID = jedecID[0];
    Fls_Config.FlashInfo.MemoryType = (uint16_t)(((uint16_t)jedecID[1] << 8) | jedecID[2]);
    Fls_Config.FlashInfo.Capacity = jedecID[2];
    return E_OK;
}

/* Return status register 1, treating a bus error as BUSY to prevent unsafe writes. */
uint8_t fls_wq25xx_Read_Status_Register1(void)
{
    uint8_t command = FLASH_READ_STATUS_REG1;
    uint8_t status = 0U;

    SPI_FLASH_CS_LOW();
    if (fls_Transmit(&command, 1U) != E_OK || fls_Receive(&status, 1U) != E_OK) {
        status = STATUS_REG1_BUSY_MASK;
    }
    SPI_FLASH_CS_HIGH();
    return status;
}

/* Return status register 2, using 0xFF to signal an SPI read failure. */
uint8_t fls_wq25xx_Read_Status_Register2(void)
{
    uint8_t command = FLASH_READ_STATUS_REG2;
    uint8_t status = 0U;

    SPI_FLASH_CS_LOW();
    if (fls_Transmit(&command, 1U) != E_OK || fls_Receive(&status, 1U) != E_OK) {
        status = 0xFFU;
    }
    SPI_FLASH_CS_HIGH();
    return status;
}

/* Write both status registers and wait until the internal programming completes. */
STD_ReturnTypeDef fls_wq25xx_Write_Status_Register(uint8_t reg1, uint8_t reg2)
{
    uint8_t command[3] = {FLASH_WRT_STATUS_PROG, reg1, reg2};

    if (fls_wq25xx_Write_Enable() != E_OK) {
        return E_NOT_OK;
    }
    SPI_FLASH_CS_LOW();
    if (fls_Transmit(command, sizeof(command)) != E_OK) {
        SPI_FLASH_CS_HIGH();
        return E_NOT_OK;
    }
    SPI_FLASH_CS_HIGH();
    return fls_WaitUntilReady(FLASH_WRITE_TIMEOUT_MS);
}

/* Enable programming/erase and verify the write-enable latch was accepted. */
STD_ReturnTypeDef fls_wq25xx_Write_Enable(void)
{
    if (fls_SendCommand(FLASH_WRITE_ENABLE) != E_OK) {
        return E_NOT_OK;
    }
    return (fls_wq25xx_Read_Status_Register1() & STATUS_REG1_WEL_MASK) != 0U ?
           E_OK : E_NOT_OK;
}

/* Disable further programming and erase commands until write-enable is set again. */
STD_ReturnTypeDef fls_wq25xx_Write_Disable(void)
{
    return fls_SendCommand(FLASH_WRITE_DISABLE);
}

/* Compare in bounded chunks so the temporary buffer does not grow with length. */
STD_ReturnTypeDef fls_Compare(uint32_t address, uint8_t *data, uint16_t length)
{
    uint8_t buffer[FLASH_W25QXX_PAGE_SIZE];
    uint16_t offset = 0U;

    if ((length > 0U && data == NULL) || !fls_IsRangeValid(address, length)) {
        return E_NOT_OK;
    }
    while (offset < length) {
        uint16_t chunk = (uint16_t)(length - offset);
        if (chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }
        if (fls_Read(address + offset, buffer, chunk) != E_OK) {
            return E_NOT_OK;
        }
        for (uint16_t i = 0U; i < chunk; i++) {
            if (buffer[i] != data[offset + i]) {
                return E_NOT_OK;
            }
        }
        offset = (uint16_t)(offset + chunk);
    }
    return E_OK;
}

/* Verify erased contents (0xFF) using a fixed-size temporary read buffer. */
STD_ReturnTypeDef fls_BlankCheck(uint32_t address, uint16_t length)
{
    uint8_t buffer[FLASH_W25QXX_PAGE_SIZE];
    uint16_t offset = 0U;

    if (!fls_IsRangeValid(address, length)) {
        return E_NOT_OK;
    }
    while (offset < length) {
        uint16_t chunk = (uint16_t)(length - offset);
        if (chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }
        if (fls_Read(address + offset, buffer, chunk) != E_OK) {
            return E_NOT_OK;
        }
        for (uint16_t i = 0U; i < chunk; i++) {
            if (buffer[i] != 0xFFU) {
                return E_NOT_OK;
            }
        }
        offset = (uint16_t)(offset + chunk);
    }
    return E_OK;
}

/* Erase the addressed sector and wait until the device reports ready. */
STD_ReturnTypeDef fls_Erase_Sector(uint32_t address)
{
    STD_ReturnTypeDef result;

    if (!fls_IsRangeValid(address, 1U) || fls_BeginJob() != E_OK) {
        return E_NOT_OK;
    }
    result = fls_wq25xx_Sector_Erase(address);
    if (result == E_OK) {
        result = fls_WaitUntilReady(FLASH_ERASE_TIMEOUT_MS);
    }
    return fls_EndJob(result);
}

/* Erase the whole chip and allow the longer chip-erase completion time. */
STD_ReturnTypeDef fls_Erase_Chip(void)
{
    STD_ReturnTypeDef result;

    if (fls_BeginJob() != E_OK) {
        return E_NOT_OK;
    }
    result = fls_wq25xx_Write_Enable();
    if (result == E_OK) {
        result = fls_SendCommand(FLASH_CHIP_ERASE);
    }
    if (result == E_OK) {
        result = fls_WaitUntilReady(FLASH_CHIP_TIMEOUT_MS);
    }
    return fls_EndJob(result);
}

/* Send the erase-suspend command. */
STD_ReturnTypeDef fls_Erase_Suspend(void)
{
    return fls_SendCommand(FLASH_ERASE_SUSPEND);
}

/* Send the erase-resume command. */
STD_ReturnTypeDef fls_Erase_Resume(void)
{
    return fls_SendCommand(FLASH_ERASE_RESUME);
}

/* Put the flash device into deep power-down mode. */
STD_ReturnTypeDef fls_PowerDown(void)
{
    return fls_SendCommand(FLASH_POWER_DOWN);
}

/* Wake the flash device and wait its required recovery interval. */
STD_ReturnTypeDef fls_ReleasePowerDown(void)
{
    STD_ReturnTypeDef result = fls_SendCommand(FLASH_RELEASE_POWER_DOWN);
    HAL_Delay(1U);
    return result;
}

/* Read the fixed-length 8-byte unique ID, rejecting a null destination. */
STD_ReturnTypeDef fls_wq25xx_Read_Unique_ID(uint8_t *unique_id)
{
    uint8_t command[5] = {FLASH_READ_UNIQUE_ID, 0U, 0U, 0U, 0U};
    STD_ReturnTypeDef result = E_OK;

    if (unique_id == NULL) {
        return E_NOT_OK;
    }

    SPI_FLASH_CS_LOW();
    if (fls_Transmit(command, sizeof(command)) != E_OK ||
        fls_Receive(unique_id, FLASH_UID_SIZE) != E_OK) {
        result = E_NOT_OK;
    }
    SPI_FLASH_CS_HIGH();
    return result;
}

/* Standard single-line SPI is the mode supported by this board wiring. */
STD_ReturnTypeDef fls_SetNormalMode(void)
{
    return E_OK;
}

/* Quad mode needs additional data-line support and is not implemented here. */
STD_ReturnTypeDef fls_SetQuadMode(void)
{
    return E_NOT_OK;
}

/* Report whether the flash write-enable latch is set. */
STD_ReturnTypeDef fls_wq25xx_Is_Writable(void)
{
    return (fls_wq25xx_Read_Status_Register1() & STATUS_REG1_WEL_MASK) != 0U ?
           E_OK : E_NOT_OK;
}
