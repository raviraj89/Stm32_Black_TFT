/**
  ******************************************************************************
  * File Name		: flash_wq25xx.h
  * Description		: This file provides code to interface the SPI based Flash driver
  ******************************************************************************
  */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FLASH_WQ25XX_H
#define __FLASH_WQ25XX_H


/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include "stm32f4xx_hal.h"

/* Constants -----------------------------------------------------------------*/
#define SPI_FLASH_CS_LOW()                               HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET)
#define SPI_FLASH_CS_HIGH()                              HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET)
#define SPI_MODE_USED                                    0 // SPI Clock Mode 0 (CPOL = 0, CPHA = 0)



#define FLASH_W25QXX_TIMEOUT                             1000
#define FLASH_W25QXX_PAGE_SIZE                           256
#define FLASH_W25QXX_SECTOR_SIZE                         4096
#define FLASH_W25QXX_BLOCK_SIZE                          65536
#define FLASH_W25QXX_FLASH_SIZE                          2097152
#define FLASH_W25QXX_FLASH_SIZE_KB                       2048
#define FLASH_W25QXX_FLASH_SIZE_MB                       2
#define FLASH_W25QXX_START_ADDRESS                       0x000000
#define FLASH_W25QXX_END_ADDRESS                         0x1FFFFF
#define FLASH_STATE_IDLE                                 0
#define FLASH_STATE_BUSY                                 1


#define NB_LED	2
#define LED2	0
#define LED3	1

#define NB_KEY	2
#define KEY0	0
#define KEY1 	1

/* Instruction Set-------------------------------------------------------------*/
#define FLASH_WRITE_ENABLE                               0x06
#define FLASH_WRITE_DISABLE                              0x04
#define FLASH_READ_STATUS_REG1                           0x05
#define FLASH_READ_STATUS_REG2                           0x35
#define FLASH_WRT_STATUS_PROG                            0x01
#define FLASH_PAGE_PROGRAM                               0x02
#define FLASH_QUAD_PAGE_PROGRAM                          0x32
#define FLASH_BLK_ERASE_64                               0xd8
#define FLASH_BLK_ERASE_32                               0x52
#define FLASH_SECTOR_ERASE                               0x20
#define FLASH_CHIP_ERASE                                 0xc7
#define FLASH_ERASE_SUSPEND                              0x75
#define FLASH_ERASE_RESUME                               0x7a
#define FLASH_POWER_DOWN                                 0xB9
#define FLASH_HIGH_PERFORMANCE_MODE                      0xa3
#define FLASH_MODE_BIT_RESET                             0xff
#define FLASH_RELEASE_POWER_DOWN                         0xab
#define FLASH_MANUFACTURER_ID                            0x90
#define FLASH_READ_UNIQUE_ID                             0x4b
#define FLASH_JEDEC_ID                                   0x9f
#define FLASH_READ_DATA                                  0x03
#define FLASH_FAST_READ                                  0x0b
#define FLASH_FAST_READ_DUAL_OUTPUT                      0x3b
#define FLASH_READ_DUAL_IO                               0xbb
#define FLASH_FAST_READ_QUAD_OUTPUT                      0x6b
#define FLASH_READ_QUAD_IO                               0xeb

/* REGISTERS ------------------------------------------------------------------*/
#define STATUS_REG1_BUSY_MASK                            0x01   // 1 = device is busy with a write/erase cycle
#define STATUS_REG1_WEL_MASK                             0x02   // 1 = device is write enabled
#define STATUS_REG1_BP_MASK                              0x1c   // Block Protect bits
#define STATUS_REG1_TB_MASK                              0x20   // 1 = Top/Bottom bit
#define STATUS_REG1_SEC_MASK                             0x40   // 1 = Sector/Block Protect bit
#define STATUS_REG1_SRP1_MASK                            0x80   // 1 = Status Register Protect1 bit  
#define STATUS_REG2_SRP2_MASK                            0x01   // 1 = Status Register Protect2 bit
#define STATUS_REG2_QE_MASK                              0x02   // 1 = Quad Enable bit




/* Structures -----------------------------------------------------------------*/

typedef enum {
    E_OK = 0,
    E_NOT_OK = 1,   
} STD_ReturnTypeDef;

typedef enum {
    FLS_UNINITIALIZED = 0,
    FLS_IDLE          = 1,
    FLS_BUSY          = 2,
} FLS_StatusTypeDef;

typedef enum {
    FLS_JOB_OK      = 0,
    FLS_JOB_FAILED  = 1,
    FLS_JOB_PENDING = 2,
} FLS_JobStatusTypeDef;

typedef enum {
    NORMAL_MODE = 0,
    QUAD_MODE   = 1,
} FLS_ModeTypeDef;

typedef struct {
    uint32_t FlashSize;                    // Size of the flash memory in bytes
    uint32_t PageSize;                     // Size of a page in bytes
    uint32_t SectorSize;                   // Size of a sector in bytes
    uint32_t BlockSize;                    // Size of a block in bytes
    uint32_t StartAddress;                 // Start address of the flash memory
    uint32_t EndAddress;                   // End address of the flash memory
    FLS_StatusTypeDef  State;              // Current state of the flash memory (idle, busy, etc.)
    FLS_JobStatusTypeDef  JobStatus;       // Status of the last job (ok, failed, pending)
    FLS_ModeTypeDef  Mode;                 // Current mode of the flash memory (normal, high performance, etc.)
    //uint8_t  JEDEC_ID[3];                // JEDEC ID of the flash memory
    struct {
        uint8_t  ManufacturerID;           // Manufacturer ID of the flash memory
        uint16_t MemoryType;               // Memory type of the flash memory
        uint8_t  Capacity;                 // Capacity of the flash memory
    } FlashInfo;
} Fls_ConfigTypeDef;


typedef struct {
    uint8_t  vendorID;           // Vendor ID of the flash memory
    uint16_t moduleID;           // Module ID of the flash memory
    uint16_t module_Size;        // Module size of the flash memory in KB
    uint8_t  sw_major_version;   // Software major version
    uint8_t  sw_minor_version;   // Software minor version
    uint8_t  sw_patch_version;   // Software patch version
} Fls_VersionInfoTypeDef;

/* Functions -----------------------------------------------------------------*/
STD_ReturnTypeDef fls_Init(void);
STD_ReturnTypeDef fls_setMode(uint8_t mode);
STD_ReturnTypeDef fls_Erase_Sector(uint32_t address);
STD_ReturnTypeDef fls_Erase_Chip(void);
STD_ReturnTypeDef fls_Erase_Suspend(void);
STD_ReturnTypeDef fls_Erase_Resume(void);
STD_ReturnTypeDef fls_PowerDown(void);
STD_ReturnTypeDef fls_ReleasePowerDown(void);
STD_ReturnTypeDef fls_Cancel_Job(void);
FLS_StatusTypeDef fls_GetStatus(void);
FLS_JobStatusTypeDef fls_GetJobResult(void);
STD_ReturnTypeDef fls_GetVersionInfo(Fls_VersionInfoTypeDef *versionInfo);

STD_ReturnTypeDef fls_Read(uint32_t address, uint8_t *data, uint16_t length);
STD_ReturnTypeDef fls_Write(uint32_t address, uint8_t *data, uint16_t length);
STD_ReturnTypeDef fls_Erase(uint32_t address, uint16_t length);
STD_ReturnTypeDef fls_Compare(uint32_t address, uint8_t *data, uint16_t length);
STD_ReturnTypeDef fls_BlankCheck(uint32_t address, uint16_t length);
/*
 * E_OK from an operation means the asynchronous job was accepted. Call
 * fls_MainFunction() regularly and check fls_GetJobResult() for completion.
 * DMA buffers must remain valid and DMA-accessible until the job completes.
 */
void fls_MainFunction(void);

STD_ReturnTypeDef fls_wq25xx_Read_JEDEC_ID(void);
STD_ReturnTypeDef fls_wq25xx_Read_Status_Register1(uint8_t *status);
STD_ReturnTypeDef fls_wq25xx_Read_Status_Register2(uint8_t *status);
STD_ReturnTypeDef fls_wq25xx_Sector_Erase(uint32_t address);
STD_ReturnTypeDef fls_wq25xx_Write_Enable(void);
STD_ReturnTypeDef fls_wq25xx_Write_Disable(void);
STD_ReturnTypeDef fls_wq25xx_Read_Data(uint32_t address, uint8_t *data, uint16_t length);
STD_ReturnTypeDef fls_wq25xx_Write_Data(uint32_t address, uint8_t *data, uint16_t length);
void fls_SPI_TransmitCompleteCallback(void);
void fls_SPI_ReceiveCompleteCallback(void);
void fls_SPI_ErrorCallback(void);
STD_ReturnTypeDef fls_SetNormalMode(void);
STD_ReturnTypeDef fls_SetQuadMode(void);


#endif // /*__FLASH_WQ25XX_H */