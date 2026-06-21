#ifndef INC_MPU6050_H_
#define INC_MPU6050_H_

#include <stdint.h>

#define ERROR_MPU_INIT              -1
#define ERROR_SET_SENSOR            -2
#define ERROR_CONFIG_FIFO           -3
#define ERROR_SET_RATE              -4
#define ERROR_LOAD_MOTION_DRIVER    -5
#define ERROR_SET_ORIENTATION       -6
#define ERROR_ENABLE_FEATURE        -7
#define ERROR_SET_FIFO_RATE         -8
#define ERROR_SELF_TEST             -9
#define ERROR_DMP_STATE             -10

/*
 * 如果你想更稳，建议用 50Hz，对应 main.c 里面 20ms 读一次。
 * 如果后续控制需要更快，可以改回 100，然后 main.c 里 MPU_READ_MS 改成 10。
 */
#define DEFAULT_MPU_HZ  50

#define MPU_SAMPLE_RATE_HZ  (500U)

#if MPU_SAMPLE_RATE_HZ == 1000U
#define MPU_SMPLRT_DIV        (0x00U)
#define MPU_SAMPLE_PERIOD_MS  (1U)
#elif MPU_SAMPLE_RATE_HZ == 500U
#define MPU_SMPLRT_DIV        (0x01U)
#define MPU_SAMPLE_PERIOD_MS  (2U)
#elif MPU_SAMPLE_RATE_HZ == 200U
#define MPU_SMPLRT_DIV        (0x04U)
#define MPU_SAMPLE_PERIOD_MS  (5U)
#else
#error "Unsupported MPU_SAMPLE_RATE_HZ"
#endif

#define Q30  1073741824.0f

typedef struct {
    uint8_t who_am_i;
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t temp;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    int16_t gz_raw;
} MPU6050_RawData;

typedef struct {
    int16_t roll_x10;
    int16_t pitch_x10;
    int16_t yaw_x10;
    int16_t gz_dps_x10;
    int16_t gz_dps_x100;
    int16_t drift_dps_x100;
    uint8_t yaw_still;
    uint8_t yaw_ready;
} MPU6050_Attitude;

int MPU6050_ReadWhoAmI(uint8_t *who_am_i);
int MPU6050_Wake(void);
int MPU6050_InitBasic(void);
int MPU6050_CalibrateStatic(uint16_t samples);
int MPU6050_ReadRaw(MPU6050_RawData *data);
int MPU6050_ReadRawData(MPU6050_RawData *data);
int MPU6050_MinimalVerify(MPU6050_RawData *data);
void MPU6050_ResetYaw(void);
int MPU6050_UpdateAttitude(
    const MPU6050_RawData *data, float dt_sec, MPU6050_Attitude *attitude);

int MPU6050_DMP_init(void);
int MPU6050_DMP_Get_Date(float *pitch, float *roll, float *yaw);
void MPU6050_DMP_ResetFIFO(void);
float MPU6050_GetYaw(void);

#endif /* INC_MPU6050_H_ */
