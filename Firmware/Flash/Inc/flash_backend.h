#ifndef FLASH_BACKEND_H
#define FLASH_BACKEND_H

#include "flash.h"

/*
 * Backend contract used by the generic flash worker. A backend implements
 * device initialization and synchronous low-level operations, including
 * waiting until each erase/program operation is complete. The generic layer
 * owns request scheduling, job state, and sector-preserving writes.
 */
typedef struct {
    STD_ReturnTypeDef (*Init)(Fls_ConfigTypeDef *config);
    STD_ReturnTypeDef (*Read)(uint32_t address, uint8_t *data, uint16_t length);
    STD_ReturnTypeDef (*ProgramPage)(uint32_t address, const uint8_t *data,
                                     uint16_t length);
    STD_ReturnTypeDef (*EraseSector)(uint32_t address);
    STD_ReturnTypeDef (*EraseChip)(void);
    STD_ReturnTypeDef (*SetMode)(uint8_t mode);
} Flash_BackendTypeDef;

extern const Flash_BackendTypeDef FlashBackend;

#endif
