/**
 * @file flash_backend.h
 * @brief Private contract between the generic flash worker and device backend.
 */
#ifndef FLASH_BACKEND_H
#define FLASH_BACKEND_H

#include "flash.h"

/**
 * @brief Device-specific operations used by the generic flash worker.
 *
 * Operations are synchronous and report success only after the hardware
 * operation has completed. The generic layer owns request scheduling, job
 * state, and sector-preserving writes.
 */
typedef struct {
    /** @brief Initialize the device and populate its geometry and identifiers. */
    STD_ReturnTypeDef (*Init)(Fls_ConfigTypeDef *config);
    /** @brief Read a byte range from the device. */
    STD_ReturnTypeDef (*Read)(uint32_t address, uint8_t *data, uint16_t length);
    /** @brief Program bytes within one page and wait for completion. */
    STD_ReturnTypeDef (*ProgramPage)(uint32_t address, const uint8_t *data,
                                     uint16_t length);
    /** @brief Erase the sector containing the address and wait for completion. */
    STD_ReturnTypeDef (*EraseSector)(uint32_t address);
    /** @brief Erase the whole device and wait for completion. */
    STD_ReturnTypeDef (*EraseChip)(void);
    /** @brief Set the device bus mode when supported. */
    STD_ReturnTypeDef (*SetMode)(uint8_t mode);
} Flash_BackendTypeDef;

/** @brief Device backend linked into the firmware. */
extern const Flash_BackendTypeDef FlashBackend;

#endif
