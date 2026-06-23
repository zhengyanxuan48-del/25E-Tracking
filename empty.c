/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and this disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and this disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MOTOR_OPEN_LOOP_TEST
#define MOTOR_OPEN_LOOP_TEST (0U)
#endif

#ifndef APP_MODE_TRACK_UART_TEST
#define APP_MODE_TRACK_UART_TEST (0U)
#endif

#ifndef APP_MODE_PAN_TILT_DEBUG
#define APP_MODE_PAN_TILT_DEBUG (0U)
#endif

#ifndef APP_MODE_TASK1
#define APP_MODE_TASK1 ((APP_MODE_PAN_TILT_DEBUG != 0U) ? 0U : 1U)
#endif

#ifndef APP_MODE_MOTOR_TEST
#define APP_MODE_MOTOR_TEST (0U)
#endif

#if (((APP_MODE_TRACK_UART_TEST != 0U) ? 1 : 0) + \
     ((APP_MODE_PAN_TILT_DEBUG != 0U) ? 1 : 0) + \
     ((APP_MODE_TASK1 != 0U) ? 1 : 0) + \
     ((APP_MODE_MOTOR_TEST != 0U) ? 1 : 0)) > 1
#error "Only one app mode can be enabled."
#endif

#include "ti_msp_dl_config.h"
#include "debug_uart.h"
#include "encoder.h"
#include "key.h"
#include "motor_open_loop_test.h"
#include "motor_test.h"
#include "MPU6050.h"
#include "OLED.h"
#include "pan_tilt_debug.h"
#include "platform_i2c.h"
#include "platform_time.h"
#include "task1.h"
#include "track.h"

#include <stdio.h>

#define I2C_SPEED_HZ    (I2C_0_BUS_SPEED_HZ)
#if MOTOR_OPEN_LOOP_TEST
#define APP_FW_TAG      "motor-open-loop-test"
#elif APP_MODE_TRACK_UART_TEST
#define APP_FW_TAG      "track-uart8-test"
#elif APP_MODE_PAN_TILT_DEBUG
#define APP_FW_TAG      "pan-tilt-debug"
#elif APP_MODE_TASK1
#define APP_FW_TAG      "task1-only"
#elif APP_MODE_MOTOR_TEST
#define APP_FW_TAG      "motor-test"
#else
#define APP_FW_TAG      "idle"
#endif
#if APP_MODE_TASK1
#define MPU_READ_MS     (10U) // 调试参数：Task1 角度环 MPU 读取周期，缩短后提高 90 度转向反馈速度
#else
#define MPU_READ_MS     (20U) // 调试参数：非 Task1 模式 MPU 读取周期，保持原有 20ms 节奏
#endif
#define TRACK_READ_MS   (5U)
#define TRACK_UART_MS   (200U)
#define TRACK_TEST_OLED_MS (100U)
#define KEY_SCAN_MS     (5U)
#define ENCODER_UPDATE_MS (20U)

#if (MOTOR_OPEN_LOOP_TEST == 0U) && (APP_MODE_TRACK_UART_TEST == 0U) && \
    (APP_MODE_PAN_TILT_DEBUG == 0U)
static MPU6050_Attitude g_mpu_attitude;
static int g_mpu_status = -1;
static uint32_t g_next_mpu_ms;
static uint32_t g_last_mpu_ms;
#endif

#if (MOTOR_OPEN_LOOP_TEST == 0U) && (APP_MODE_PAN_TILT_DEBUG == 0U) && \
    (APP_MODE_TASK1 == 0U)
static uint32_t g_next_track_ms;
static uint32_t g_next_track_uart_ms;
static uint32_t g_next_track_oled_ms;
static uint8_t g_track_bits;
#endif

#if (MOTOR_OPEN_LOOP_TEST == 0U) && (APP_MODE_PAN_TILT_DEBUG == 0U) && \
    (APP_MODE_TASK1 == 0U)
static void binary8_string(uint8_t value, char out[9])
{
    uint8_t bit;

    for (bit = 0U; bit < 8U; bit++) {
        out[bit] = ((value & (uint8_t)(1U << (7U - bit))) != 0U) ? '1' : '0';
    }
    out[8] = '\0';
}
#endif

static int oled_boot_test(void)
{
    platform_i2c_status_t status;
    uint8_t attempt;

    status = platform_i2c_probe(PLATFORM_I2C_ADDR_OLED);
    debug_printf("[OLED] probe 0x3C %s\r\n",
        (status == PLATFORM_I2C_OK) ? "ok" : "fail");
    if (status != PLATFORM_I2C_OK) {
        return (int) status;
    }

    for (attempt = 0U; attempt < 2U; attempt++) {
        status = OLED_InitStatus();
        if (status == PLATFORM_I2C_OK) {
            break;
        }

        debug_printf("[OLED] init fail status=%d attempt=%u\r\n",
            (int) status, (unsigned int) (attempt + 1U));
        platform_i2c_recover_bus();
        platform_delay_ms(10U);
    }
    if (status != PLATFORM_I2C_OK) {
        debug_printf("[OLED] init final fail status=%d\r\n", (int) status);
        return (int) status;
    }

    (void) OLED_Clear();
    (void) OLED_UpdateStatus();
#if APP_MODE_TRACK_UART_TEST
    OLED_ShowString(0, 0, "TRK UART8", OLED_8X16);
#elif APP_MODE_PAN_TILT_DEBUG
    OLED_ShowString(0, 0, "PAN DEBUG", OLED_8X16);
#elif APP_MODE_TASK1
    OLED_ShowString(0, 0, "TASK1", OLED_8X16);
#elif APP_MODE_MOTOR_TEST
    OLED_ShowString(0, 0, "MOTOR TEST", OLED_8X16);
#else
    OLED_ShowString(0, 0, "IDLE", OLED_8X16);
#endif
    (void) OLED_UpdateAreaStatus(0, 0, 128U, 16U);

    return 0;
}

#if (MOTOR_OPEN_LOOP_TEST == 0U) && (APP_MODE_TRACK_UART_TEST == 0U) && \
    (APP_MODE_PAN_TILT_DEBUG == 0U)
static int mpu_boot_init(uint8_t oled_enabled)
{
    MPU6050_RawData raw;
    int status;

    if (oled_enabled != 0U) {
        OLED_ClearArea(0, 16, 128U, 16U);
        OLED_ShowString(0, 16, "MPU CAL...", OLED_8X16);
        (void) OLED_UpdateAreaStatus(0, 16, 128U, 16U);
    }

    debug_print("[MPU] init start\r\n");
    status = MPU6050_MinimalVerify(&raw);
    if (status != 0) {
        debug_printf("[MPU] init fail status=%d\r\n", status);
        return status;
    }

    (void) MPU6050_UpdateAttitude(&raw, 0.0f, &g_mpu_attitude);
    debug_print("[MPU] init ok\r\n");

    return 0;
}

static void mpu_update_task(uint32_t now_ms)
{
    if ((int32_t)(now_ms - g_next_mpu_ms) < 0) {
        return;
    }

    if (g_mpu_status == 0) {
        MPU6050_RawData raw;
        uint32_t dt_ms = now_ms - g_last_mpu_ms;
        float dt_sec;
        int status;

        if (dt_ms == 0U) {
            dt_ms = MPU_READ_MS;
        }
        dt_sec = (float) dt_ms / 1000.0f;

        status = MPU6050_ReadRaw(&raw);
        if (status == 0) {
            status = MPU6050_UpdateAttitude(&raw, dt_sec, &g_mpu_attitude);
        }
        g_mpu_status = status;
        g_last_mpu_ms = now_ms;
    }

    g_next_mpu_ms = now_ms + MPU_READ_MS;
#if APP_MODE_MOTOR_TEST
    MotorTest_SetTelemetry(&g_mpu_attitude, g_mpu_status);
#endif
}
#endif

#if (MOTOR_OPEN_LOOP_TEST == 0U) && (APP_MODE_PAN_TILT_DEBUG == 0U) && \
    (APP_MODE_TASK1 == 0U)
static void track_update_task(uint32_t now_ms)
{
    if ((int32_t)(now_ms - g_next_track_ms) < 0) {
        return;
    }

    g_track_bits = Track_ReadBits();
    Track_DiagnosticUpdate(now_ms);
    g_next_track_ms = now_ms + TRACK_READ_MS;
#if APP_MODE_MOTOR_TEST
    MotorTest_SetTrackBits(g_track_bits, 0);
#endif

    if (APP_MODE_MOTOR_TEST != 0U) {
        if ((int32_t)(now_ms - g_next_track_uart_ms) >= 0) {
            char raw[9];
            char black[9];

            binary8_string(Track_GetRawBits(), raw);
            binary8_string(Track_GetBlackBits(), black);
            debug_printf("[TRACK] raw=0b%s black=0b%s err=%d lost=%u\r\n",
                raw,
                black,
                (int) Track_GetLineError(),
                (unsigned int) Track_IsLineLost());
            g_next_track_uart_ms = now_ms + TRACK_UART_MS;
        }
    }

    if (APP_MODE_TRACK_UART_TEST != 0U) {
        if ((int32_t)(now_ms - g_next_track_uart_ms) >= 0) {
            char raw[9];
            char black[9];

            binary8_string(Track_GetRawBits(), raw);
            binary8_string(Track_GetBlackBits(), black);
            debug_printf("[TRACK-UART] baud=%lu lock=%u rx=%lu frame=%lu byte=0x%02X raw=0b%s black=0b%s err=%d lost=%u\r\n",
                (unsigned long) Track_GetCurrentBaud(),
                (unsigned int) Track_IsBaudScanLocked(),
                (unsigned long) Track_GetRxCount(),
                (unsigned long) Track_GetFrameCount(),
                (unsigned int) Track_GetLastRxByte(),
                raw,
                black,
                (int) Track_GetLineError(),
                (unsigned int) Track_IsLineLost());
            g_next_track_uart_ms = now_ms + TRACK_UART_MS;
        }
    }
}
#endif

#if APP_MODE_TRACK_UART_TEST == 0U
static void key_update_task(uint32_t now_ms)
{
    static uint32_t next_ms = 0U;

    if ((int32_t) (now_ms - next_ms) < 0) {
        return;
    }

    Key_Update(now_ms);
    next_ms = now_ms + KEY_SCAN_MS;
}
#endif

#if APP_MODE_TRACK_UART_TEST
static void oled_track_uart_test_task(uint32_t now_ms, uint8_t oled_enabled)
{
    char line[17];

    if (oled_enabled == 0U) {
        return;
    }
    if ((int32_t)(now_ms - g_next_track_oled_ms) < 0) {
        return;
    }

    OLED_ClearArea(0, 0, 128U, 64U);
    OLED_ShowString(0, 0, "TRACK UART", OLED_8X16);

    (void) snprintf(line, sizeof(line), "RX:%04lu %luk",
        (unsigned long)(Track_GetRxCount() % 10000UL),
        (unsigned long)(Track_GetCurrentBaud() / 1000UL));
    OLED_ShowString(0, 16, line, OLED_8X16);

    OLED_ShowString(0, 32, "B:", OLED_8X16);
    OLED_ShowBinNum(16, 32, Track_GetBlackBits(), 8, OLED_8X16);

    (void) snprintf(line, sizeof(line), "ERR:%+03d L:%u",
        (int) Track_GetLineError(),
        (unsigned int) Track_IsLineLost());
    OLED_ShowString(0, 48, line, OLED_8X16);
    (void) OLED_UpdateAreaStatus(0, 0, 128U, 64U);

    g_next_track_oled_ms = now_ms + TRACK_TEST_OLED_MS;
}
#endif

#if (MOTOR_OPEN_LOOP_TEST != 0U) || \
    ((APP_MODE_TRACK_UART_TEST == 0U) && (APP_MODE_PAN_TILT_DEBUG == 0U))
static void encoder_update_task(uint32_t now_ms, uint32_t *prev_ms)
{
    uint32_t dt_ms = now_ms - *prev_ms;
    if (dt_ms == 0U) {
        dt_ms = ENCODER_UPDATE_MS;
    }

    Encoder_Update(dt_ms);
    *prev_ms = now_ms;
}
#endif

int main(void)
{
    int oled_status;
    uint8_t oled_enabled;
    uint32_t now_ms;
#if (MOTOR_OPEN_LOOP_TEST != 0U) || \
    ((APP_MODE_TRACK_UART_TEST == 0U) && (APP_MODE_PAN_TILT_DEBUG == 0U))
    uint32_t last_encoder_ms;
#endif

    SYSCFG_DL_init();
    platform_time_init();
    platform_i2c_init();

    debug_print("\r\n[BOOT] motor yaw task1\r\n");
    debug_printf("[FW] %s\r\n", APP_FW_TAG);
    debug_printf("[RATE] encoder=%ums track=%ums mpu=%ums i2c=%luk\r\n",
        (unsigned int) ENCODER_UPDATE_MS,
        (unsigned int) TRACK_READ_MS,
        (unsigned int) MPU_READ_MS,
        (unsigned long)(I2C_SPEED_HZ / 1000U));

    oled_status = oled_boot_test();
    oled_enabled = (oled_status == 0) ? 1U : 0U;

    now_ms = platform_time_ms();
#if (MOTOR_OPEN_LOOP_TEST == 0U) && (APP_MODE_TRACK_UART_TEST == 0U) && \
    (APP_MODE_PAN_TILT_DEBUG == 0U)
    g_last_mpu_ms = now_ms;
    g_next_mpu_ms = now_ms + MPU_READ_MS;
#endif
#if (MOTOR_OPEN_LOOP_TEST == 0U) && (APP_MODE_PAN_TILT_DEBUG == 0U) && \
    (APP_MODE_TASK1 == 0U)
    g_next_track_ms = now_ms + TRACK_READ_MS;
    g_next_track_uart_ms = now_ms + TRACK_UART_MS;
    g_next_track_oled_ms = now_ms + TRACK_TEST_OLED_MS;
#endif
#if (MOTOR_OPEN_LOOP_TEST != 0U) || \
    ((APP_MODE_TRACK_UART_TEST == 0U) && (APP_MODE_PAN_TILT_DEBUG == 0U))
    last_encoder_ms = now_ms;
#endif

#if MOTOR_OPEN_LOOP_TEST
    MotorOpenLoopTest_Init(oled_enabled);
#else
#if APP_MODE_TRACK_UART_TEST
    Track_Init();
    debug_print("[APP] track uart8 independent test\r\n");
#elif APP_MODE_PAN_TILT_DEBUG
    PanTiltDebug_Init(oled_enabled);
    debug_print("[APP] pan tilt key debug\r\n");
#else
    g_mpu_status = mpu_boot_init(oled_enabled);
    Track_Init();

#if APP_MODE_TASK1
    Task1_Init();
#elif APP_MODE_MOTOR_TEST
    MotorTest_Init();
    MotorTest_SetOledEnabled(oled_enabled);
#else
    debug_print("[APP] no active app mode\r\n");
#endif
#endif
#endif

    while (1) {
        now_ms = platform_time_ms();
#if MOTOR_OPEN_LOOP_TEST
        encoder_update_task(now_ms, &last_encoder_ms);
        MotorOpenLoopTest_Update(now_ms);
#else
#if APP_MODE_TRACK_UART_TEST
        track_update_task(now_ms);
        oled_track_uart_test_task(now_ms, oled_enabled);
#elif APP_MODE_PAN_TILT_DEBUG
        key_update_task(now_ms);
        PanTiltDebug_Update(now_ms);
#else
        mpu_update_task(now_ms);
#if APP_MODE_TASK1
        Track_DiagnosticUpdate(now_ms);
#else
        track_update_task(now_ms);
#endif
        key_update_task(now_ms);
        encoder_update_task(now_ms, &last_encoder_ms);

#if APP_MODE_TASK1
        Task1_Update(now_ms);
#elif APP_MODE_MOTOR_TEST
        MotorTest_Update(now_ms);
#else
        (void) now_ms;
#endif
#endif
#endif
    }
}
