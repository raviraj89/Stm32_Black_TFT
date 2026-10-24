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



/* Functions -----------------------------------------------------------------*/
void flash_wq25xx_init(void)
{
    
}

/**
  * @brief  Enable the write operations on the flash memory.
  * @param  None
  * @retval None
  */
void flash_wq25xx_write_enable(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_WRITE_ENABLE;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, FLASH_W25QXX_TIMEOUT);
    SPI_FLASH_CS_HIGH();
}

/**
  * @brief  Disable the write operations on the flash memory.
  * @param  None
  * @retval None
  */
void flash_wq25xx_write_disable(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_WRITE_DISABLE;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, FLASH_W25QXX_TIMEOUT);
    SPI_FLASH_CS_HIGH();
}



/**
  * @brief  Erase a sector in the flash memory.
  * @param  address: The address of the sector to erase.
  * @retval None
  */
void flash_wq25xx_sector_erase(uint32_t address)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_SECTOR_ERASE;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, FLASH_W25QXX_TIMEOUT);
    // Transmit the address (3 bytes)
    HAL_SPI_Transmit(&hspi1, (uint8_t *)&address, 3, FLASH_W25QXX_TIMEOUT);
    SPI_FLASH_CS_HIGH();
}

/**
  * @brief  Erase the entire flash memory.
  * @param  None
  * @retval None
  */
void flash_wq25xx_chip_erase(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_CHIP_ERASE;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, FLASH_W25QXX_TIMEOUT);
    SPI_FLASH_CS_HIGH();
}

/**
  * @brief  Suspend the ongoing erase operation.
  * @param  None
  * @retval None
  */
void flash_wq25xx_erase_suspend(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_ERASE_SUSPEND;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, FLASH_W25QXX_TIMEOUT);
    SPI_FLASH_CS_HIGH();
}


/**
  * @brief  Resume the suspended erase operation.
  * @param  None
  * @retval None
  */
void flash_wq25xx_erase_resume(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_ERASE_RESUME;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, FLASH_W25QXX_TIMEOUT);
    SPI_FLASH_CS_HIGH();
}

/**
  * @brief  Put the flash memory in power down mode.
  * @param  None
  * @retval None
  */
void flash_wq25xx_power_down(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_POWER_DOWN;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, FLASH_W25QXX_TIMEOUT);
    SPI_FLASH_CS_HIGH();
}

/**
  * @brief  Put the flash memory in high performance mode.
  * @param  None
  * @retval None
  */
void flash_wq25xx_high_performance_mode(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_HIGH_PERFORMANCE_MODE;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, FLASH_W25QXX_TIMEOUT);
    SPI_FLASH_CS_HIGH();
}

/**
  * @brief  Reset the mode bit of the flash memory.
  * @param  None
  * @retval None
  */
void flash_wq25xx_mode_bit_reset(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_MODE_BIT_RESET;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, FLASH_W25QXX_TIMEOUT);
    SPI_FLASH_CS_HIGH();
}

/**
  * @brief  Release the flash memory from power down mode.
  * @param  None
  * @retval None
  */
void flash_wq25xx_release_power_down(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_RELEASE_POWER_DOWN;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    SPI_FLASH_CS_HIGH();
}


/**
  * @brief  Read the manufacturer ID of the flash memory.
  * @param  None
  * @retval The manufacturer ID.
  */
uint8_t flash_wq25xx_read_manufacturer_id(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_MANUFACTURER_ID;
    uint8_t txData[4] = {cmd,0x00,0x00,0x00}; // Dummy byte to clock out the ID
    HAL_SPI_Transmit_IT(&hspi1, txData, 4);
    //osDelay(10); // Delay to ensure the flash is ready to send the ID
    uint8_t id;
    while (HAL_SPI_Receive_IT(&hspi1, &id, 1) == HAL_BUSY)
    {
        // Wait for the receive to complete
    }
    SPI_FLASH_CS_HIGH();
    return id;
}

/**
  * @brief  Read the JEDEC ID of the flash memory.
  * @param  None
  * @retval The JEDEC ID.
  */
uint8_t flash_wq25xx_read_jedec_id(void)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_JEDEC_ID;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    //osDelay(10); // Delay to ensure the flash is ready to send the ID
    uint8_t id[3];
    while (HAL_SPI_Receive_IT(&hspi1, id, 3) == HAL_BUSY)
    {
        // Wait for the receive to complete
    }
    //osDelay(10); // Delay to ensure the flash is ready to send the ID
    SPI_FLASH_CS_HIGH();
    return id[0];
}


/**
  * @brief  Read the unique ID of the flash memory.
  * @param  id: Pointer to a buffer to store the unique ID (8 bytes).
  * @retval None
  */
void flash_wq25xx_read_unique_id(uint8_t *id)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_READ_UNIQUE_ID;
    HAL_SPI_Transmit_IT(&hspi1, &cmd, 1);
    HAL_SPI_Receive_IT(&hspi1, id, 8);
    SPI_FLASH_CS_HIGH();
}


/**
  * @brief  Read data from the flash memory.
  * @param  address: The starting address to read from.
  * @param  data: Pointer to a buffer to store the read data.
  * @param  length: The number of bytes to read.
  * @retval None
  */
void flash_wq25xx_read_data(uint32_t address, uint8_t *data, uint16_t length)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_READ_DATA;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, FLASH_W25QXX_TIMEOUT);
    // Transmit the address (3 bytes)
    HAL_SPI_Transmit(&hspi1, (uint8_t *)&address, 3, FLASH_W25QXX_TIMEOUT);
    HAL_SPI_Receive(&hspi1, data, length, FLASH_W25QXX_TIMEOUT);
    SPI_FLASH_CS_HIGH();
}



/**
  * @brief  Fast read data from the flash memory.
  * @param  address: The starting address to read from.
  * @param  data: Pointer to a buffer to store the read data.
  * @param  length: The number of bytes to read.
  * @retval None
  */
void flash_wq25xx_read_dual_io(uint32_t address, uint8_t *data, uint16_t length)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_READ_DUAL_IO;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, FLASH_W25QXX_TIMEOUT);
    // Transmit the address (3 bytes)
    HAL_SPI_Transmit(&hspi1, (uint8_t *)&address, 3, FLASH_W25QXX_TIMEOUT);
    HAL_SPI_Receive(&hspi1, data, length, FLASH_W25QXX_TIMEOUT);
    SPI_FLASH_CS_HIGH();
}



/**
  * @brief  Fast read data from the flash memory using quad output.
  * @param  address: The starting address to read from.
  * @param  data: Pointer to a buffer to store the read data.
  * @param  length: The number of bytes to read.
  * @retval None
  */
void flash_wq25xx_fast_read_quad_output(uint32_t address, uint8_t *data, uint16_t length)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_FAST_READ_QUAD_OUTPUT;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, FLASH_W25QXX_TIMEOUT);
    // Transmit the address (3 bytes)
    HAL_SPI_Transmit(&hspi1, (uint8_t *)&address, 3, FLASH_W25QXX_TIMEOUT);
    HAL_SPI_Receive(&hspi1, data, length, FLASH_W25QXX_TIMEOUT);
    SPI_FLASH_CS_HIGH();
}



/**
  * @brief  Read data from the flash memory using quad I/O.
  * @param  address: The starting address to read from.
  * @param  data: Pointer to a buffer to store the read data.
  * @param  length: The number of bytes to read.
  * @retval None
  */
void flash_wq25xx_read_quad_io(uint32_t address, uint8_t *data, uint16_t length)
{
    SPI_FLASH_CS_LOW();
    uint8_t cmd = FLASH_READ_QUAD_IO;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, FLASH_W25QXX_TIMEOUT);
    // Transmit the address (3 bytes)
    HAL_SPI_Transmit(&hspi1, (uint8_t *)&address, 3, FLASH_W25QXX_TIMEOUT);
    HAL_SPI_Receive(&hspi1, data, length, FLASH_W25QXX_TIMEOUT);
    SPI_FLASH_CS_HIGH();
}
