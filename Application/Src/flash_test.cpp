#include "flash_test.hpp"

#include <cstring>
#include "cmsis_os.h"
#include "flash.h"
#include "wq25xx.h"

namespace {

constexpr uint32_t kTestSectorAddress = WQ25XX_FLASH_SIZE - WQ25XX_SECTOR_SIZE;
constexpr uint16_t kSectorSize = WQ25XX_SECTOR_SIZE;
constexpr uint16_t kDataOffset = 128U;
constexpr uint16_t kSeedSize = 32U;
constexpr uint16_t kUpdateOffset = 11U;
constexpr uint16_t kUpdateSize = 8U;

uint8_t sectorBackup[kSectorSize];
uint8_t expectedData[kSeedSize];
uint8_t readbackData[kSeedSize];

const uint8_t seedData[kSeedSize] = {
    0x12U, 0x34U, 0x56U, 0x78U, 0x9AU, 0xBCU, 0xDEU, 0xF0U,
    0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, 0x88U,
    0xA1U, 0xB2U, 0xC3U, 0xD4U, 0xE5U, 0xF6U, 0x07U, 0x18U,
    0x29U, 0x3AU, 0x4BU, 0x5CU, 0x6DU, 0x7EU, 0x8FU, 0x90U
};

uint8_t updateData[kUpdateSize] = {
    0xFEU, 0xDCU, 0xBAU, 0x98U, 0x76U, 0x54U, 0x32U, 0x10U
};

bool waitForJob(STD_ReturnTypeDef submission)
{
    if (submission != E_OK) {
        return false;
    }

    while (fls_GetStatus() == FLS_BUSY) {
        osDelay(1U);
    }
    return fls_GetJobResult() == FLS_JOB_OK;
}

} // namespace

extern "C" bool FlashTest_Run(void)
{
    const uint32_t dataAddress = kTestSectorAddress + kDataOffset;

    if (!waitForJob(fls_Read(kTestSectorAddress, sectorBackup, kSectorSize))) {
        return false;
    }

    std::memcpy(expectedData, seedData, sizeof(expectedData));

    bool passed = waitForJob(fls_Erase_Sector(kTestSectorAddress));
    if (passed) {
        passed = waitForJob(fls_BlankCheck(kTestSectorAddress, kSectorSize));
    }
    if (passed) {
        passed = waitForJob(fls_Write(dataAddress, expectedData, kSeedSize));
    }
    if (passed) {
        std::memcpy(&expectedData[kUpdateOffset], updateData, sizeof(updateData));
        passed = waitForJob(fls_Write(dataAddress + kUpdateOffset,
                                      updateData,
                                      kUpdateSize));
    }
    if (passed) {
        passed = waitForJob(fls_Compare(dataAddress, expectedData, kSeedSize));
    }
    if (passed) {
        passed = waitForJob(fls_Read(dataAddress, readbackData, kSeedSize)) &&
                 std::memcmp(readbackData, expectedData, sizeof(expectedData)) == 0;
    }

    /* Restore the original sector even if one of the checks failed. */
    const bool restored = waitForJob(fls_Write(kTestSectorAddress,
                                               sectorBackup,
                                               kSectorSize));
    if (!restored) {
        return false;
    }
    return passed &&
           waitForJob(fls_Compare(kTestSectorAddress, sectorBackup, kSectorSize));
}
