/**
 ******************************************************************************
 * @file flash_wq25xx.h
 * @brief Backward-compatible include for the generic flash API and W25Qxx.
 ******************************************************************************
 */
#ifndef FLASH_WQ25XX_H
#define FLASH_WQ25XX_H

#include "flash.h"
#include "wq25xx.h"

#define FLASH_W25QXX_TIMEOUT        WQ25XX_SPI_TIMEOUT
#define FLASH_W25QXX_PAGE_SIZE      WQ25XX_PAGE_SIZE
#define FLASH_W25QXX_SECTOR_SIZE    WQ25XX_SECTOR_SIZE
#define FLASH_W25QXX_BLOCK_SIZE     WQ25XX_BLOCK_SIZE
#define FLASH_W25QXX_FLASH_SIZE     WQ25XX_FLASH_SIZE
#define FLASH_W25QXX_FLASH_SIZE_KB  (WQ25XX_FLASH_SIZE / 1024U)
#define FLASH_W25QXX_FLASH_SIZE_MB  (WQ25XX_FLASH_SIZE / 1048576U)
#define FLASH_W25QXX_START_ADDRESS  0x000000U
#define FLASH_W25QXX_END_ADDRESS    (WQ25XX_FLASH_SIZE - 1U)
#define FLASH_STATE_IDLE            0U
#define FLASH_STATE_BUSY            1U

#endif
