#include "pan_tilt_debug.h"

#include "OLED.h"
#include "debug_uart.h"
#include "key.h"
#include "task1.h"
#include "ti_msp_dl_config.h"

#include <stdio.h>
#include <string.h>

// ===== Pan tilt debug tunable parameters =====

#ifndef PAN_TILT_SERVO_PORT
#define PAN_TILT_SERVO_PORT (TRACK_OUT5_PORT) // Default signal pin: PB18, old 5-line sensor input.
#endif

#ifndef PAN_TILT_SERVO_PIN
#define PAN_TILT_SERVO_PIN (TRACK_OUT5_PIN_TRACK_OUT5_PIN) // Default signal pin mask: PB18.
#endif

#ifndef PAN_TILT_SERVO_IOMUX
#define PAN_TILT_SERVO_IOMUX (TRACK_OUT5_PIN_TRACK_OUT5_IOMUX) // Default signal pin IOMUX: PB18.
#endif

#ifndef PAN_TILT_CENTER_DEG
#define PAN_TILT_CENTER_DEG (90) // Servo center angle after boot.
#endif

#ifndef PAN_TILT_STEP_DEG
#define PAN_TILT_STEP_DEG (45) // KEY1 adds this angle, KEY2 subtracts this angle.
#endif

#ifndef PAN_TILT_MIN_DEG
#define PAN_TILT_MIN_DEG (0) // Minimum allowed servo angle.
#endif

#ifndef PAN_TILT_MAX_DEG
#define PAN_TILT_MAX_DEG (180) // Maximum allowed servo angle.
#endif

#ifndef PAN_TILT_SERVO_MIN_US
#define PAN_TILT_SERVO_MIN_US (500U) // Pulse width for minimum angle.
#endif

#ifndef PAN_TILT_SERVO_MAX_US
#define PAN_TILT_SERVO_MAX_US (2500U) // Pulse width for maximum angle.
#endif

#ifndef PAN_TILT_SERVO_PERIOD_MS
#define PAN_TILT_SERVO_PERIOD_MS (20U) // Standard analog servo frame period.
#endif

#ifndef PAN_TILT_OLED_PERIOD_MS
#define PAN_TILT_OLED_PERIOD_MS (100U) // OLED refresh period in debug mode.
#endif

typedef struct {
    uint8_t initialized;
    uint8_t oled_enabled;
    int16_t angle_deg;
    uint16_t pulse_us;
    uint32_t next_pulse_ms;
    uint32_t next_oled_ms;
    char oled_line[4][18];
} PanTiltDebugState;

static PanTiltDebugState g_pan_tilt;

static int16_t PanTilt_ClampI16(int32_t value, int32_t min_value,
    int32_t max_value)
{
    if (value > max_value) {
        value = max_value;
    }
    if (value < min_value) {
        value = min_value;
    }
    return (int16_t) value;
}

static uint16_t PanTilt_AngleToPulseUs(int16_t angle_deg)
{
    int32_t angle_span = (int32_t) PAN_TILT_MAX_DEG -
        (int32_t) PAN_TILT_MIN_DEG;
    int32_t pulse_span = (int32_t) PAN_TILT_SERVO_MAX_US -
        (int32_t) PAN_TILT_SERVO_MIN_US;
    int32_t offset_deg = (int32_t) angle_deg - (int32_t) PAN_TILT_MIN_DEG;
    int32_t pulse_us;

    if (angle_span <= 0) {
        return (uint16_t) PAN_TILT_SERVO_MIN_US;
    }

    pulse_us = (int32_t) PAN_TILT_SERVO_MIN_US +
        ((offset_deg * pulse_span) / angle_span);
    pulse_us = PanTilt_ClampI16(pulse_us,
        (int32_t) PAN_TILT_SERVO_MIN_US,
        (int32_t) PAN_TILT_SERVO_MAX_US);

    return (uint16_t) pulse_us;
}

static void PanTilt_SetAngle(int16_t angle_deg)
{
    g_pan_tilt.angle_deg = PanTilt_ClampI16(angle_deg,
        (int32_t) PAN_TILT_MIN_DEG,
        (int32_t) PAN_TILT_MAX_DEG);
    g_pan_tilt.pulse_us = PanTilt_AngleToPulseUs(g_pan_tilt.angle_deg);
}

static void PanTilt_WriteSignal(uint8_t high)
{
    if (high != 0U) {
        DL_GPIO_setPins(PAN_TILT_SERVO_PORT, PAN_TILT_SERVO_PIN);
    } else {
        DL_GPIO_clearPins(PAN_TILT_SERVO_PORT, PAN_TILT_SERVO_PIN);
    }
}

static void PanTilt_SendServoPulse(uint32_t now_ms)
{
    uint32_t cycles_per_us;

    if ((int32_t)(now_ms - g_pan_tilt.next_pulse_ms) < 0) {
        return;
    }

    g_pan_tilt.next_pulse_ms = now_ms + PAN_TILT_SERVO_PERIOD_MS;
    cycles_per_us = CPUCLK_FREQ / 1000000U;

    PanTilt_WriteSignal(1U);
    delay_cycles(cycles_per_us * (uint32_t) g_pan_tilt.pulse_us);
    PanTilt_WriteSignal(0U);
}

static void PanTilt_OledUpdateLine(uint8_t index, const char *text)
{
    uint8_t y;

    if ((index >= 4U) || (text == 0)) {
        return;
    }

    if (strncmp(g_pan_tilt.oled_line[index], text,
            sizeof(g_pan_tilt.oled_line[index])) == 0) {
        return;
    }

    (void) snprintf(g_pan_tilt.oled_line[index],
        sizeof(g_pan_tilt.oled_line[index]), "%s", text);
    y = (uint8_t)(index * 16U);
    OLED_ClearArea(0, y, 128U, 16U);
    OLED_ShowString(0, y, g_pan_tilt.oled_line[index], OLED_8X16);
    (void) OLED_UpdateAreaStatus(0, y, 128U, 16U);
}

static void PanTilt_UpdateOled(uint32_t now_ms)
{
    char line[18];

    if (g_pan_tilt.oled_enabled == 0U) {
        return;
    }
    if ((int32_t)(now_ms - g_pan_tilt.next_oled_ms) < 0) {
        return;
    }

    (void) snprintf(line, sizeof(line), "PAN DBG C%03u",
        (unsigned int) TASK1_OLED_CODE);
    PanTilt_OledUpdateLine(0U, line);
    (void) snprintf(line, sizeof(line), "ANG:%03d STEP:%02d",
        (int) g_pan_tilt.angle_deg,
        (int) PAN_TILT_STEP_DEG);
    PanTilt_OledUpdateLine(1U, line);
    (void) snprintf(line, sizeof(line), "PWM:%04uus",
        (unsigned int) g_pan_tilt.pulse_us);
    PanTilt_OledUpdateLine(2U, line);
    (void) snprintf(line, sizeof(line), "K1:+%02d K2:-%02d",
        (int) PAN_TILT_STEP_DEG,
        (int) PAN_TILT_STEP_DEG);
    PanTilt_OledUpdateLine(3U, line);

    g_pan_tilt.next_oled_ms = now_ms + PAN_TILT_OLED_PERIOD_MS;
}

void PanTiltDebug_Init(uint8_t oled_enabled)
{
    memset(&g_pan_tilt, 0, sizeof(g_pan_tilt));

    Key_Init();
    DL_GPIO_initDigitalOutputFeatures(PAN_TILT_SERVO_IOMUX,
        DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE,
        DL_GPIO_DRIVE_STRENGTH_LOW,
        DL_GPIO_HIZ_DISABLE);
    DL_GPIO_clearPins(PAN_TILT_SERVO_PORT, PAN_TILT_SERVO_PIN);
    DL_GPIO_enableOutput(PAN_TILT_SERVO_PORT, PAN_TILT_SERVO_PIN);

    g_pan_tilt.initialized = 1U;
    g_pan_tilt.oled_enabled = oled_enabled;
    PanTilt_SetAngle(PAN_TILT_CENTER_DEG);

    debug_printf("[PANDBG] init code=C%03u pin=PB18 center=%d step=%d range=%d..%d pulse=%u..%uus\r\n",
        (unsigned int) TASK1_OLED_CODE,
        (int) PAN_TILT_CENTER_DEG,
        (int) PAN_TILT_STEP_DEG,
        (int) PAN_TILT_MIN_DEG,
        (int) PAN_TILT_MAX_DEG,
        (unsigned int) PAN_TILT_SERVO_MIN_US,
        (unsigned int) PAN_TILT_SERVO_MAX_US);
}

void PanTiltDebug_Update(uint32_t now_ms)
{
    if (g_pan_tilt.initialized == 0U) {
        return;
    }

    if (Key_WasPressed(KEY_ID_1) != 0U) {
        PanTilt_SetAngle((int16_t)(g_pan_tilt.angle_deg +
            (int16_t) PAN_TILT_STEP_DEG));
        debug_printf("[PANDBG] KEY1 angle=%d pulse=%uus\r\n",
            (int) g_pan_tilt.angle_deg,
            (unsigned int) g_pan_tilt.pulse_us);
    }

    if (Key_WasPressed(KEY_ID_2) != 0U) {
        PanTilt_SetAngle((int16_t)(g_pan_tilt.angle_deg -
            (int16_t) PAN_TILT_STEP_DEG));
        debug_printf("[PANDBG] KEY2 angle=%d pulse=%uus\r\n",
            (int) g_pan_tilt.angle_deg,
            (unsigned int) g_pan_tilt.pulse_us);
    }

    PanTilt_SendServoPulse(now_ms);
    PanTilt_UpdateOled(now_ms);
}
