# Flash driver layers

The flash module is split into a device-independent asynchronous layer and a
device backend:

- [`flash.h`](./Inc/flash.h) declares the generic job API, state, and device
  geometry types.
- [`flash.c`](./Src/flash.c) validates and schedules jobs, reports completion,
  implements compare/blank-check, and preserves untouched data during writes
  using sector read-modify-erase-rewrite.
- [`flash_backend.h`](./Inc/flash_backend.h) defines the private backend
  contract. Backend operations are synchronous; the generic worker runs them
  outside the submitting task.
- [`wq25xx.h`](../Flash_W25QXX/Inc/wq25xx.h) and
  [`wq25xx.c`](../Flash_W25QXX/Src/wq25xx.c) implement W25Qxx SPI commands,
  geometry, and JEDEC detection.
- [`flash_wq25xx.h`](../Flash_W25QXX/Inc/flash_wq25xx.h) is a compatibility
  include for code using the original combined header.

Call `fls_Init()` before RTOS initialization and `fls_StartWorker()` after
`osKernelInitialize()`. High-level operations return `E_OK` when accepted;
poll `fls_GetStatus()` and then inspect `fls_GetJobResult()`. Only one request
may be active. Any caller-owned data buffer must remain valid until the job
finishes.

The generic core currently reserves a 4 KiB sector buffer, so backends must
report sectors no larger than 4 KiB and page sizes that divide the sector size.
The selected backend must provide the single `FlashBackend` definition declared
by `flash_backend.h`; link only one backend implementation into the firmware.

## Application self-test

`Application/Src/flash_test.cpp` is an RTOS-task example that exercises
asynchronous read, write, erase, blank-check, compare, and partial-write
preservation. It backs up the device's final 4 KiB sector at `0x1FF000`,
temporarily erases and tests it, then restores and compares the saved contents.
In the current application, press KEY1 to run the test; LED2 indicates success
and LED3 indicates failure. A power loss during the test can interrupt sector
restoration, so reserve the test sector if its contents are important.
