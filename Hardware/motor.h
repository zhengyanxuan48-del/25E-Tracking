#ifndef HARDWARE_MOTOR_H
#define HARDWARE_MOTOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MOTOR_LEFT_REVERSE
#define MOTOR_LEFT_REVERSE   (1) /* Positive PWM should drive left wheel forward. */
#endif

#ifndef MOTOR_RIGHT_REVERSE
#define MOTOR_RIGHT_REVERSE  (1) /* Positive PWM should drive right wheel forward. */
#endif

#define MOTOR_PWM_MIN        (-1000)
#define MOTOR_PWM_MAX        (1000)

typedef enum {
    MOTOR_LEFT = 0,
    MOTOR_RIGHT = 1
} MotorId;

void Motor_Init(void);
void Motor_SetPWM(MotorId id, int16_t pwm);
void Motor_SetBoth(int16_t left_pwm, int16_t right_pwm);
void Motor_Stop(MotorId id);
void Motor_StopAll(void);
void Motor_Brake(MotorId id);

/* Compatibility wrappers for older project code. */
void MotorA_Dir(int direction);
void MotorB_Dir(int direction);
void MotorA_Set(int direction, int pwm);
void MotorB_Set(int direction, int pwm);

#ifdef __cplusplus
}
#endif

#endif
