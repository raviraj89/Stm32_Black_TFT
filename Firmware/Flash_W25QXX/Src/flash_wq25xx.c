/**
 ******************************************************************************
 * File Name		: flash_wq25xx.c
 * Description		: This file provides code for the SPI based flash driver
 ******************************************************************************
 */
/* Includes ------------------------------------------------------------------*/
#include "flash_wq25xx.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_spi.h"
#include "main.h"



/* Global Variables ----------------------------------------------------------*/
extern SPI_HandleTypeDef hspi1;
 Fls_ConfigTypeDef Fls_Config;





/* Functions -----------------------------------------------------------------*/
/**
  * @brief  Initialize the flash memory.
  * @param  None
  * @retval None
  */  
STD_ReturnTypeDef fls_Init(void)
{
    Fls_Config.FlashSize = FLASH_W25QXX_FLASH_SIZE;
    Fls_Config.PageSize = FLASH_W25QXX_PAGE_SIZE;
    Fls_Config.SectorSize = FLASH_W25QXX_SECTOR_SIZE;
    Fls_Config.BlockSize = FLASH_W25QXX_BLOCK_SIZE;
    Fls_Config.StartAddress = FLASH_W25QXX_START_ADDRESS;
    Fls_Config.EndAddress = FLASH_W25QXX_END_ADDRESS;
    Fls_Config.State = FLS_UNINITIALIZED; // Set the state to idle if initialized, otherwise uninitialized
    Fls_Config.JobStatus = FLS_JOB_OK;
    Fls_Config.Mode = NORMAL_MODE;                                     // Normal mode 
    fls_wq25xx_Read_JEDEC_ID();   
    return E_OK;
}

/**
  * @brief  Set the mode of the flash memory.
  * @param  mode: The mode to set (normal, high performance, etc.).
  * @retval None
  */
STD_ReturnTypeDef fls_setMode(uint8_t mode)
{
    Fls_Config.Mode = mode;
    switch (mode) {
        case NORMAL_MODE: // Normal mode
            if (fls_SetNormalMode() != E_OK) {
                return E_NOT_OK;
            }
            break;
        case QUAD_MODE: // Quad mode
            if (fls_SetQuadMode() != E_OK) {
                return E_NOT_OK;
            }
            break;
        default:
            return E_NOT_OK; // Invalid mode
    }
    // Implement mode setting logic if required
    return E_OK;
}

/**
  * @brief  Cancel the ongoing job in the flash memory.
  * @param  None
  * @retval None
  */
STD_ReturnTypeDef fls_Cancel_Job(void)
{
    // Implement job cancellation logic if required
    if (Fls_Config.State == FLASH_STATE_BUSY) {
        Fls_Config.State = FLASH_STATE_IDLE;
        Fls_Config.JobStatus = FLS_JOB_FAILED; // Mark the job as failed
    }
    return E_OK;
}

/**
  * @brief  Get the current status of the flash memory.
  * @param  None
  * @retval None
  */
FLS_StatusTypeDef fls_GetStatus(void)
{
    return Fls_Config.State; // Implement status retrieval logic if required
}

/**
  * @brief  Get the result of the last job in the flash memory.
  * @param  None
  * @retval None
  */
FLS_JobStatusTypeDef fls_GetJobResult(void)
{
    // Implement job result retrieval logic if required
    return Fls_Config.JobStatus;
}



/**
  * @brief  Get the version information of the flash memory driver.
  * @param  versionInfo: Pointer to a structure to store the version information.
  * @retval E_OK if successful, E_NOT_OK if an error occurred.
  */
STD_ReturnTypeDef fls_GetVersionInfo(Fls_VersionInfoTypeDef *versionInfo)
{
  if (versionInfo == NULL) {
        return E_NOT_OK; // Handle null pointer error
    }
    versionInfo->vendorID = Fls_Config.FlashInfo.ManufacturerID;
    versionInfo->moduleID = Fls_Config.FlashInfo.MemoryType;
    versionInfo->module_Size = Fls_Config.FlashSize / 1024; // Size in KB
    versionInfo->sw_major_version = 0;
    versionInfo->sw_minor_version = 0;
    versionInfo->sw_patch_version = 0;
    return E_OK;
}

/**
  * @brief  Read data from the flash memory.
  * @param  address: The starting address to read from.
  * @param  data: Pointer to a buffer to store the read data.
  * @param  length: The number of bytes to read.
  * @retval E_OK if successful, E_NOT_OK if an error occurred.
  */
STD_ReturnTypeDef fls_Read(uint32_t address, uint8_t *data, uint16_t length)
{
    if (address < Fls_Config.StartAddress || address + length > Fls_Config.EndAddress) {
        return E_NOT_OK; // Address out of range
    }
    Fls_Config.State = FLS_BUSY; // Set the state to busy
    Fls_Config.JobStatus = FLS_JOB_PENDING; // Mark the job as pending
    if (fls_wq25xx_Read_Data(address, data, length) != E_OK) {
        return E_NOT_OK;
    }
    return E_OK;
}

/**
  * @brief  Read data from the flash memory using SPI.
  * @param  address: The starting address to read from.
  * @param  data: Pointer to a buffer to store the read data.
  * @param  length: The number of bytes to read.
  * @retval E_OK if successful, E_NOT_OK if an error occurred.
  */
 STD_ReturnTypeDef fls_wq25xx_Read_Data(uint32_t address, uint8_t *data, uint16_t length)
{
    uint8_t address_bytes[3];
    address_bytes[2] = (address >> 16) & 0xFF; // Address byte 1
    address_bytes[1] = (address >> 8) & 0xFF; // Address byte 2
    address_bytes[0] = address & 0xFF; // Address byte 3    
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_READ_DATA;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    // Transmit the address (3 bytes)
    HAL_SPI_Transmit_IT(&hspi1, (uint8_t *)&address_bytes, 3);
    HAL_SPI_Receive_IT(&hspi1, data, length);
    SPI_FLASH_CS_HIGH();
    return E_OK;
}

STD_ReturnTypeDef fls_Write(uint32_t address, uint8_t *data, uint16_t length)
{ 
    if (address < Fls_Config.StartAddress || address + length > Fls_Config.EndAddress) {
        return E_NOT_OK; // Address out of range
    }
    Fls_Config.State = FLS_BUSY; // Set the state to busy
    Fls_Config.JobStatus = FLS_JOB_PENDING; // Mark the job as pending
    if(fls_wq25xx_Is_Writable()) // Enable write operations
    {
      fls_wq25xx_Write_Enable(); // Failed to enable write operations
    }
    if (fls_wq25xx_Write_Data(address, data, length) != E_OK) {
        return E_NOT_OK;
    } 
    return E_OK;
} 

 STD_ReturnTypeDef fls_wq25xx_Write_Data(uint32_t address, uint8_t *data, uint16_t length)
{
    uint8_t address_bytes[3];
    address_bytes[2] = (address >> 16) & 0xFF; // Address byte 1
    address_bytes[1] = (address >> 8) & 0xFF; // Address byte 2
    address_bytes[0] = address & 0xFF;    
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_PAGE_PROGRAM;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    // Transmit the address (3 bytes)
    HAL_SPI_Transmit_IT(&hspi1, address_bytes, 3);
    HAL_SPI_Transmit_IT(&hspi1, data, length);
    SPI_FLASH_CS_HIGH();
    return E_OK;
}


void fls_SPI_ReceiveCompleteCallback(void)
{
    Fls_Config.State = FLS_IDLE; // Set the state to idle after receive is complete
    Fls_Config.JobStatus = FLS_JOB_OK; // Mark the job as successful
}

void fls_SPI_TransmitCompleteCallback(void)
{
    Fls_Config.State = FLS_IDLE; // Set the state to idle after transmit is complete
    Fls_Config.JobStatus = FLS_JOB_OK; // Mark the job as successful
}

STD_ReturnTypeDef fls_Erase(uint32_t address, uint16_t length)
{
    if (address < Fls_Config.StartAddress || address + length > Fls_Config.EndAddress) {
        return E_NOT_OK; // Address out of range
    }
    Fls_Config.State = FLS_BUSY; // Set the state to busy
    Fls_Config.JobStatus = FLS_JOB_PENDING; // Mark the job as pending
    fls_wq25xx_Sector_Erase(address); // Implement erase logic if required
    while (fls_wq25xx_Read_Status_Register1() & STATUS_REG1_BUSY_MASK) {
        // Wait for the erase operation to complete
    }
    Fls_Config.State = FLS_IDLE; // Set the state to idle after erase is complete
    Fls_Config.JobStatus = FLS_JOB_OK; // Mark the job as successful
    return E_OK;
}

 STD_ReturnTypeDef fls_wq25xx_Sector_Erase(uint32_t address)
{
    uint8_t address_bytes[3];
    address_bytes[2] = (address >> 16) & 0xFF; // Address byte 1
    address_bytes[1] = (address >> 8) & 0xFF; // Address byte 2   
    address_bytes[0] = address & 0xFF; // Address byte 3    
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_SECTOR_ERASE;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    // Transmit the address (3 bytes)
    HAL_SPI_Transmit_IT(&hspi1, address_bytes, 3);
    SPI_FLASH_CS_HIGH();
    return E_OK;
}

 STD_ReturnTypeDef fls_wq25xx_Read_Status_Register1(void)
{
    uint8_t status;
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_READ_STATUS_REG1;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    HAL_SPI_Receive_IT(&hspi1, &status, 1);
    SPI_FLASH_CS_HIGH();
    return status;
}

 STD_ReturnTypeDef fls_wq25xx_Read_JEDEC_ID()
{
    uint8_t jedec_id[3];
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_JEDEC_ID;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    HAL_SPI_Receive_IT(&hspi1, jedec_id, 3);
    SPI_FLASH_CS_HIGH();
    Fls_Config.FlashInfo.ManufacturerID = jedec_id[0];
    Fls_Config.FlashInfo.MemoryType = (jedec_id[1] << 8) | jedec_id[2];
    return E_OK;
}

 STD_ReturnTypeDef fls_wq25xx_Write_Enable(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_WRITE_ENABLE;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    SPI_FLASH_CS_HIGH();
    return E_OK;
}

 STD_ReturnTypeDef fls_wq25xx_Write_Disable(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_WRITE_DISABLE;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    SPI_FLASH_CS_HIGH();
    return E_OK;
}

STD_ReturnTypeDef fls_Compare(uint32_t address, uint8_t *data, uint16_t length)
{
    if (address < Fls_Config.StartAddress || address + length > Fls_Config.EndAddress) {
        return E_NOT_OK; // Address out of range
    }
    uint8_t read_data[length];
    if (fls_Read(address, read_data, length) != E_OK) {
        return E_NOT_OK; // Failed to read data
    }
    for (uint16_t i = 0; i < length; i++) {
        if (read_data[i] != data[i]) {
            return E_NOT_OK; // Data mismatch
        }
    }
    return E_OK; // Data matches
}



/**
  * @brief  Check if the specified flash memory region is blank (erased).
  * @param  address: The starting address to check.
  * @param  length: The number of bytes to check.
  * @retval E_OK if the region is blank, E_NOT_OK if not blank or an error occurred.
  */
STD_ReturnTypeDef fls_BlankCheck(uint32_t address, uint16_t length)
{
    if (address < Fls_Config.StartAddress || address + length > Fls_Config.EndAddress) {
        return E_NOT_OK; // Address out of range
    }
    uint8_t read_data[length];
    if (fls_Read(address, read_data, length) != E_OK) {
        return E_NOT_OK; // Failed to read data
    }
    for (uint16_t i = 0; i < length; i++) {
        if (read_data[i] != 0xFF) { // Assuming erased state is 0xFF
            return E_NOT_OK; // Not blank
        }
    }
    return E_OK; // Blank
}

 STD_ReturnTypeDef fls_Erase_Sector(uint32_t address)
{
    return fls_wq25xx_Sector_Erase(address);
}

 STD_ReturnTypeDef fls_Erase_Chip(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_CHIP_ERASE;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    SPI_FLASH_CS_HIGH();
    return E_OK;
}
 STD_ReturnTypeDef fls_Erase_Suspend(void)  
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_ERASE_SUSPEND;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    SPI_FLASH_CS_HIGH();
    return E_OK;
}
 STD_ReturnTypeDef fls_Erase_Resume(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_ERASE_RESUME;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    SPI_FLASH_CS_HIGH();
    return E_OK;  
}
 STD_ReturnTypeDef fls_PowerDown(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_POWER_DOWN;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    SPI_FLASH_CS_HIGH();
    return E_OK;
}
 STD_ReturnTypeDef fls_ReleasePowerDown(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_RELEASE_POWER_DOWN;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    SPI_FLASH_CS_HIGH();
    return E_OK;
}

STD_ReturnTypeDef fls_SetNormalMode(void)
{
    return E_OK;
}

STD_ReturnTypeDef fls_SetQuadMode(void)
{
    // Implement logic to set the flash memory to quad mode if required
    return E_OK;
}


STD_ReturnTypeDef fls_wq25xx_Is_Writable( void)
{
    uint8_t status = fls_wq25xx_Read_Status_Register1();
    if (status & STATUS_REG1_WEL_MASK) {
        return E_OK; // Flash is writable
    } else {
        return E_NOT_OK; // Flash is not writable
    }
}

