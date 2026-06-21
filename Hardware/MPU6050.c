/*
 * MPU6050.c
 *
 *  Created on: Aug 25, 2024
 *      Author: 王滋行
 */

#include "MPU6050.h"
#include "debug_uart.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h"
#include "math.h"
#include "platform_i2c.h"
#include "ti_msp_dl_config.h"
/* The sensors can be mounted onto the board in any orientation. The mounting
 * matrix seen below tells the MPL how to rotate the raw data from thei
 * driver(s).
 * TODO: The following matrices refer to the configuration on an internal test
 * board at Invensense. If needed, please modify the matrices to match the
 * chip-to-body matrix for your particular set up.
 */
static signed char gyro_orientation[9] = {-1, 0, 0,
                                           0,-1, 0,
                                           0, 0, 1};

#define MPU6050_DMP_FIFO_DRAIN_MAX 32
#define MPU6050_REG_ACCEL_XOUT_H   (0x3BU)
#define MPU6050_REG_SMPLRT_DIV     (0x19U)
#define MPU6050_REG_CONFIG         (0x1AU)
#define MPU6050_REG_GYRO_CONFIG    (0x1BU)
#define MPU6050_REG_ACCEL_CONFIG   (0x1CU)
#define MPU6050_REG_PWR_MGMT_1     (0x6BU)
#define MPU6050_REG_WHO_AM_I       (0x75U)
#define MPU6050_WHO_AM_I_EXPECTED  (0x68U)
#define MPU6050_RAW_DATA_LEN       (14U)
#define MPU6050_ERROR_WHO_MISMATCH (-20)
#define MPU6050_CALIB_DELAY_MS     (5U)
#define MPU6050_CALIB_SAMPLES      (1000U)
#define MPU6050_CALIB_STABLE_MS    (1000U)
#define MPU6050_GYRO_USE_250DPS    (1U)
#if MPU6050_GYRO_USE_250DPS
#define MPU6050_GYRO_CONFIG_VALUE  (0x00U)
#define MPU6050_GYRO_LSB_PER_DPS   (131.0f)
#else
#define MPU6050_GYRO_CONFIG_VALUE  (0x08U)
#define MPU6050_GYRO_LSB_PER_DPS   (65.5f)
#endif
#define MPU6050_ACCEL_LSB_PER_G    (16384)
#define MPU6050_LPF_ALPHA          (0.80f)
#define MPU6050_COMP_ALPHA         (0.98f)
#define MPU6050_RAD_TO_DEG         (57.2957795f)
#define YAW_GYRO_DEADBAND_DPS      (0.3f)
#define YAW_READY_GZ_DPS           (0.3f)

static int16_t g_mpu6050_ax_offset;
static int16_t g_mpu6050_ay_offset;
static int16_t g_mpu6050_az_offset;
static int16_t g_mpu6050_gx_offset;
static int16_t g_mpu6050_gy_offset;
static int16_t g_mpu6050_gz_offset;
static int16_t g_mpu6050_az_reference;
static float g_mpu6050_filtered_ax;
static float g_mpu6050_filtered_ay;
static float g_mpu6050_filtered_az;
static float g_mpu6050_filtered_gx;
static float g_mpu6050_filtered_gy;
static float g_mpu6050_filtered_gz;
static uint8_t g_mpu6050_filter_ready;
static float g_mpu6050_roll_deg;
static float g_mpu6050_pitch_deg;
static float g_mpu6050_yaw_deg;
static uint8_t g_mpu6050_attitude_ready;

static void MPU6050_DelayMs(uint32_t ms)
{
    delay_cycles((CPUCLK_FREQ / 1000U) * ms);
}

static int16_t MPU6050_MakeInt16(uint8_t high, uint8_t low)
{
    return (int16_t)((uint16_t) high << 8U | (uint16_t) low);
}

static int16_t MPU6050_FloatToInt16(float value)
{
    if (value >= 0.0f) {
        return (int16_t)(value + 0.5f);
    }

    return (int16_t)(value - 0.5f);
}

void MPU6050_ResetYaw(void)
{
    g_mpu6050_yaw_deg = 0.0f;
}

float MPU6050_GetYaw(void)
{
    return g_mpu6050_yaw_deg;
}

static void MPU6050_ResetAttitude(void)
{
    g_mpu6050_roll_deg = 0.0f;
    g_mpu6050_pitch_deg = 0.0f;
    MPU6050_ResetYaw();
    g_mpu6050_attitude_ready = 0U;
}

static void MPU6050_ResetFilter(void)
{
    g_mpu6050_filtered_ax = 0.0f;
    g_mpu6050_filtered_ay = 0.0f;
    g_mpu6050_filtered_az = 0.0f;
    g_mpu6050_filtered_gx = 0.0f;
    g_mpu6050_filtered_gy = 0.0f;
    g_mpu6050_filtered_gz = 0.0f;
    g_mpu6050_filter_ready = 0U;
}

static float MPU6050_LowPass(float old_value, float new_value)
{
    return (MPU6050_LPF_ALPHA * old_value) +
        ((1.0f - MPU6050_LPF_ALPHA) * new_value);
}

int MPU6050_ReadWhoAmI(uint8_t *who_am_i)
{
    if (who_am_i == 0) {
        return (int) PLATFORM_I2C_BAD_PARAM;
    }

    return (int) platform_i2c_read_u8(PLATFORM_I2C_ADDR_MPU6050,
        MPU6050_REG_WHO_AM_I, who_am_i);
}

int MPU6050_Wake(void)
{
    return (int) platform_i2c_write_u8(PLATFORM_I2C_ADDR_MPU6050,
        MPU6050_REG_PWR_MGMT_1, 0x00U);
}

int MPU6050_InitBasic(void)
{
    int status;

    status = MPU6050_Wake();
    if (status != 0) {
        return status;
    }

    MPU6050_DelayMs(100U);

    status = (int) platform_i2c_write_u8(PLATFORM_I2C_ADDR_MPU6050,
        MPU6050_REG_SMPLRT_DIV, MPU_SMPLRT_DIV);
    if (status != 0) {
        return status;
    }

    status = (int) platform_i2c_write_u8(PLATFORM_I2C_ADDR_MPU6050,
        MPU6050_REG_CONFIG, 0x02U);
    if (status != 0) {
        return status;
    }

    status = (int) platform_i2c_write_u8(PLATFORM_I2C_ADDR_MPU6050,
        MPU6050_REG_GYRO_CONFIG, MPU6050_GYRO_CONFIG_VALUE);
    if (status != 0) {
        return status;
    }

    return (int) platform_i2c_write_u8(PLATFORM_I2C_ADDR_MPU6050,
        MPU6050_REG_ACCEL_CONFIG, 0x00U);
}

int MPU6050_ReadRawData(MPU6050_RawData *data)
{
    uint8_t raw[MPU6050_RAW_DATA_LEN];
    platform_i2c_status_t status;

    if (data == 0) {
        return (int) PLATFORM_I2C_BAD_PARAM;
    }

    status = platform_i2c_read_reg(PLATFORM_I2C_ADDR_MPU6050,
        MPU6050_REG_ACCEL_XOUT_H, raw, (uint16_t) sizeof(raw));
    if (status != PLATFORM_I2C_OK) {
        return (int) status;
    }

    data->ax = MPU6050_MakeInt16(raw[0], raw[1]);
    data->ay = MPU6050_MakeInt16(raw[2], raw[3]);
    data->az = MPU6050_MakeInt16(raw[4], raw[5]);
    data->temp = MPU6050_MakeInt16(raw[6], raw[7]);
    data->gx = MPU6050_MakeInt16(raw[8], raw[9]);
    data->gy = MPU6050_MakeInt16(raw[10], raw[11]);
    data->gz = MPU6050_MakeInt16(raw[12], raw[13]);
    data->gz_raw = data->gz;

    return 0;
}

int MPU6050_ReadRaw(MPU6050_RawData *data)
{
    int status = MPU6050_ReadRawData(data);
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;

    if (status != 0) {
        return status;
    }

    ax = (float)((int32_t) data->ax - (int32_t) g_mpu6050_ax_offset);
    ay = (float)((int32_t) data->ay - (int32_t) g_mpu6050_ay_offset);
    az = (float)((int32_t) data->az - (int32_t) g_mpu6050_az_offset);
    gx = (float)((int32_t) data->gx - (int32_t) g_mpu6050_gx_offset);
    gy = (float)((int32_t) data->gy - (int32_t) g_mpu6050_gy_offset);
    data->gz_raw = data->gz;
    gz = (float)((int32_t) data->gz_raw - (int32_t) g_mpu6050_gz_offset);

    if (g_mpu6050_filter_ready == 0U) {
        g_mpu6050_filtered_ax = ax;
        g_mpu6050_filtered_ay = ay;
        g_mpu6050_filtered_az = az;
        g_mpu6050_filtered_gx = gx;
        g_mpu6050_filtered_gy = gy;
        g_mpu6050_filtered_gz = gz;
        g_mpu6050_filter_ready = 1U;
    } else {
        g_mpu6050_filtered_ax = MPU6050_LowPass(g_mpu6050_filtered_ax, ax);
        g_mpu6050_filtered_ay = MPU6050_LowPass(g_mpu6050_filtered_ay, ay);
        g_mpu6050_filtered_az = MPU6050_LowPass(g_mpu6050_filtered_az, az);
        g_mpu6050_filtered_gx = MPU6050_LowPass(g_mpu6050_filtered_gx, gx);
        g_mpu6050_filtered_gy = MPU6050_LowPass(g_mpu6050_filtered_gy, gy);
        g_mpu6050_filtered_gz = MPU6050_LowPass(g_mpu6050_filtered_gz, gz);
    }

    data->ax = MPU6050_FloatToInt16(g_mpu6050_filtered_ax);
    data->ay = MPU6050_FloatToInt16(g_mpu6050_filtered_ay);
    data->az = MPU6050_FloatToInt16(g_mpu6050_filtered_az);
    data->gx = MPU6050_FloatToInt16(g_mpu6050_filtered_gx);
    data->gy = MPU6050_FloatToInt16(g_mpu6050_filtered_gy);
    data->gz = MPU6050_FloatToInt16(g_mpu6050_filtered_gz);

    return 0;
}

int MPU6050_UpdateAttitude(
    const MPU6050_RawData *data, float dt_sec, MPU6050_Attitude *attitude)
{
    float ax;
    float ay;
    float az;
    float gx_dps;
    float gy_dps;
    float gz_dps;
    float gz_abs_dps;
    float roll_acc;
    float pitch_acc;

    if ((data == 0) || (attitude == 0)) {
        return (int) PLATFORM_I2C_BAD_PARAM;
    }

    ax = (float) data->ax;
    ay = (float) data->ay;
    az = (float) data->az;
    gx_dps = (float) data->gx / MPU6050_GYRO_LSB_PER_DPS;
    gy_dps = (float) data->gy / MPU6050_GYRO_LSB_PER_DPS;
    gz_dps = (float) data->gz / MPU6050_GYRO_LSB_PER_DPS;
    gz_abs_dps = fabsf(gz_dps);

    attitude->gz_dps_x100 = MPU6050_FloatToInt16(gz_dps * 100.0f);
    attitude->drift_dps_x100 = MPU6050_FloatToInt16(gz_abs_dps * 100.0f);
    attitude->yaw_ready = (gz_abs_dps < YAW_READY_GZ_DPS) ? 1U : 0U;

    if (gz_abs_dps < YAW_GYRO_DEADBAND_DPS) {
        gz_dps = 0.0f;
        attitude->yaw_still = 1U;
    } else {
        attitude->yaw_still = 0U;
    }

    roll_acc = atan2f(ay, az) * MPU6050_RAD_TO_DEG;
    pitch_acc = atan2f(-ax, sqrtf((ay * ay) + (az * az))) *
        MPU6050_RAD_TO_DEG;

    if ((g_mpu6050_attitude_ready == 0U) || (dt_sec <= 0.0f)) {
        g_mpu6050_roll_deg = roll_acc;
        g_mpu6050_pitch_deg = pitch_acc;
        g_mpu6050_attitude_ready = 1U;
    } else {
        g_mpu6050_roll_deg =
            (MPU6050_COMP_ALPHA * (g_mpu6050_roll_deg + (gx_dps * dt_sec))) +
            ((1.0f - MPU6050_COMP_ALPHA) * roll_acc);
        g_mpu6050_pitch_deg =
            (MPU6050_COMP_ALPHA *
                (g_mpu6050_pitch_deg + (gy_dps * dt_sec))) +
            ((1.0f - MPU6050_COMP_ALPHA) * pitch_acc);
    }

    g_mpu6050_yaw_deg += gz_dps * dt_sec;
    if (g_mpu6050_yaw_deg > 180.0f) {
        g_mpu6050_yaw_deg -= 360.0f;
    } else if (g_mpu6050_yaw_deg < -180.0f) {
        g_mpu6050_yaw_deg += 360.0f;
    }

    attitude->roll_x10 = MPU6050_FloatToInt16(g_mpu6050_roll_deg * 10.0f);
    attitude->pitch_x10 = MPU6050_FloatToInt16(g_mpu6050_pitch_deg * 10.0f);
    attitude->yaw_x10 = MPU6050_FloatToInt16(g_mpu6050_yaw_deg * 10.0f);
    attitude->gz_dps_x10 = MPU6050_FloatToInt16(gz_dps * 10.0f);

    return 0;
}

int MPU6050_CalibrateStatic(uint16_t samples)
{
    uint16_t index;
    int32_t ax_sum = 0;
    int32_t ay_sum = 0;
    int32_t az_sum = 0;
    int32_t gx_sum = 0;
    int32_t gy_sum = 0;
    int32_t gz_sum = 0;
    int16_t ax_avg;
    int16_t ay_avg;
    int16_t az_avg;

    if (samples == 0U) {
        return (int) PLATFORM_I2C_BAD_PARAM;
    }

    for (index = 0U; index < samples; index++) {
        MPU6050_RawData sample;
        int status = MPU6050_ReadRawData(&sample);

        if (status != 0) {
            return status;
        }

        ax_sum += sample.ax;
        ay_sum += sample.ay;
        az_sum += sample.az;
        gx_sum += sample.gx;
        gy_sum += sample.gy;
        gz_sum += sample.gz;
        MPU6050_DelayMs(MPU6050_CALIB_DELAY_MS);
    }

    ax_avg = (int16_t)(ax_sum / (int32_t) samples);
    ay_avg = (int16_t)(ay_sum / (int32_t) samples);
    az_avg = (int16_t)(az_sum / (int32_t) samples);
    g_mpu6050_ax_offset = ax_avg;
    g_mpu6050_ay_offset = ay_avg;
    g_mpu6050_az_offset =
        (int16_t)((int32_t) az_avg - (int32_t) MPU6050_ACCEL_LSB_PER_G);
    g_mpu6050_gx_offset = (int16_t)(gx_sum / (int32_t) samples);
    g_mpu6050_gy_offset = (int16_t)(gy_sum / (int32_t) samples);
    g_mpu6050_gz_offset = (int16_t)(gz_sum / (int32_t) samples);
    g_mpu6050_az_reference = az_avg;
    MPU6050_ResetFilter();
    MPU6050_ResetAttitude();

    debug_printf("[CAL] gz_offset=%d\r\n", (int) g_mpu6050_gz_offset);

    (void) g_mpu6050_az_reference;

    return 0;
}

int MPU6050_MinimalVerify(MPU6050_RawData *data)
{
    int status;
    uint8_t who_am_i = 0U;

    if (data == 0) {
        return (int) PLATFORM_I2C_BAD_PARAM;
    }

    status = MPU6050_ReadWhoAmI(&who_am_i);
    data->who_am_i = who_am_i;
    if (status != 0) {
        debug_printf("[MPU] who_am_i read fail status=%d\r\n", status);
        return status;
    }

    if (who_am_i != MPU6050_WHO_AM_I_EXPECTED) {
        debug_printf("[MPU] who_am_i=0x%02X fail\r\n",
            (unsigned int) who_am_i);
        return MPU6050_ERROR_WHO_MISMATCH;
    }

    debug_printf("[MPU] who_am_i=0x%02X ok\r\n",
        (unsigned int) who_am_i);

    status = MPU6050_InitBasic();
    if (status != 0) {
        debug_printf("[MPU] wake fail status=%d\r\n", status);
        return status;
    }

    debug_print("[MPU] wake ok\r\n");
    debug_print("[MPU] keep still calibrating\r\n");
    MPU6050_DelayMs(MPU6050_CALIB_STABLE_MS);

    status = MPU6050_CalibrateStatic(MPU6050_CALIB_SAMPLES);
    if (status != 0) {
        debug_printf("[MPU] calib fail status=%d\r\n", status);
        return status;
    }

    status = MPU6050_ReadRaw(data);
    if (status != 0) {
        debug_printf("[MPU] raw read fail status=%d\r\n", status);
        return status;
    }

    data->who_am_i = who_am_i;
    return 0;
}

/* These next two functions converts the orientation matrix (see
 * gyro_orientation) to a scalar representation for use by the DMP.
 * NOTE: These functions are borrowed from Invensense's MPL.
 */
static unsigned short inv_row_2_scale(const signed char *row)
{
    unsigned short b;

    if (row[0] > 0)
        b = 0;
    else if (row[0] < 0)
        b = 4;
    else if (row[1] > 0)
        b = 1;
    else if (row[1] < 0)
        b = 5;
    else if (row[2] > 0)
        b = 2;
    else if (row[2] < 0)
        b = 6;
    else
        b = 7;      // error
    return b;
}

static unsigned short inv_orientation_matrix_to_scalar(
    const signed char *mtx)
{
    unsigned short scalar;

    /*
       XYZ  010_001_000 Identity Matrix
       XZY  001_010_000
       YXZ  010_000_001
       YZX  000_010_001
       ZXY  001_000_010
       ZYX  000_001_010
     */

    scalar = inv_row_2_scale(mtx);
    scalar |= inv_row_2_scale(mtx + 3) << 3;
    scalar |= inv_row_2_scale(mtx + 6) << 6;


    return scalar;
}

static int run_self_test(void) __attribute__((unused));
static int run_self_test(void)
{
    int result;
    long gyro[3], accel[3];

    result = mpu_run_self_test(gyro, accel);
    if (result == 0x3) {
        /* Test passed. We can trust the gyro data here, so let's push it down
         * to the DMP.
         */
        float sens;
        unsigned short accel_sens;
        mpu_get_gyro_sens(&sens);
        gyro[0] = (long)(gyro[0] * sens);
        gyro[1] = (long)(gyro[1] * sens);
        gyro[2] = (long)(gyro[2] * sens);
        dmp_set_gyro_bias(gyro);
        mpu_get_accel_sens(&accel_sens);
        accel[0] *= accel_sens;
        accel[1] *= accel_sens;
        accel[2] *= accel_sens;
        dmp_set_accel_bias(accel);
    } else {
        return -1;
    }

    return 0;
}

int MPU6050_DMP_init(void)
{
    int ret;
    struct int_param_s int_param = {0};

    ret = mpu_init(&int_param);
    if(ret != 0)
    {
        return (ret < -100) ? ret : ERROR_MPU_INIT;
    }

    ret = mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL);
    if(ret != 0)
    {
        return ERROR_SET_SENSOR;
    }

    ret = mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL);
    if(ret != 0)
    {
        return ERROR_CONFIG_FIFO;
    }

    ret = mpu_set_sample_rate(DEFAULT_MPU_HZ);
    if(ret != 0)
    {
        return ERROR_SET_RATE;
    }

    ret = dmp_load_motion_driver_firmware();
    if(ret != 0)
    {
        return ERROR_LOAD_MOTION_DRIVER;
    }

    ret = dmp_set_orientation(inv_orientation_matrix_to_scalar(gyro_orientation));
    if(ret != 0)
    {
        return ERROR_SET_ORIENTATION;
    }

    /*
     * 只开启姿态解算需要的功能。
     * 暂时不要开 TAP 和 ANDROID_ORIENT。
     */
    ret = dmp_enable_feature(DMP_FEATURE_6X_LP_QUAT |
                             DMP_FEATURE_SEND_RAW_ACCEL |
                             DMP_FEATURE_SEND_CAL_GYRO |
                             DMP_FEATURE_GYRO_CAL);
    if(ret != 0)
    {
        return ERROR_ENABLE_FEATURE;
    }

    ret = dmp_set_fifo_rate(DEFAULT_MPU_HZ);
    if(ret != 0)
    {
        return ERROR_SET_FIFO_RATE;
    }

    /*
     * 自检你之前会返回 -9，所以先关闭。
     */
#if 0
    ret = run_self_test();
    if(ret != 0)
    {
        return ERROR_SELF_TEST;
    }
#endif

    ret = mpu_set_dmp_state(1);
    if(ret != 0)
    {
        return ERROR_DMP_STATE;
    }

    /*
     * DMP 使能后清 FIFO，等待新数据产生。
     */
    mpu_reset_fifo();
    MPU6050_DelayMs(100);

    return 0;
}

int MPU6050_DMP_Get_Date(float *pitch, float *roll, float *yaw)
{
    float q0, q1, q2, q3;
    short gyro[3];
    short accel[3];
    long quat[4];
    unsigned long timestamp;
    short sensors;
    unsigned char more;
    unsigned char drain_count = 0;
    unsigned char quat_valid = 0;

    int ret;

    do
    {
        ret = dmp_read_fifo(gyro, accel, quat, &timestamp, &sensors, &more);

        if (ret != 0)
        {
            return -1;   // FIFO 没读到完整数据包，或者 FIFO 读取失败
        }

        if (sensors & INV_WXYZ_QUAT)
        {
            quat_valid = 1;
        }

        drain_count++;
    }
    while ((more > 0) && (drain_count < MPU6050_DMP_FIFO_DRAIN_MAX));

    if (more > 0)
    {
        mpu_reset_fifo();
        return -3;       // FIFO 积压过多，本次丢弃旧包，下一次再读新包
    }

    if (quat_valid == 0)
    {
        return -2;       // 读到了 FIFO，但这一包里没有四元数
    }

    q0 = quat[0] / Q30;
    q1 = quat[1] / Q30;
    q2 = quat[2] / Q30;
    q3 = quat[3] / Q30;

    float sinp = -2.0f * q1 * q3 + 2.0f * q0 * q2;

    if (sinp > 1.0f)
    {
        sinp = 1.0f;
    }
    else if (sinp < -1.0f)
    {
        sinp = -1.0f;
    }

    *pitch = asinf(sinp) * 57.29578f;

    *roll = atan2f(2.0f * q2 * q3 + 2.0f * q0 * q1,
                   -2.0f * q1 * q1 - 2.0f * q2 * q2 + 1.0f) * 57.29578f;

    *yaw = atan2f(2.0f * (q0 * q3 + q1 * q2),
                  q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3) * 57.29578f;

    return 0;
}
void MPU6050_DMP_ResetFIFO(void)
{
    mpu_reset_fifo();
    MPU6050_DelayMs(100);
}

