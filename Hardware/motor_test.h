#ifndef HARDWARE_MOTOR_TEST_H_
#define HARDWARE_MOTOR_TEST_H_

#include "MPU6050.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void MotorTest_Init(void);
void MotorTest_SetOledEnabled(uint8_t enabled);
void MotorTest_SetTelemetry(const MPU6050_Attitude *attitude, int status);
void MotorTest_SetTrackBits(uint8_t bits, int status);
void MotorTest_Update(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_MOTOR_TEST_H_ */
