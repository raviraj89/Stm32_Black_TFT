/**
 ******************************************************************************
 * File Name        : flash_wq25xx.c
 * Description      : Asynchronous SPI DMA driver for W25Qxx flash
 ******************************************************************************
 */
#include "flash_wq25xx.h"
#include "main.h"
#include <string.h>

extern SPI_HandleTypeDef hspi1;

typedef enum {
    FLS_OP_NONE,
    FLS_OP_READ,
    FLS_OP_WRITE,
    FLS_OP_ERASE,
    FLS_OP_CHIP_ERASE,
    FLS_OP_COMPARE,
    FLS_OP_BLANK_CHECK,
    FLS_OP_STATUS1,
    FLS_OP_STATUS2,
    FLS_OP_JEDEC_ID,
    FLS_OP_COMMAND
} Fls_OperationTypeDef;

typedef enum {
    FLS_PHASE_IDLE,
    FLS_PHASE_READ_HEADER,
    FLS_PHASE_READ_DATA,
    FLS_PHASE_WRITE_ENABLE,
    FLS_PHASE_WRITE_HEADER,
    FLS_PHASE_WRITE_DATA,
    FLS_PHASE_ERASE_COMMAND,
    FLS_PHASE_STATUS_COMMAND,
    FLS_PHASE_STATUS_DATA,
    FLS_PHASE_JEDEC_COMMAND,
    FLS_PHASE_JEDEC_DATA,
    FLS_PHASE_SIMPLE_COMMAND,
    FLS_PHASE_WAIT_READY
} Fls_PhaseTypeDef;

typedef struct {
    Fls_OperationTypeDef operation;
    Fls_PhaseTypeDef phase;
    uint32_t address;
    uint32_t remaining;
    uint32_t erase_timeout;
    uint32_t last_status_poll;
    uint32_t wait_started;
    uint16_t chunk_length;
    uint8_t *data;
    const uint8_t *compare_data;
    uint8_t *status_output;
    uint8_t command;
    uint8_t status;
    uint8_t jedec_id[3];
    uint8_t compare_buffer[FLASH_W25QXX_PAGE_SIZE];
    uint8_t header[4];
    volatile uint8_t transfer_complete;
    volatile uint8_t transfer_error;
} Fls_JobContextTypeDef;

static Fls_JobContextTypeDef Fls_Job;
Fls_ConfigTypeDef Fls_Config;

static void finish_job(FLS_JobStatusTypeDef result)
{
    SPI_FLASH_CS_HIGH();
    Fls_Job.phase = FLS_PHASE_IDLE;
    Fls_Job.operation = FLS_OP_NONE;
    Fls_Job.transfer_complete = 0U;
    Fls_Job.transfer_error = 0U;
    Fls_Config.State = FLS_IDLE;
    Fls_Config.JobStatus = result;
}

static void fail_job(void)
{
    finish_job(FLS_JOB_FAILED);
}

static STD_ReturnTypeDef start_tx(const uint8_t *data, uint16_t length, Fls_PhaseTypeDef phase, bool begin_transaction)
{
    if (begin_transaction) {
        SPI_FLASH_CS_LOW();
    }
    Fls_Job.phase = phase;
    Fls_Job.transfer_complete = 0U;
    Fls_Job.transfer_error = 0U;
    if (HAL_SPI_Transmit_DMA(&hspi1, data, length) != HAL_OK) {
        fail_job();
        return E_NOT_OK;
    }
    return E_OK;
}

static STD_ReturnTypeDef start_rx(uint8_t *data, uint16_t length, Fls_PhaseTypeDef phase)
{
    Fls_Job.phase = phase;
    Fls_Job.transfer_complete = 0U;
    Fls_Job.transfer_error = 0U;
    if (HAL_SPI_Receive_DMA(&hspi1, data, length) != HAL_OK) {
        fail_job();
        return E_NOT_OK;
    }
    return E_OK;
}

static STD_ReturnTypeDef start_status_read(void)
{
    Fls_Job.command = FLASH_READ_STATUS_REG1;
    return start_tx(&Fls_Job.command, 1U, FLS_PHASE_STATUS_COMMAND, true);
}

static STD_ReturnTypeDef start_read_chunk(uint32_t address, uint8_t *buffer, uint16_t length)
{
    Fls_Job.header[0] = FLASH_READ_DATA;
    Fls_Job.header[1] = (uint8_t)(address >> 16);
    Fls_Job.header[2] = (uint8_t)(address >> 8);
    Fls_Job.header[3] = (uint8_t)address;
    Fls_Job.data = buffer;
    Fls_Job.chunk_length = length;
    return start_tx(Fls_Job.header, sizeof(Fls_Job.header), FLS_PHASE_READ_HEADER, true);
}

static STD_ReturnTypeDef start_write_enable(void)
{
    Fls_Job.command = FLASH_WRITE_ENABLE;
    return start_tx(&Fls_Job.command, 1U, FLS_PHASE_WRITE_ENABLE, true);
}

static STD_ReturnTypeDef start_page_program(void)
{
    Fls_Job.header[0] = FLASH_PAGE_PROGRAM;
    Fls_Job.header[1] = (uint8_t)(Fls_Job.address >> 16);
    Fls_Job.header[2] = (uint8_t)(Fls_Job.address >> 8);
    Fls_Job.header[3] = (uint8_t)Fls_Job.address;
    Fls_Job.chunk_length = (uint16_t)((Fls_Job.remaining > FLASH_W25QXX_PAGE_SIZE)
        ? FLASH_W25QXX_PAGE_SIZE : Fls_Job.remaining);
    {
        uint16_t page_remaining = (uint16_t)(FLASH_W25QXX_PAGE_SIZE -
            (Fls_Job.address % FLASH_W25QXX_PAGE_SIZE));
        if (Fls_Job.chunk_length > page_remaining) {
            Fls_Job.chunk_length = page_remaining;
        }
    }
    return start_tx(Fls_Job.header, sizeof(Fls_Job.header), FLS_PHASE_WRITE_HEADER, true);
}

static STD_ReturnTypeDef start_erase_command(void)
{
    if (Fls_Job.operation == FLS_OP_CHIP_ERASE) {
        Fls_Job.command = FLASH_CHIP_ERASE;
        return start_tx(&Fls_Job.command, 1U, FLS_PHASE_ERASE_COMMAND, true);
    }

    Fls_Job.header[0] = FLASH_SECTOR_ERASE;
    Fls_Job.header[1] = (uint8_t)(Fls_Job.address >> 16);
    Fls_Job.header[2] = (uint8_t)(Fls_Job.address >> 8);
    Fls_Job.header[3] = (uint8_t)Fls_Job.address;
    return start_tx(Fls_Job.header, sizeof(Fls_Job.header), FLS_PHASE_ERASE_COMMAND, true);
}

static bool flash_range_valid(uint32_t address, uint32_t length)
{
    return length > 0U &&
        address >= Fls_Config.StartAddress &&
        address - Fls_Config.StartAddress < Fls_Config.FlashSize &&
        length <= Fls_Config.FlashSize - (address - Fls_Config.StartAddress);
}

static STD_ReturnTypeDef begin_job(Fls_OperationTypeDef operation)
{
    if (Fls_Config.State != FLS_IDLE || Fls_Job.operation != FLS_OP_NONE) {
        return E_NOT_OK;
    }

    Fls_Job.operation = operation;
    Fls_Job.phase = FLS_PHASE_IDLE;
    Fls_Job.transfer_complete = 0U;
    Fls_Job.transfer_error = 0U;
    Fls_Config.State = FLS_BUSY;
    Fls_Config.JobStatus = FLS_JOB_PENDING;
    return E_OK;
}

static STD_ReturnTypeDef start_simple_command(uint8_t command)
{
    if (begin_job(FLS_OP_COMMAND) != E_OK) {
        return E_NOT_OK;
    }
    Fls_Job.command = command;
    return start_tx(&Fls_Job.command, 1U, FLS_PHASE_SIMPLE_COMMAND, true);
}

STD_ReturnTypeDef fls_Init(void)
{
    memset(&Fls_Job, 0, sizeof(Fls_Job));
    memset(&Fls_Config, 0, sizeof(Fls_Config));
    Fls_Config.FlashSize = FLASH_W25QXX_FLASH_SIZE;
    Fls_Config.PageSize = FLASH_W25QXX_PAGE_SIZE;
    Fls_Config.SectorSize = FLASH_W25QXX_SECTOR_SIZE;
    Fls_Config.BlockSize = FLASH_W25QXX_BLOCK_SIZE;
    Fls_Config.StartAddress = FLASH_W25QXX_START_ADDRESS;
    Fls_Config.EndAddress = FLASH_W25QXX_END_ADDRESS;
    Fls_Config.State = FLS_IDLE;
    Fls_Config.JobStatus = FLS_JOB_OK;
    Fls_Config.Mode = NORMAL_MODE;
    return fls_wq25xx_Read_JEDEC_ID();
}

STD_ReturnTypeDef fls_setMode(uint8_t mode)
{
    if (mode != NORMAL_MODE && mode != QUAD_MODE) {
        return E_NOT_OK;
    }
    if (Fls_Config.State != FLS_IDLE) {
        return E_NOT_OK;
    }
    Fls_Config.Mode = (FLS_ModeTypeDef)mode;
    return mode == NORMAL_MODE ? fls_SetNormalMode() : fls_SetQuadMode();
}

STD_ReturnTypeDef fls_Cancel_Job(void)
{
    if (Fls_Job.operation == FLS_OP_NONE) {
        return E_NOT_OK;
    }
    if (Fls_Job.phase != FLS_PHASE_WAIT_READY &&
        Fls_Job.phase != FLS_PHASE_IDLE) {
        if (HAL_SPI_DMAStop(&hspi1) != HAL_OK) {
            return E_NOT_OK;
        }
    }
    finish_job(FLS_JOB_FAILED);
    return E_OK;
}

FLS_StatusTypeDef fls_GetStatus(void)
{
    return Fls_Config.State;
}

FLS_JobStatusTypeDef fls_GetJobResult(void)
{
    return Fls_Config.JobStatus;
}

STD_ReturnTypeDef fls_GetVersionInfo(Fls_VersionInfoTypeDef *versionInfo)
{
    if (versionInfo == NULL) {
        return E_NOT_OK;
    }
    versionInfo->vendorID = Fls_Config.FlashInfo.ManufacturerID;
    versionInfo->moduleID = Fls_Config.FlashInfo.MemoryType;
    versionInfo->module_Size = (uint16_t)(Fls_Config.FlashSize / 1024U);
    versionInfo->sw_major_version = 0U;
    versionInfo->sw_minor_version = 0U;
    versionInfo->sw_patch_version = 0U;
    return E_OK;
}

STD_ReturnTypeDef fls_Read(uint32_t address, uint8_t *data, uint16_t length)
{
    if (data == NULL || !flash_range_valid(address, length) ||
        begin_job(FLS_OP_READ) != E_OK) {
        return E_NOT_OK;
    }
    Fls_Job.address = address;
    Fls_Job.remaining = length;
    return start_read_chunk(address, data, length);
}

STD_ReturnTypeDef fls_wq25xx_Read_Data(uint32_t address, uint8_t *data, uint16_t length)
{
    return fls_Read(address, data, length);
}

STD_ReturnTypeDef fls_Write(uint32_t address, uint8_t *data, uint16_t length)
{
    if (data == NULL || !flash_range_valid(address, length) ||
        begin_job(FLS_OP_WRITE) != E_OK) {
        return E_NOT_OK;
    }
    Fls_Job.address = address;
    Fls_Job.remaining = length;
    Fls_Job.data = data;
    return start_write_enable();
}

STD_ReturnTypeDef fls_wq25xx_Write_Data(uint32_t address, uint8_t *data, uint16_t length)
{
    return fls_Write(address, data, length);
}

STD_ReturnTypeDef fls_Erase(uint32_t address, uint16_t length)
{
    uint32_t end_address;

    if (!flash_range_valid(address, length) || begin_job(FLS_OP_ERASE) != E_OK) {
        return E_NOT_OK;
    }
    Fls_Job.address = address - (address % Fls_Config.SectorSize);
    end_address = address + length;
    Fls_Job.remaining = (end_address - Fls_Job.address + Fls_Config.SectorSize - 1U) /
        Fls_Config.SectorSize;
    Fls_Job.erase_timeout = FLASH_W25QXX_TIMEOUT;
    return start_write_enable();
}

STD_ReturnTypeDef fls_Compare(uint32_t address, uint8_t *data, uint16_t length)
{
    if (data == NULL || !flash_range_valid(address, length) ||
        begin_job(FLS_OP_COMPARE) != E_OK) {
        return E_NOT_OK;
    }
    Fls_Job.address = address;
    Fls_Job.remaining = length;
    Fls_Job.compare_data = data;
    Fls_Job.data = Fls_Job.compare_buffer;
    return start_read_chunk(address, Fls_Job.compare_buffer,
        (uint16_t)((length > sizeof(Fls_Job.compare_buffer))
            ? sizeof(Fls_Job.compare_buffer) : length));
}

STD_ReturnTypeDef fls_BlankCheck(uint32_t address, uint16_t length)
{
    if (!flash_range_valid(address, length) || begin_job(FLS_OP_BLANK_CHECK) != E_OK) {
        return E_NOT_OK;
    }
    Fls_Job.address = address;
    Fls_Job.remaining = length;
    Fls_Job.data = Fls_Job.compare_buffer;
    return start_read_chunk(address, Fls_Job.compare_buffer,
        (uint16_t)((length > sizeof(Fls_Job.compare_buffer))
            ? sizeof(Fls_Job.compare_buffer) : length));
}

STD_ReturnTypeDef fls_Erase_Sector(uint32_t address)
{
    return fls_Erase(address, (uint16_t)Fls_Config.SectorSize);
}

STD_ReturnTypeDef fls_Erase_Chip(void)
{
    if (begin_job(FLS_OP_CHIP_ERASE) != E_OK) {
        return E_NOT_OK;
    }
    Fls_Job.erase_timeout = 120000U;
    return start_write_enable();
}

STD_ReturnTypeDef fls_Erase_Suspend(void)
{
    return start_simple_command(FLASH_ERASE_SUSPEND);
}

STD_ReturnTypeDef fls_Erase_Resume(void)
{
    return start_simple_command(FLASH_ERASE_RESUME);
}

STD_ReturnTypeDef fls_PowerDown(void)
{
    return start_simple_command(FLASH_POWER_DOWN);
}

STD_ReturnTypeDef fls_ReleasePowerDown(void)
{
    return start_simple_command(FLASH_RELEASE_POWER_DOWN);
}

STD_ReturnTypeDef fls_wq25xx_Sector_Erase(uint32_t address)
{
    return fls_Erase_Sector(address);
}

STD_ReturnTypeDef fls_wq25xx_Read_Status_Register1(uint8_t *status)
{
    if (status == NULL || begin_job(FLS_OP_STATUS1) != E_OK) {
        return E_NOT_OK;
    }
    Fls_Job.status_output = status;
    Fls_Job.command = FLASH_READ_STATUS_REG1;
    return start_tx(&Fls_Job.command, 1U, FLS_PHASE_STATUS_COMMAND, true);
}

STD_ReturnTypeDef fls_wq25xx_Read_Status_Register2(uint8_t *status)
{
    if (status == NULL || begin_job(FLS_OP_STATUS2) != E_OK) {
        return E_NOT_OK;
    }
    Fls_Job.status_output = status;
    Fls_Job.command = FLASH_READ_STATUS_REG2;
    return start_tx(&Fls_Job.command, 1U, FLS_PHASE_STATUS_COMMAND, true);
}

STD_ReturnTypeDef fls_wq25xx_Read_JEDEC_ID(void)
{
    if (begin_job(FLS_OP_JEDEC_ID) != E_OK) {
        return E_NOT_OK;
    }
    Fls_Job.command = FLASH_JEDEC_ID;
    return start_tx(&Fls_Job.command, 1U, FLS_PHASE_JEDEC_COMMAND, true);
}

STD_ReturnTypeDef fls_wq25xx_Write_Enable(void)
{
    return start_simple_command(FLASH_WRITE_ENABLE);
}

STD_ReturnTypeDef fls_wq25xx_Write_Disable(void)
{
    return start_simple_command(FLASH_WRITE_DISABLE);
}

STD_ReturnTypeDef fls_SetNormalMode(void)
{
    return E_OK;
}

STD_ReturnTypeDef fls_SetQuadMode(void)
{
    return E_OK;
}

void fls_MainFunction(void)
{
    if (Fls_Job.operation == FLS_OP_NONE) {
        return;
    }

    if (Fls_Job.transfer_error != 0U) {
        fail_job();
        return;
    }

    if (Fls_Job.transfer_complete != 0U) {
        Fls_Job.transfer_complete = 0U;
        switch (Fls_Job.phase) {
        case FLS_PHASE_READ_HEADER:
            (void)start_rx(Fls_Job.data, Fls_Job.chunk_length, FLS_PHASE_READ_DATA);
            return;
        case FLS_PHASE_READ_DATA:
            SPI_FLASH_CS_HIGH();
            if (Fls_Job.operation == FLS_OP_COMPARE ||
                Fls_Job.operation == FLS_OP_BLANK_CHECK) {
                uint16_t i;
                for (i = 0U; i < Fls_Job.chunk_length; i++) {
                    uint8_t expected = Fls_Job.operation == FLS_OP_BLANK_CHECK
                        ? 0xFFU : Fls_Job.compare_data[i];
                    if (Fls_Job.compare_buffer[i] != expected) {
                        finish_job(FLS_JOB_FAILED);
                        return;
                    }
                }
                Fls_Job.address += Fls_Job.chunk_length;
                Fls_Job.remaining -= Fls_Job.chunk_length;
                if (Fls_Job.remaining == 0U) {
                    finish_job(FLS_JOB_OK);
                    return;
                }
                if (Fls_Job.operation == FLS_OP_COMPARE) {
                    Fls_Job.compare_data += Fls_Job.chunk_length;
                }
                (void)start_read_chunk(Fls_Job.address, Fls_Job.compare_buffer,
                    (uint16_t)((Fls_Job.remaining > sizeof(Fls_Job.compare_buffer))
                        ? sizeof(Fls_Job.compare_buffer) : Fls_Job.remaining));
                return;
            }
            finish_job(FLS_JOB_OK);
            return;
        case FLS_PHASE_WRITE_ENABLE:
            SPI_FLASH_CS_HIGH();
            if (Fls_Job.operation == FLS_OP_WRITE) {
                (void)start_page_program();
            } else {
                (void)start_erase_command();
            }
            return;
        case FLS_PHASE_WRITE_HEADER:
            (void)start_tx(Fls_Job.data, Fls_Job.chunk_length, FLS_PHASE_WRITE_DATA, false);
            return;
        case FLS_PHASE_WRITE_DATA:
            SPI_FLASH_CS_HIGH();
            Fls_Job.phase = FLS_PHASE_WAIT_READY;
            Fls_Job.wait_started = HAL_GetTick();
            Fls_Job.last_status_poll = Fls_Job.wait_started - 1U;
            Fls_Job.erase_timeout = 1000U;
            return;
        case FLS_PHASE_ERASE_COMMAND:
            SPI_FLASH_CS_HIGH();
            Fls_Job.phase = FLS_PHASE_WAIT_READY;
            Fls_Job.wait_started = HAL_GetTick();
            Fls_Job.last_status_poll = Fls_Job.wait_started - 1U;
            return;
        case FLS_PHASE_STATUS_COMMAND:
            (void)start_rx(&Fls_Job.status, 1U, FLS_PHASE_STATUS_DATA);
            return;
        case FLS_PHASE_STATUS_DATA:
            SPI_FLASH_CS_HIGH();
            if (Fls_Job.operation == FLS_OP_STATUS1 ||
                Fls_Job.operation == FLS_OP_STATUS2) {
                *Fls_Job.status_output = Fls_Job.status;
                finish_job(FLS_JOB_OK);
                return;
            }
            if ((Fls_Job.status & STATUS_REG1_BUSY_MASK) != 0U) {
                Fls_Job.phase = FLS_PHASE_WAIT_READY;
                Fls_Job.last_status_poll = HAL_GetTick();
                return;
            }
            if (Fls_Job.operation == FLS_OP_WRITE) {
                Fls_Job.address += Fls_Job.chunk_length;
                Fls_Job.data += Fls_Job.chunk_length;
                Fls_Job.remaining -= Fls_Job.chunk_length;
                if (Fls_Job.remaining == 0U) {
                    finish_job(FLS_JOB_OK);
                } else {
                    (void)start_write_enable();
                }
            } else if (Fls_Job.operation == FLS_OP_ERASE) {
                Fls_Job.address += Fls_Config.SectorSize;
                Fls_Job.remaining--;
                if (Fls_Job.remaining == 0U) {
                    finish_job(FLS_JOB_OK);
                } else {
                    (void)start_write_enable();
                }
            } else {
                finish_job(FLS_JOB_OK);
            }
            return;
        case FLS_PHASE_JEDEC_COMMAND:
            (void)start_rx(Fls_Job.jedec_id, sizeof(Fls_Job.jedec_id), FLS_PHASE_JEDEC_DATA);
            return;
        case FLS_PHASE_JEDEC_DATA:
            SPI_FLASH_CS_HIGH();
            Fls_Config.FlashInfo.ManufacturerID = Fls_Job.jedec_id[0];
            Fls_Config.FlashInfo.MemoryType =
                (uint16_t)(((uint16_t)Fls_Job.jedec_id[1] << 8) | Fls_Job.jedec_id[2]);
            Fls_Config.FlashInfo.Capacity = Fls_Job.jedec_id[2];
            finish_job(FLS_JOB_OK);
            return;
        case FLS_PHASE_SIMPLE_COMMAND:
            finish_job(FLS_JOB_OK);
            return;
        default:
            fail_job();
            return;
        }
    }

    if (Fls_Job.phase == FLS_PHASE_WAIT_READY) {
        uint32_t timeout = Fls_Job.erase_timeout;
        if (timeout == 0U) {
            timeout = FLASH_W25QXX_TIMEOUT;
        }
        if ((uint32_t)(HAL_GetTick() - Fls_Job.wait_started) >= timeout) {
            fail_job();
        } else if ((uint32_t)(HAL_GetTick() - Fls_Job.last_status_poll) >= 1U) {
            Fls_Job.command = FLASH_READ_STATUS_REG1;
            Fls_Job.last_status_poll = HAL_GetTick();
            (void)start_status_read();
        }
    }
}

void fls_SPI_TransmitCompleteCallback(void)
{
    Fls_Job.transfer_complete = 1U;
}

void fls_SPI_ReceiveCompleteCallback(void)
{
    Fls_Job.transfer_complete = 1U;
}

void fls_SPI_ErrorCallback(void)
{
    Fls_Job.transfer_error = 1U;
}
