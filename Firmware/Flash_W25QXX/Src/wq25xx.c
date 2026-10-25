/**
 ******************************************************************************
 * @file wq25xx.c
 * @brief W25Qxx SPI commands and generic flash-backend implementation.
 ******************************************************************************
 */
#include "wq25xx.h"
#include "flash_backend.h"
#include "main.h"

extern SPI_HandleTypeDef hspi1;

#define WQ25XX_WRITE_TIMEOUT_MS  5000U
#define WQ25XX_ERASE_TIMEOUT_MS  5000U
#define WQ25XX_CHIP_TIMEOUT_MS   120000U
#define WQ25XX_UID_SIZE          8U

#define SPI_FLASH_CS_LOW()  HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET)
#define SPI_FLASH_CS_HIGH() HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET)

static uint8_t wq25xx_jedecID[3];

static STD_ReturnTypeDef wq25xx_Transmit(const uint8_t *data, uint16_t length)
{
    return HAL_SPI_Transmit(&hspi1, (uint8_t *)data, length,
                            WQ25XX_SPI_TIMEOUT) == HAL_OK ? E_OK : E_NOT_OK;
}

static STD_ReturnTypeDef wq25xx_Receive(uint8_t *data, uint16_t length)
{
    return HAL_SPI_Receive(&hspi1, data, length,
                           WQ25XX_SPI_TIMEOUT) == HAL_OK ? E_OK : E_NOT_OK;
}

static STD_ReturnTypeDef wq25xx_SendCommand(uint8_t command)
{
    STD_ReturnTypeDef result;

    SPI_FLASH_CS_LOW();
    result = wq25xx_Transmit(&command, 1U);
    SPI_FLASH_CS_HIGH();
    return result;
}

static STD_ReturnTypeDef wq25xx_WaitReady(uint32_t timeout)
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

static STD_ReturnTypeDef wq25xx_Read(uint32_t address, uint8_t *data,
                                     uint16_t length)
{
    uint8_t command[4] = {
        FLASH_READ_DATA,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)address
    };

    if ((length > 0U && data == NULL) ||
        address > WQ25XX_FLASH_SIZE ||
        length > WQ25XX_FLASH_SIZE - address) {
        return E_NOT_OK;
    }
    if (length == 0U) {
        return E_OK;
    }

    SPI_FLASH_CS_LOW();
    if (wq25xx_Transmit(command, sizeof(command)) != E_OK ||
        wq25xx_Receive(data, length) != E_OK) {
        SPI_FLASH_CS_HIGH();
        return E_NOT_OK;
    }
    SPI_FLASH_CS_HIGH();
    return E_OK;
}

static STD_ReturnTypeDef wq25xx_ProgramPage(uint32_t address,
                                            const uint8_t *data,
                                            uint16_t length)
{
    uint8_t command[4] = {
        FLASH_PAGE_PROGRAM,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)address
    };

    if (data == NULL || length == 0U || length > WQ25XX_PAGE_SIZE ||
        (address % WQ25XX_PAGE_SIZE) + length > WQ25XX_PAGE_SIZE ||
        address > WQ25XX_FLASH_SIZE ||
        length > WQ25XX_FLASH_SIZE - address ||
        fls_wq25xx_Write_Enable() != E_OK) {
        return E_NOT_OK;
    }

    SPI_FLASH_CS_LOW();
    if (wq25xx_Transmit(command, sizeof(command)) != E_OK ||
        wq25xx_Transmit(data, length) != E_OK) {
        SPI_FLASH_CS_HIGH();
        return E_NOT_OK;
    }
    SPI_FLASH_CS_HIGH();
    return wq25xx_WaitReady(WQ25XX_WRITE_TIMEOUT_MS);
}

static STD_ReturnTypeDef wq25xx_EraseSector(uint32_t address)
{
    uint8_t command[4] = {
        FLASH_SECTOR_ERASE,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)address
    };

    if (address >= WQ25XX_FLASH_SIZE ||
        fls_wq25xx_Write_Enable() != E_OK) {
        return E_NOT_OK;
    }

    SPI_FLASH_CS_LOW();
    if (wq25xx_Transmit(command, sizeof(command)) != E_OK) {
        SPI_FLASH_CS_HIGH();
        return E_NOT_OK;
    }
    SPI_FLASH_CS_HIGH();
    return wq25xx_WaitReady(WQ25XX_ERASE_TIMEOUT_MS);
}

static STD_ReturnTypeDef wq25xx_EraseChip(void)
{
    if (fls_wq25xx_Write_Enable() != E_OK) {
        return E_NOT_OK;
    }
    if (wq25xx_SendCommand(FLASH_CHIP_ERASE) != E_OK) {
        return E_NOT_OK;
    }
    return wq25xx_WaitReady(WQ25XX_CHIP_TIMEOUT_MS);
}

static STD_ReturnTypeDef wq25xx_SetMode(uint8_t mode)
{
    if (mode == NORMAL_MODE) {
        return E_OK;
    }
    return E_NOT_OK;
}

static STD_ReturnTypeDef wq25xx_ReadJedecID(uint8_t jedecID[3])
{
    uint8_t command = FLASH_JEDEC_ID;

    SPI_FLASH_CS_LOW();
    if (wq25xx_Transmit(&command, 1U) != E_OK ||
        wq25xx_Receive(jedecID, 3U) != E_OK) {
        SPI_FLASH_CS_HIGH();
        return E_NOT_OK;
    }
    SPI_FLASH_CS_HIGH();
    return E_OK;
}

static STD_ReturnTypeDef wq25xx_Init(Fls_ConfigTypeDef *config)
{
    if (config == NULL ||
        wq25xx_WaitReady(WQ25XX_SPI_TIMEOUT) != E_OK ||
        wq25xx_ReadJedecID(wq25xx_jedecID) != E_OK ||
        wq25xx_jedecID[2] != WQ25XX_CAPACITY_CODE) {
        return E_NOT_OK;
    }

    config->FlashSize = WQ25XX_FLASH_SIZE;
    config->PageSize = WQ25XX_PAGE_SIZE;
    config->SectorSize = WQ25XX_SECTOR_SIZE;
    config->BlockSize = WQ25XX_BLOCK_SIZE;
    config->StartAddress = 0U;
    config->EndAddress = WQ25XX_FLASH_SIZE - 1U;
    config->FlashInfo.ManufacturerID = wq25xx_jedecID[0];
    config->FlashInfo.MemoryType =
        (uint16_t)(((uint16_t)wq25xx_jedecID[1] << 8) | wq25xx_jedecID[2]);
    config->FlashInfo.Capacity = wq25xx_jedecID[2];
    return E_OK;
}

const Flash_BackendTypeDef FlashBackend = {
    wq25xx_Init,
    wq25xx_Read,
    wq25xx_ProgramPage,
    wq25xx_EraseSector,
    wq25xx_EraseChip,
    wq25xx_SetMode
};

STD_ReturnTypeDef fls_wq25xx_Read_JEDEC_ID(void)
{
    return wq25xx_ReadJedecID(wq25xx_jedecID);
}

uint8_t fls_wq25xx_Read_Status_Register1(void)
{
    uint8_t command = FLASH_READ_STATUS_REG1;
    uint8_t status = STATUS_REG1_BUSY_MASK;

    SPI_FLASH_CS_LOW();
    if (wq25xx_Transmit(&command, 1U) == E_OK) {
        (void)wq25xx_Receive(&status, 1U);
    }
    SPI_FLASH_CS_HIGH();
    return status;
}

uint8_t fls_wq25xx_Read_Status_Register2(void)
{
    uint8_t command = FLASH_READ_STATUS_REG2;
    uint8_t status = 0xFFU;

    SPI_FLASH_CS_LOW();
    if (wq25xx_Transmit(&command, 1U) == E_OK) {
        (void)wq25xx_Receive(&status, 1U);
    }
    SPI_FLASH_CS_HIGH();
    return status;
}

STD_ReturnTypeDef fls_wq25xx_Write_Enable(void)
{
    if (wq25xx_SendCommand(FLASH_WRITE_ENABLE) != E_OK) {
        return E_NOT_OK;
    }
    return (fls_wq25xx_Read_Status_Register1() & STATUS_REG1_WEL_MASK) != 0U ?
           E_OK : E_NOT_OK;
}

STD_ReturnTypeDef fls_wq25xx_Write_Disable(void)
{
    return wq25xx_SendCommand(FLASH_WRITE_DISABLE);
}

STD_ReturnTypeDef fls_wq25xx_Read_Data(uint32_t address, uint8_t *data,
                                       uint16_t length)
{
    return wq25xx_Read(address, data, length);
}

STD_ReturnTypeDef fls_wq25xx_Write_Data(uint32_t address, uint8_t *data,
                                        uint16_t length)
{
    return fls_Write(address, data, length);
}

STD_ReturnTypeDef fls_wq25xx_Sector_Erase(uint32_t address)
{
    return wq25xx_EraseSector(address);
}

STD_ReturnTypeDef fls_wq25xx_Write_Status_Register(uint8_t reg1, uint8_t reg2)
{
    uint8_t command[3] = {FLASH_WRT_STATUS_PROG, reg1, reg2};

    if (fls_wq25xx_Write_Enable() != E_OK) {
        return E_NOT_OK;
    }
    SPI_FLASH_CS_LOW();
    if (wq25xx_Transmit(command, sizeof(command)) != E_OK) {
        SPI_FLASH_CS_HIGH();
        return E_NOT_OK;
    }
    SPI_FLASH_CS_HIGH();
    return wq25xx_WaitReady(WQ25XX_WRITE_TIMEOUT_MS);
}

STD_ReturnTypeDef fls_wq25xx_Read_Unique_ID(uint8_t *unique_id)
{
    uint8_t command[5] = {FLASH_READ_UNIQUE_ID, 0U, 0U, 0U, 0U};

    if (unique_id == NULL) {
        return E_NOT_OK;
    }
    SPI_FLASH_CS_LOW();
    if (wq25xx_Transmit(command, sizeof(command)) != E_OK ||
        wq25xx_Receive(unique_id, WQ25XX_UID_SIZE) != E_OK) {
        SPI_FLASH_CS_HIGH();
        return E_NOT_OK;
    }
    SPI_FLASH_CS_HIGH();
    return E_OK;
}

STD_ReturnTypeDef fls_Erase_Suspend(void)
{
    return wq25xx_SendCommand(FLASH_ERASE_SUSPEND);
}

STD_ReturnTypeDef fls_Erase_Resume(void)
{
    return wq25xx_SendCommand(FLASH_ERASE_RESUME);
}

STD_ReturnTypeDef fls_PowerDown(void)
{
    return wq25xx_SendCommand(FLASH_POWER_DOWN);
}

STD_ReturnTypeDef fls_ReleasePowerDown(void)
{
    STD_ReturnTypeDef result = wq25xx_SendCommand(FLASH_RELEASE_POWER_DOWN);
    HAL_Delay(1U);
    return result;
}

STD_ReturnTypeDef fls_SetNormalMode(void)
{
    return wq25xx_SetMode(NORMAL_MODE);
}

STD_ReturnTypeDef fls_SetQuadMode(void)
{
    return wq25xx_SetMode(QUAD_MODE);
}

STD_ReturnTypeDef fls_wq25xx_Is_Writable(void)
{
    return (fls_wq25xx_Read_Status_Register1() & STATUS_REG1_WEL_MASK) != 0U ?
           E_OK : E_NOT_OK;
}

void fls_SPI_TransmitCompleteCallback(void)
{
}

void fls_SPI_ReceiveCompleteCallback(void)
{
}
