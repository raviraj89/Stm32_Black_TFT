# STEP/DIR stepper driver

This driver targets external STEP/DIR modules such as A4988 or DRV8825. GPIO
pins must be configured as outputs by the application. The periodic timer is
also application-owned; do not use TIM13, which provides the STM32 HAL timebase
in this project.

Initialize a `Stepper_Handle` with the GPIO ports/pins and timer interrupt
frequency. In the periodic timer callback, call `Stepper_OnTimerTick()` once
per timer update. The configured frequency must be at least twice the requested
step rate; the STEP output is high for one timer period and low for at least one
period. `Stepper_Move()` schedules a move and returns immediately. Poll
`Stepper_GetState()` for `STEPPER_STATE_COMPLETE`, or call `Stepper_Stop()` to
stop early. The driver enables the module automatically at the start of a move
and leaves it enabled after completion to maintain holding torque.

The position counter is relative; it does not represent an absolute mechanical
position until the application establishes a reference such as a homing switch.

Example integration for a dedicated timer:

```c
static Stepper_Handle motor;

void StartMotor(void)
{
  const Stepper_Config config = {
      .step_port = MOTOR_STEP_GPIO_Port,
      .step_pin = MOTOR_STEP_Pin,
      .direction_port = MOTOR_DIR_GPIO_Port,
      .direction_pin = MOTOR_DIR_Pin,
      .enable_port = MOTOR_EN_GPIO_Port,
      .enable_pin = MOTOR_EN_Pin,
      .positive_direction_level = GPIO_PIN_SET,
      .enable_active_level = GPIO_PIN_RESET,
      .tick_frequency_hz = 20000U,
  };

  if (Stepper_Init(&motor, &config) != STEPPER_RESULT_OK ||
      HAL_TIM_Base_Start_IT(&htimX) != HAL_OK)
  {
    Error_Handler();
  }
  (void)Stepper_Move(&motor, 1600, 800U);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == htimX.Instance)
  {
    Stepper_OnTimerTick(&motor);
  }
}
```

Replace the example GPIO names and `htimX` with application-defined pins and a
timer configured for a 20 kHz update interrupt. Add the stepper branch to the
project's existing timer callback rather than defining a second callback.
