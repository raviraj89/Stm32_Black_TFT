#ifndef APPLICATION_FLASH_TEST_HPP
#define APPLICATION_FLASH_TEST_HPP

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Run the flash self-test from an RTOS task. The test temporarily erases the
 * last 4 KiB sector, so it backs up and restores that sector before returning.
 */
bool FlashTest_Run(void);

#ifdef __cplusplus
}
#endif

#endif
