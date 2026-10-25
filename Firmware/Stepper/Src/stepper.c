#include "stepper.h"

/**
 * @brief Check whether a GPIO mask contains exactly one pin.
 * @param pin HAL GPIO pin mask.
 * @return Nonzero if pin is a single-bit mask.
 */
static uint8_t Stepper_IsSinglePin(uint16_t pin)
{
  return (uint8_t)(pin != 0U && (pin & (uint16_t)(pin - 1U)) == 0U);
}

/**
 * @brief Disable interrupts while preserving the caller's prior mask state.
 * @param primask Receives the prior PRIMASK value.
 */
static void Stepper_EnterCritical(uint32_t *primask)
{
  *primask = __get_PRIMASK();
  __disable_irq();
}

/**
 * @brief Restore the caller's interrupt state after a critical section.
 * @param primask PRIMASK value saved by Stepper_EnterCritical().
 */
static void Stepper_ExitCritical(uint32_t primask)
{
  if (primask == 0U)
  {
    __enable_irq();
  }
}

/** @copydoc Stepper_Init */
Stepper_Result Stepper_Init(Stepper_Handle *handle, const Stepper_Config *config)
{
  if (handle == NULL || config == NULL ||
      config->step_port == NULL || !Stepper_IsSinglePin(config->step_pin) ||
      config->direction_port == NULL || !Stepper_IsSinglePin(config->direction_pin) ||
      config->tick_frequency_hz < 2U ||
      (config->positive_direction_level != GPIO_PIN_RESET &&
       config->positive_direction_level != GPIO_PIN_SET) ||
      (config->enable_active_level != GPIO_PIN_RESET &&
       config->enable_active_level != GPIO_PIN_SET) ||
      ((config->enable_port == NULL) != (config->enable_pin == 0U)) ||
      (config->enable_port != NULL && !Stepper_IsSinglePin(config->enable_pin)))
  {
    return STEPPER_RESULT_INVALID_ARGUMENT;
  }

  handle->step_port = config->step_port;
  handle->step_pin = config->step_pin;
  handle->direction_port = config->direction_port;
  handle->direction_pin = config->direction_pin;
  handle->enable_port = config->enable_port;
  handle->enable_pin = config->enable_pin;
  handle->positive_direction_level = config->positive_direction_level;
  handle->enable_active_level = config->enable_active_level;
  handle->tick_frequency_hz = config->tick_frequency_hz;
  handle->state = STEPPER_STATE_IDLE;
  handle->speed_steps_per_second = 0U;
  handle->remaining_steps = 0U;
  handle->phase = 0U;
  handle->position_steps = 0;
  handle->direction = 1;
  handle->pulse_high = 0U;
  handle->completion_pending = 0U;
  handle->initialized = 1U;

  /* Begin with STEP low and the optional active-low/high enable deasserted. */
  HAL_GPIO_WritePin(handle->step_port, handle->step_pin, GPIO_PIN_RESET);
  if (handle->enable_port != NULL)
  {
    HAL_GPIO_WritePin(handle->enable_port, handle->enable_pin,
                      (handle->enable_active_level == GPIO_PIN_SET)
                          ? GPIO_PIN_RESET
                          : GPIO_PIN_SET);
  }

  return STEPPER_RESULT_OK;
}

/** @copydoc Stepper_Move */
Stepper_Result Stepper_Move(Stepper_Handle *handle, int32_t steps,
                            uint32_t speed_steps_per_second)
{
  uint32_t primask;
  uint32_t step_count;

  if (handle == NULL || handle->initialized == 0U || steps == 0 ||
      speed_steps_per_second == 0U ||
      speed_steps_per_second > (handle->tick_frequency_hz / 2U))
  {
    return (handle != NULL && handle->initialized == 0U)
               ? STEPPER_RESULT_NOT_INITIALIZED
               : STEPPER_RESULT_INVALID_ARGUMENT;
  }

  Stepper_EnterCritical(&primask);
  if (handle->state == STEPPER_STATE_MOVING)
  {
    Stepper_ExitCritical(primask);
    return STEPPER_RESULT_BUSY;
  }

  step_count = (steps < 0) ? (uint32_t)(-(int64_t)steps) : (uint32_t)steps;
  handle->direction = (steps < 0) ? -1 : 1;
  handle->speed_steps_per_second = speed_steps_per_second;
  handle->remaining_steps = step_count;
  handle->phase = 0U;
  handle->pulse_high = 0U;
  handle->completion_pending = 0U;
  /* Set direction before starting pulses so the driver sees a setup interval. */
  HAL_GPIO_WritePin(handle->direction_port, handle->direction_pin,
                    (handle->direction > 0)
                        ? handle->positive_direction_level
                        : (handle->positive_direction_level == GPIO_PIN_SET
                               ? GPIO_PIN_RESET
                               : GPIO_PIN_SET));
  if (handle->enable_port != NULL)
  {
    HAL_GPIO_WritePin(handle->enable_port, handle->enable_pin,
                      handle->enable_active_level);
  }
  handle->state = STEPPER_STATE_MOVING;
  Stepper_ExitCritical(primask);
  return STEPPER_RESULT_OK;
}

/** @copydoc Stepper_Stop */
Stepper_Result Stepper_Stop(Stepper_Handle *handle)
{
  uint32_t primask;

  if (handle == NULL)
  {
    return STEPPER_RESULT_INVALID_ARGUMENT;
  }
  if (handle->initialized == 0U)
  {
    return STEPPER_RESULT_NOT_INITIALIZED;
  }

  Stepper_EnterCritical(&primask);
  HAL_GPIO_WritePin(handle->step_port, handle->step_pin, GPIO_PIN_RESET);
  handle->pulse_high = 0U;
  handle->completion_pending = 0U;
  handle->remaining_steps = 0U;
  handle->state = STEPPER_STATE_IDLE;
  Stepper_ExitCritical(primask);
  return STEPPER_RESULT_OK;
}

/** @copydoc Stepper_SetEnabled */
Stepper_Result Stepper_SetEnabled(Stepper_Handle *handle, uint8_t enabled)
{
  if (handle == NULL)
  {
    return STEPPER_RESULT_INVALID_ARGUMENT;
  }
  if (handle->initialized == 0U)
  {
    return STEPPER_RESULT_NOT_INITIALIZED;
  }
  if (enabled > 1U)
  {
    return STEPPER_RESULT_INVALID_ARGUMENT;
  }
  if (handle->enable_port != NULL)
  {
    HAL_GPIO_WritePin(handle->enable_port, handle->enable_pin,
                      enabled != 0U
                          ? handle->enable_active_level
                          : (handle->enable_active_level == GPIO_PIN_SET
                                 ? GPIO_PIN_RESET
                                 : GPIO_PIN_SET));
  }
  return STEPPER_RESULT_OK;
}

/** @copydoc Stepper_OnTimerTick */
void Stepper_OnTimerTick(Stepper_Handle *handle)
{
  if (handle == NULL || handle->initialized == 0U ||
      handle->state != STEPPER_STATE_MOVING)
  {
    return;
  }

  if (handle->pulse_high != 0U)
  {
    HAL_GPIO_WritePin(handle->step_port, handle->step_pin, GPIO_PIN_RESET);
    handle->pulse_high = 0U;
    if (handle->completion_pending != 0U)
    {
      handle->completion_pending = 0U;
      handle->state = STEPPER_STATE_COMPLETE;
    }
    else
    {
      handle->phase += handle->speed_steps_per_second;
    }
    return;
  }

  /* Accumulate requested pulse rate against the timer rate to avoid rounding
   * the requested speed to an integer number of timer periods per step. */
  handle->phase += handle->speed_steps_per_second;
  if (handle->phase < handle->tick_frequency_hz)
  {
    return;
  }

  handle->phase -= handle->tick_frequency_hz;
  HAL_GPIO_WritePin(handle->step_port, handle->step_pin, GPIO_PIN_SET);
  handle->pulse_high = 1U;
  handle->position_steps += handle->direction;
  handle->remaining_steps--;
  if (handle->remaining_steps == 0U)
  {
    handle->completion_pending = 1U;
  }
}

/** @copydoc Stepper_GetState */
Stepper_State Stepper_GetState(const Stepper_Handle *handle)
{
  uint32_t primask;
  Stepper_State state;

  if (handle == NULL || handle->initialized == 0U)
  {
    return STEPPER_STATE_INVALID;
  }
  Stepper_EnterCritical(&primask);
  state = handle->state;
  Stepper_ExitCritical(primask);
  return state;
}

/** @copydoc Stepper_GetPosition */
Stepper_Result Stepper_GetPosition(const Stepper_Handle *handle, int64_t *position)
{
  uint32_t primask;

  if (handle == NULL || position == NULL)
  {
    return STEPPER_RESULT_INVALID_ARGUMENT;
  }
  if (handle->initialized == 0U)
  {
    return STEPPER_RESULT_NOT_INITIALIZED;
  }
  Stepper_EnterCritical(&primask);
  *position = handle->position_steps;
  Stepper_ExitCritical(primask);
  return STEPPER_RESULT_OK;
}
