#include "motor.h"

#include "ti_msp_dl_config.h"

#define MOTOR_PWM_PERIOD_COUNTS  (1600U)

#define MOTOR_RIGHT_IN1_PORT     AIN1_PORT
#define MOTOR_RIGHT_IN1_PIN      AIN1_PIN_AIN1_PIN
#define MOTOR_RIGHT_IN2_PORT     AIN2_PORT
#define MOTOR_RIGHT_IN2_PIN      AIN2_PIN_AIN2_PIN
#define MOTOR_RIGHT_PWM_INDEX    GPIO_PWM_C1_IDX  /* PA16, TB6612 PWMA */

/*
 * Schematic: BIN1 -> PA12, BIN2 -> PA13.
 * Current SysConfig labels PA12 as BIN2 and PA13 as BIN1, so use the
 * generated macros by their physical pins here.
 */
#define MOTOR_LEFT_IN1_PORT      BIN2_PORT
#define MOTOR_LEFT_IN1_PIN       BIN2_PIN_BIN2_PIN
#define MOTOR_LEFT_IN2_PORT      BIN1_PORT
#define MOTOR_LEFT_IN2_PIN       BIN1_PIN_BIN1_PIN
#define MOTOR_LEFT_PWM_INDEX     GPIO_PWM_C0_IDX  /* PA17, TB6612 PWMB */

typedef struct {
    GPIO_Regs *in1_port;
    uint32_t in1_pin;
    GPIO_Regs *in2_port;
    uint32_t in2_pin;
    uint32_t pwm_index;
    uint8_t reverse;
} MotorHw;

static const MotorHw g_motor_hw[] = {
    {
        MOTOR_LEFT_IN1_PORT,
        MOTOR_LEFT_IN1_PIN,
        MOTOR_LEFT_IN2_PORT,
        MOTOR_LEFT_IN2_PIN,
        MOTOR_LEFT_PWM_INDEX,
        MOTOR_LEFT_REVERSE
    },
    {
        MOTOR_RIGHT_IN1_PORT,
        MOTOR_RIGHT_IN1_PIN,
        MOTOR_RIGHT_IN2_PORT,
        MOTOR_RIGHT_IN2_PIN,
        MOTOR_RIGHT_PWM_INDEX,
        MOTOR_RIGHT_REVERSE
    }
};

static int16_t motor_limit_pwm(int16_t pwm)
{
    if (pwm > MOTOR_PWM_MAX) {
        return MOTOR_PWM_MAX;
    }
    if (pwm < MOTOR_PWM_MIN) {
        return MOTOR_PWM_MIN;
    }
    return pwm;
}

static uint16_t motor_abs_pwm(int16_t pwm)
{
    if (pwm < 0) {
        return (uint16_t) -pwm;
    }
    return (uint16_t) pwm;
}

static uint32_t motor_get_pwm_period(void)
{
    uint32_t load = DL_TimerA_getLoadValue(PWM_INST);

    if (load == 0U) {
        return MOTOR_PWM_PERIOD_COUNTS;
    }

    return load + 1U;
}

static uint32_t motor_pwm_to_compare(uint16_t abs_pwm)
{
    uint32_t period = motor_get_pwm_period();
    uint32_t high_counts;

    if (abs_pwm > MOTOR_PWM_MAX) {
        abs_pwm = MOTOR_PWM_MAX;
    }

    high_counts = ((uint32_t) abs_pwm * period) / MOTOR_PWM_MAX;
    if (high_counts >= period) {
        return 0U;
    }

    return period - high_counts;
}

static const MotorHw *motor_get_hw(MotorId id)
{
    if ((id != MOTOR_LEFT) && (id != MOTOR_RIGHT)) {
        return 0;
    }

    return &g_motor_hw[(uint32_t) id];
}

static void motor_set_pwm_compare(const MotorHw *hw, uint16_t abs_pwm)
{
    if (hw == 0) {
        return;
    }

    DL_TimerA_setCaptureCompareValue(
        PWM_INST, motor_pwm_to_compare(abs_pwm), hw->pwm_index);
}

static void motor_set_direction_pins(const MotorHw *hw, int8_t direction)
{
    if (hw == 0) {
        return;
    }

    if (direction > 0) {
        DL_GPIO_setPins(hw->in1_port, hw->in1_pin);
        DL_GPIO_clearPins(hw->in2_port, hw->in2_pin);
    } else if (direction < 0) {
        DL_GPIO_clearPins(hw->in1_port, hw->in1_pin);
        DL_GPIO_setPins(hw->in2_port, hw->in2_pin);
    } else {
        DL_GPIO_clearPins(hw->in1_port, hw->in1_pin);
        DL_GPIO_clearPins(hw->in2_port, hw->in2_pin);
    }
}

void Motor_Init(void)
{
    Motor_StopAll();
    DL_TimerA_startCounter(PWM_INST);
}

void Motor_SetPWM(MotorId id, int16_t pwm)
{
    const MotorHw *hw = motor_get_hw(id);
    int8_t direction = 0;

    if (hw == 0) {
        return;
    }

    pwm = motor_limit_pwm(pwm);
    if (hw->reverse != 0U) {
        pwm = (int16_t) -pwm;
    }

    if (pwm > 0) {
        direction = 1;
    } else if (pwm < 0) {
        direction = -1;
    }

    motor_set_pwm_compare(hw, motor_abs_pwm(pwm));
    motor_set_direction_pins(hw, direction);
}

void Motor_SetBoth(int16_t left_pwm, int16_t right_pwm)
{
    Motor_SetPWM(MOTOR_LEFT, left_pwm);
    Motor_SetPWM(MOTOR_RIGHT, right_pwm);
}

void Motor_Stop(MotorId id)
{
    const MotorHw *hw = motor_get_hw(id);

    motor_set_pwm_compare(hw, 0U);
    motor_set_direction_pins(hw, 0);
}

void Motor_StopAll(void)
{
    Motor_Stop(MOTOR_LEFT);
    Motor_Stop(MOTOR_RIGHT);
}

void Motor_Brake(MotorId id)
{
    const MotorHw *hw = motor_get_hw(id);

    if (hw == 0) {
        return;
    }

    motor_set_pwm_compare(hw, MOTOR_PWM_MAX);
    DL_GPIO_setPins(hw->in1_port, hw->in1_pin);
    DL_GPIO_setPins(hw->in2_port, hw->in2_pin);
}

void MotorA_Dir(int direction)
{
    const MotorHw *hw = motor_get_hw(MOTOR_RIGHT);

    if (direction == 1) {
        motor_set_direction_pins(hw, 1);
    } else if (direction == 2) {
        motor_set_direction_pins(hw, -1);
    } else {
        motor_set_direction_pins(hw, 0);
    }
}

void MotorB_Dir(int direction)
{
    const MotorHw *hw = motor_get_hw(MOTOR_LEFT);

    if (direction == 1) {
        motor_set_direction_pins(hw, 1);
    } else if (direction == 2) {
        motor_set_direction_pins(hw, -1);
    } else {
        motor_set_direction_pins(hw, 0);
    }
}

void MotorA_Set(int direction, int pwm)
{
    if (direction == 2) {
        pwm = -pwm;
    } else if (direction != 1) {
        pwm = 0;
    }

    Motor_SetPWM(MOTOR_RIGHT, (int16_t) pwm);
}

void MotorB_Set(int direction, int pwm)
{
    if (direction == 2) {
        pwm = -pwm;
    } else if (direction != 1) {
        pwm = 0;
    }

    Motor_SetPWM(MOTOR_LEFT, (int16_t) pwm);
}
