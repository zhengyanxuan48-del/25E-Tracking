#ifndef HARDWARE_ENCODER_H
#define HARDWARE_ENCODER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Placeholder defaults. Replace with:
 * motor_encoder_pulses_per_rev * 4 * gearbox_ratio.
 */
#ifndef ENCODER_LEFT_COUNTS_PER_REV
#define ENCODER_LEFT_COUNTS_PER_REV   (1560L)
#endif

#ifndef ENCODER_RIGHT_COUNTS_PER_REV
#define ENCODER_RIGHT_COUNTS_PER_REV  (1560L)
#endif

#ifndef ENCODER_LEFT_REVERSE
#define ENCODER_LEFT_REVERSE          (0)
#endif

/* Right wheel forward movement currently produces negative counts. */
#ifndef ENCODER_RIGHT_REVERSE
#define ENCODER_RIGHT_REVERSE         (1)
#endif

typedef enum {
    ENCODER_LEFT = 0,
    ENCODER_RIGHT = 1
} EncoderId;

void Encoder_Init(void);
void Encoder_Reset(EncoderId id);
int32_t Encoder_GetCount(EncoderId id);
void Encoder_Update(uint32_t dt_ms);
int32_t Encoder_GetRPM(EncoderId id);
int32_t Encoder_GetSpeedCPS(EncoderId id);

/* Compatibility wrappers for older project code. */
int16_t Get_EncoderA_Count(void);
int16_t Get_EncoderB_Count(void);

#ifdef __cplusplus
}
#endif

#endif
