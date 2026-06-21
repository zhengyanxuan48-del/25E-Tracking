#ifndef HARDWARE_MOTOR_OPEN_LOOP_TEST_H_
#define HARDWARE_MOTOR_OPEN_LOOP_TEST_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void MotorOpenLoopTest_Init(uint8_t oled_enabled);
void MotorOpenLoopTest_Update(uint32_t now_ms);
uint8_t MotorOpenLoopTest_IsDone(void);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_MOTOR_OPEN_LOOP_TEST_H_ */
