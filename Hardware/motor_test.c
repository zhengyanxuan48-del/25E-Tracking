#include "motor_test.h"

#include "OLED.h"
#include "debug_uart.h"
#include "encoder.h"
#include "key.h"
#include "motor.h"
#include "platform_time.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define MOTOR_TEST_KEY_SCAN_MS (5U)
#define MOTOR_TEST_ENCODER_MS  (20U)
#define MOTOR_TEST_OLED_MS     (50U)
#define MOTOR_TEST_UART_MS     (200U)
#define MOTOR_TEST_OLED_WIDTH  (128U)
#define MOTOR_TEST_OLED_HEIGHT (16U)

#define LOW_PWM  (200)
#define MID_PWM  (400)
#define HIGH_PWM (650)

typedef enum {
    MOTOR_RUN_LOW = 0,
    MOTOR_RUN_MID,
    MOTOR_RUN_HIGH,
    MOTOR_RUN_STOP
} MotorRunState;

typedef struct {
    uint8_t initialized;
    uint8_t oled_enabled;
    uint8_t log_valid;
    uint8_t telemetry_valid;
    uint8_t track_valid;
    uint8_t track_bits;
    MotorRunState run_state;
    MotorRunState logged_state;
    MPU6050_Attitude attitude;
    int telemetry_status;
    int track_status;
    uint32_t next_key_ms;
    uint32_t next_encoder_ms;
    uint32_t next_oled_ms;
    uint32_t next_uart_ms;
    uint32_t last_encoder_ms;
    char oled_line[4][18];
} MotorTestState;

static MotorTestState g_motor_test;

static const char *MotorTest_StateName(MotorRunState state)
{
    switch (state) {
        case MOTOR_RUN_LOW:
            return "LOW";
        case MOTOR_RUN_MID:
            return "MID";
        case MOTOR_RUN_HIGH:
            return "HIGH";
        case MOTOR_RUN_STOP:
            return "STOP";
        default:
            return "UNK";
    }
}

static int16_t MotorTest_StatePwm(MotorRunState state)
{
    switch (state) {
        case MOTOR_RUN_LOW:
            return LOW_PWM;
        case MOTOR_RUN_MID:
            return MID_PWM;
        case MOTOR_RUN_HIGH:
            return HIGH_PWM;
        case MOTOR_RUN_STOP:
        default:
            return 0;
    }
}

static void MotorTest_LogStateIfChanged(void)
{
    if ((g_motor_test.log_valid != 0U) &&
        (g_motor_test.logged_state == g_motor_test.run_state)) {
        return;
    }

    debug_printf("[MOTOR] state=%s pwm=%d\r\n",
        MotorTest_StateName(g_motor_test.run_state),
        (int) MotorTest_StatePwm(g_motor_test.run_state));

    g_motor_test.logged_state = g_motor_test.run_state;
    g_motor_test.log_valid = 1U;
}

static void MotorTest_ApplyState(MotorRunState state, uint8_t force)
{
    int16_t pwm;

    if ((force == 0U) && (g_motor_test.run_state == state)) {
        return;
    }

    g_motor_test.run_state = state;
    pwm = MotorTest_StatePwm(state);

    if (state == MOTOR_RUN_STOP) {
        Motor_StopAll();
    } else {
        Motor_SetBoth(pwm, pwm);
    }

    MotorTest_LogStateIfChanged();
    g_motor_test.next_uart_ms = platform_time_ms() + MOTOR_TEST_UART_MS;
}

static int32_t MotorTest_ClampDisplayValue(int32_t value, int32_t limit)
{
    if (value > limit) {
        return limit;
    }
    if (value < -limit) {
        return -limit;
    }
    return value;
}

static void MotorTest_FormatRpm(
    char *buffer, size_t buffer_size, int32_t rpm)
{
    char sign = '+';

    if (buffer == NULL) {
        return;
    }

    rpm = MotorTest_ClampDisplayValue(rpm, 9999);
    if (rpm < 0) {
        sign = '-';
        rpm = -rpm;
    }

    (void) snprintf(buffer, buffer_size, "%c%04ld", sign, (long) rpm);
}

static void MotorTest_FormatX10(
    char *buffer, size_t buffer_size, int16_t value_x10)
{
    char sign = '+';
    int32_t value = value_x10;
    int32_t integer;
    int32_t fraction;

    if (buffer == NULL) {
        return;
    }

    value = MotorTest_ClampDisplayValue(value, 1800);
    if (value < 0) {
        sign = '-';
        value = -value;
    }

    integer = value / 10;
    fraction = value % 10;

    (void) snprintf(buffer, buffer_size, "%c%03ld.%ld",
        sign, (long) integer, (long) fraction);
}

static void MotorTest_FormatBinary5(uint8_t value, char out[6])
{
    uint8_t bit;

    if (out == NULL) {
        return;
    }

    for (bit = 0U; bit < 5U; bit++) {
        out[bit] =
            ((value & (uint8_t)(1U << (4U - bit))) != 0U) ? '1' : '0';
    }
    out[5] = '\0';
}

static int MotorTest_UpdateOledLine(uint8_t line_index, const char *text)
{
    uint8_t y;

    if ((text == NULL) || (line_index >= 4U)) {
        return -1;
    }

    if (strncmp(g_motor_test.oled_line[line_index], text,
            sizeof(g_motor_test.oled_line[line_index])) == 0) {
        return 0;
    }

    (void) snprintf(g_motor_test.oled_line[line_index],
        sizeof(g_motor_test.oled_line[line_index]), "%s", text);

    y = (uint8_t)(line_index * MOTOR_TEST_OLED_HEIGHT);
    OLED_ClearArea(0, y, MOTOR_TEST_OLED_WIDTH, MOTOR_TEST_OLED_HEIGHT);
    OLED_ShowString(0, y, g_motor_test.oled_line[line_index], OLED_8X16);

    return (int) OLED_UpdateAreaStatus(
        0, y, MOTOR_TEST_OLED_WIDTH, MOTOR_TEST_OLED_HEIGHT);
}

static void MotorTest_UpdateOled(void)
{
    char left_rpm[8];
    char right_rpm[8];
    char yaw[9];
    char track_bits[6];
    char line[4][18];
    int update_status;

    if (g_motor_test.oled_enabled == 0U) {
        return;
    }

    MotorTest_FormatRpm(
        left_rpm, sizeof(left_rpm), Encoder_GetRPM(ENCODER_LEFT));
    MotorTest_FormatRpm(
        right_rpm, sizeof(right_rpm), Encoder_GetRPM(ENCODER_RIGHT));
    if ((g_motor_test.telemetry_valid != 0U) &&
        (g_motor_test.telemetry_status == 0)) {
        MotorTest_FormatX10(
            yaw, sizeof(yaw), g_motor_test.attitude.yaw_x10);
    } else {
        (void) snprintf(yaw, sizeof(yaw), "---.-");
    }
    if ((g_motor_test.track_valid != 0U) &&
        (g_motor_test.track_status == 0)) {
        MotorTest_FormatBinary5(g_motor_test.track_bits, track_bits);
    } else {
        (void) snprintf(track_bits, sizeof(track_bits), "-----");
    }

    (void) snprintf(line[0], sizeof(line[0]), "L:%srpm", left_rpm);
    (void) snprintf(line[1], sizeof(line[1]), "R:%srpm", right_rpm);
    (void) snprintf(line[2], sizeof(line[2]), "Y:%s M:%s", yaw,
        MotorTest_StateName(g_motor_test.run_state));
    (void) snprintf(line[3], sizeof(line[3]), "TRK:%s", track_bits);

    update_status = MotorTest_UpdateOledLine(0U, line[0]);
    if (update_status != 0) {
        debug_printf("[MOTOR] oled line0 fail status=%d\r\n", update_status);
        return;
    }

    update_status = MotorTest_UpdateOledLine(1U, line[1]);
    if (update_status != 0) {
        debug_printf("[MOTOR] oled line1 fail status=%d\r\n", update_status);
        return;
    }

    update_status = MotorTest_UpdateOledLine(2U, line[2]);
    if (update_status != 0) {
        debug_printf("[MOTOR] oled line2 fail status=%d\r\n", update_status);
        return;
    }

    update_status = MotorTest_UpdateOledLine(3U, line[3]);
    if (update_status != 0) {
        debug_printf("[MOTOR] oled line3 fail status=%d\r\n", update_status);
    }
}

static void MotorTest_HandleKeyEvents(void)
{
    uint8_t key2_pressed = Key_WasPressed(KEY_ID_2);
    uint8_t key1_pressed = Key_WasPressed(KEY_ID_1);

    if (key2_pressed != 0U) {
        MotorTest_ApplyState(MOTOR_RUN_STOP, 0U);
        return;
    }

    if (key1_pressed == 0U) {
        return;
    }

    if (g_motor_test.run_state == MOTOR_RUN_LOW) {
        MotorTest_ApplyState(MOTOR_RUN_MID, 0U);
    } else if (g_motor_test.run_state == MOTOR_RUN_MID) {
        MotorTest_ApplyState(MOTOR_RUN_HIGH, 0U);
    }
}

void MotorTest_Init(void)
{
    uint32_t now_ms = platform_time_ms();

    memset(&g_motor_test, 0, sizeof(g_motor_test));

    Motor_Init();
    Encoder_Init();
    Key_Init();
    Motor_StopAll();

    g_motor_test.initialized = 1U;
    g_motor_test.next_key_ms = now_ms + MOTOR_TEST_KEY_SCAN_MS;
    g_motor_test.next_encoder_ms = now_ms + MOTOR_TEST_ENCODER_MS;
    g_motor_test.next_oled_ms = now_ms + MOTOR_TEST_OLED_MS;
    g_motor_test.next_uart_ms = now_ms + MOTOR_TEST_UART_MS;
    g_motor_test.last_encoder_ms = now_ms;

    MotorTest_ApplyState(MOTOR_RUN_LOW, 1U);
}

void MotorTest_SetOledEnabled(uint8_t enabled)
{
    g_motor_test.oled_enabled = (enabled != 0U) ? 1U : 0U;
}

void MotorTest_SetTelemetry(const MPU6050_Attitude *attitude, int status)
{
    g_motor_test.telemetry_status = status;
    if (attitude != NULL) {
        g_motor_test.attitude = *attitude;
        g_motor_test.telemetry_valid = 1U;
    }
}

void MotorTest_SetTrackBits(uint8_t bits, int status)
{
    g_motor_test.track_status = status;
    if (status == 0) {
        g_motor_test.track_bits = bits;
        g_motor_test.track_valid = 1U;
    }
}

void MotorTest_Update(uint32_t now_ms)
{
    if (g_motor_test.initialized == 0U) {
        return;
    }

    if ((int32_t)(now_ms - g_motor_test.next_key_ms) >= 0) {
        Key_Update(now_ms);
        MotorTest_HandleKeyEvents();
        g_motor_test.next_key_ms = now_ms + MOTOR_TEST_KEY_SCAN_MS;
    }

    if ((int32_t)(now_ms - g_motor_test.next_encoder_ms) >= 0) {
        uint32_t dt_ms = now_ms - g_motor_test.last_encoder_ms;

        if (dt_ms == 0U) {
            dt_ms = MOTOR_TEST_ENCODER_MS;
        }

        Encoder_Update(dt_ms);
        g_motor_test.last_encoder_ms = now_ms;
        g_motor_test.next_encoder_ms = now_ms + MOTOR_TEST_ENCODER_MS;
    }

    if ((int32_t)(now_ms - g_motor_test.next_oled_ms) >= 0) {
        MotorTest_UpdateOled();
        g_motor_test.next_oled_ms = now_ms + MOTOR_TEST_OLED_MS;
    }

    if ((int32_t)(now_ms - g_motor_test.next_uart_ms) >= 0) {
        MotorTest_LogStateIfChanged();
        g_motor_test.next_uart_ms = now_ms + MOTOR_TEST_UART_MS;
    }
}
