/**
 * @file stepper.h
 * @brief Non-blocking STEP/DIR stepper-motor control API.
 */

#ifndef STEPPER_H
#define STEPPER_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/**
 * @brief Result of a stepper API call.
 */
typedef enum
{
  STEPPER_RESULT_OK = 0,
  STEPPER_RESULT_INVALID_ARGUMENT,
  STEPPER_RESULT_NOT_INITIALIZED,
  STEPPER_RESULT_BUSY
} Stepper_Result;

/**
 * @brief Current state of a stepper move.
 */
typedef enum
{
  STEPPER_STATE_INVALID = 0,
  STEPPER_STATE_IDLE,
  STEPPER_STATE_MOVING,
  STEPPER_STATE_COMPLETE
} Stepper_State;

/**
 * @brief Hardware and timer configuration for a STEP/DIR driver.
 *
 * GPIO pins must be configured as outputs by the application. A timer must
 * call Stepper_OnTimerTick() at tick_frequency_hz.
 */
typedef struct
{
  GPIO_TypeDef *step_port;                 /**< GPIO port connected to STEP. */
  uint16_t step_pin;                       /**< Single GPIO pin connected to STEP. */
  GPIO_TypeDef *direction_port;            /**< GPIO port connected to DIR. */
  uint16_t direction_pin;                  /**< Single GPIO pin connected to DIR. */
  GPIO_TypeDef *enable_port;               /**< ENABLE port, or NULL when unused. */
  uint16_t enable_pin;                     /**< ENABLE pin, or zero when unused. */
  GPIO_PinState positive_direction_level;  /**< DIR level for positive moves. */
  GPIO_PinState enable_active_level;       /**< Active polarity of ENABLE. */
  uint32_t tick_frequency_hz;              /**< Periodic timer update frequency. */
} Stepper_Config;

/**
 * @brief Mutable state for one stepper motor.
 *
 * Allocate one handle per motor and retain it while the timer callback can
 * call Stepper_OnTimerTick(). Initialize it before other API calls.
 */
typedef struct
{
  GPIO_TypeDef *step_port;                  /**< STEP GPIO port. */
  uint16_t step_pin;                        /**< STEP GPIO pin. */
  GPIO_TypeDef *direction_port;             /**< DIR GPIO port. */
  uint16_t direction_pin;                   /**< DIR GPIO pin. */
  GPIO_TypeDef *enable_port;                /**< ENABLE GPIO port, or NULL. */
  uint16_t enable_pin;                      /**< ENABLE GPIO pin, or zero. */
  GPIO_PinState positive_direction_level;   /**< DIR level for positive moves. */
  GPIO_PinState enable_active_level;        /**< Active ENABLE level. */
  uint32_t tick_frequency_hz;               /**< Timer update frequency. */
  volatile Stepper_State state;             /**< Current move state. */
  volatile uint32_t speed_steps_per_second; /**< Requested pulse rate. */
  volatile uint32_t remaining_steps;        /**< Pulses remaining in the move. */
  volatile uint64_t phase;                  /**< Rate-generator accumulator. */
  volatile int64_t position_steps;          /**< Relative signed position. */
  volatile int8_t direction;                /**< +1 or -1 for the active move. */
  volatile uint8_t pulse_high;              /**< Whether STEP is currently high. */
  volatile uint8_t completion_pending;      /**< Final pulse must be lowered. */
  uint8_t initialized;                      /**< Set by Stepper_Init(). */
} Stepper_Handle;

/**
 * @brief Initialize a stepper driver interface.
 * @param handle Driver state, owned by the caller and retained for later use.
 * @param config GPIO mapping and periodic timer frequency.
 * @return STEPPER_RESULT_OK on success; otherwise an error code.
 * @note GPIO pins must already be configured as outputs. The optional enable
 * pin is set inactive during initialization.
 */
Stepper_Result Stepper_Init(Stepper_Handle *handle, const Stepper_Config *config);

/**
 * @brief Start a non-blocking move.
 * @param handle Initialized motor handle.
 * @param steps Signed step count; sign selects direction. Zero is invalid.
 * @param speed_steps_per_second Requested rate; must be nonzero and no greater
 * than half the configured timer tick frequency.
 * @return STEPPER_RESULT_OK when started, STEPPER_RESULT_BUSY if another move
 * is active, or an argument/initialization error.
 * @note The driver activates the optional ENABLE output when a move starts.
 */
Stepper_Result Stepper_Move(Stepper_Handle *handle, int32_t steps,
                            uint32_t speed_steps_per_second);

/**
 * @brief Stop motion immediately and force STEP low.
 * @param handle Initialized motor handle.
 * @return STEPPER_RESULT_OK on success; otherwise an error code.
 * @note This does not disable the motor driver.
 */
Stepper_Result Stepper_Stop(Stepper_Handle *handle);

/**
 * @brief Set the optional driver-enable output.
 * @param handle Initialized motor handle.
 * @param enabled Nonzero to enable, zero to disable.
 * @return STEPPER_RESULT_OK on success; otherwise an error code.
 * @note If no enable pin was configured, succeeds without changing hardware.
 */
Stepper_Result Stepper_SetEnabled(Stepper_Handle *handle, uint8_t enabled);

/**
 * @brief Advance the pulse generator by one timer period.
 * @param handle Initialized motor handle.
 * @note Call exactly once per configured timer update, normally from that
 * timer's interrupt callback. STEP is high for one period and low for at least
 * one period. Keep the callback path short.
 */
void Stepper_OnTimerTick(Stepper_Handle *handle);

/**
 * @brief Read the current move state.
 * @param handle Initialized motor handle.
 * @return STEPPER_STATE_INVALID for a null or uninitialized handle.
 */
Stepper_State Stepper_GetState(const Stepper_Handle *handle);

/**
 * @brief Read the driver's relative signed position in steps.
 * @param handle Initialized motor handle.
 * @param position Receives the position; must not be NULL.
 * @return STEPPER_RESULT_OK on success; otherwise an error code.
 * @note Position is relative and requires application homing to represent
 * absolute mechanical position.
 */
Stepper_Result Stepper_GetPosition(const Stepper_Handle *handle, int64_t *position);

#endif
