#include "motor_open_loop_test.h"

#include "OLED.h"
#include "debug_uart.h"
#include "encoder.h"
#include "motor.h"
#include "platform_time.h"

#include <stdio.h>
#include <string.h>

#define MOTOR_OL_STAGE_MS       (2000U)
#define MOTOR_OL_UART_MS        (200U)
#define MOTOR_OL_OLED_MS        (100U)
#define MOTOR_OL_SAMPLE_MS      (200U)
#define MOTOR_OL_OLED_WIDTH     (128U)
#define MOTOR_OL_OLED_HEIGHT    (16U)
#define MOTOR_OL_STAGE_COUNT    (4U)

typedef struct {
    int16_t pwm;
    int32_t left_sum;
    int32_t right_sum;
    uint16_t samples;
    int32_t left_last;
    int32_t right_last;
} MotorOpenLoopStage;

typedef struct {
    uint8_t initialized;
    uint8_t oled_enabled;
    uint8_t done;
    uint8_t summary_printed;
    uint8_t stage_index;
    uint32_t stage_start_ms;
    uint32_t next_uart_ms;
    uint32_t next_oled_ms;
    uint32_t next_sample_ms;
    char oled_line[4][18];
    MotorOpenLoopStage stage[MOTOR_OL_STAGE_COUNT];
} MotorOpenLoopContext;

static MotorOpenLoopContext g_motor_ol;

static void MotorOpenLoop_UpdateOledLine(uint8_t line_index, const char *text)
{
    uint8_t y;

    if ((line_index >= 4U) || (text == NULL)) {
        return;
    }

    if (strncmp(g_motor_ol.oled_line[line_index], text,
            sizeof(g_motor_ol.oled_line[line_index])) == 0) {
        return;
    }

    (void) snprintf(g_motor_ol.oled_line[line_index],
        sizeof(g_motor_ol.oled_line[line_index]), "%s", text);

    y = (uint8_t)(line_index * MOTOR_OL_OLED_HEIGHT);
    OLED_ClearArea(0, y, MOTOR_OL_OLED_WIDTH, MOTOR_OL_OLED_HEIGHT);
    OLED_ShowString(0, y, g_motor_ol.oled_line[line_index], OLED_8X16);
    (void) OLED_UpdateAreaStatus(0, y, MOTOR_OL_OLED_WIDTH,
        MOTOR_OL_OLED_HEIGHT);
}

static int32_t MotorOpenLoop_Abs32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int32_t MotorOpenLoop_Avg(int32_t sum, uint16_t samples)
{
    if (samples == 0U) {
        return 0;
    }

    return sum / (int32_t) samples;
}

static void MotorOpenLoop_ApplyStage(uint8_t index, uint32_t now_ms)
{
    int16_t pwm = g_motor_ol.stage[index].pwm;

    g_motor_ol.stage_index = index;
    g_motor_ol.stage_start_ms = now_ms;
    g_motor_ol.next_uart_ms = now_ms;
    g_motor_ol.next_oled_ms = now_ms;
    g_motor_ol.next_sample_ms = now_ms;

    Motor_SetBoth(pwm, pwm);
    debug_printf("[MOTORTEST] stage=%u pwm=%d start\r\n",
        (unsigned int)(index + 1U), (int) pwm);
}

static void MotorOpenLoop_Sample(uint32_t now_ms)
{
    MotorOpenLoopStage *stage;
    int32_t left_rpm;
    int32_t right_rpm;

    if ((int32_t)(now_ms - g_motor_ol.next_sample_ms) < 0) {
        return;
    }

    stage = &g_motor_ol.stage[g_motor_ol.stage_index];
    left_rpm = Encoder_GetRPM(ENCODER_LEFT);
    right_rpm = Encoder_GetRPM(ENCODER_RIGHT);

    stage->left_sum += left_rpm;
    stage->right_sum += right_rpm;
    stage->samples++;
    stage->left_last = left_rpm;
    stage->right_last = right_rpm;

    g_motor_ol.next_sample_ms = now_ms + MOTOR_OL_SAMPLE_MS;
}

static void MotorOpenLoop_Log(uint32_t now_ms)
{
    const MotorOpenLoopStage *stage;

    if ((int32_t)(now_ms - g_motor_ol.next_uart_ms) < 0) {
        return;
    }

    stage = &g_motor_ol.stage[g_motor_ol.stage_index];
    debug_printf("[MOTORTEST] pwm=%d Lrpm=%ld Rrpm=%ld\r\n",
        (int) stage->pwm,
        (long) Encoder_GetRPM(ENCODER_LEFT),
        (long) Encoder_GetRPM(ENCODER_RIGHT));

    g_motor_ol.next_uart_ms = now_ms + MOTOR_OL_UART_MS;
}

static void MotorOpenLoop_UpdateOled(uint32_t now_ms)
{
    const MotorOpenLoopStage *stage;
    char line[4][18];

    if (g_motor_ol.oled_enabled == 0U) {
        return;
    }

    if ((int32_t)(now_ms - g_motor_ol.next_oled_ms) < 0) {
        return;
    }

    stage = &g_motor_ol.stage[g_motor_ol.stage_index];

    (void) snprintf(line[0], sizeof(line[0]), "PWM:%d", (int) stage->pwm);
    (void) snprintf(line[1], sizeof(line[1]), "L:%ld",
        (long) Encoder_GetRPM(ENCODER_LEFT));
    (void) snprintf(line[2], sizeof(line[2]), "R:%ld",
        (long) Encoder_GetRPM(ENCODER_RIGHT));
    (void) snprintf(line[3], sizeof(line[3]), "S:%u/%u",
        (unsigned int)(g_motor_ol.stage_index + 1U),
        (unsigned int) MOTOR_OL_STAGE_COUNT);

    MotorOpenLoop_UpdateOledLine(0U, line[0]);
    MotorOpenLoop_UpdateOledLine(1U, line[1]);
    MotorOpenLoop_UpdateOledLine(2U, line[2]);
    MotorOpenLoop_UpdateOledLine(3U, line[3]);

    g_motor_ol.next_oled_ms = now_ms + MOTOR_OL_OLED_MS;
}

static void MotorOpenLoop_PrintSummary(void)
{
    uint8_t index;
    int32_t pwm300_avg;
    int32_t pwm900_avg;

    if (g_motor_ol.summary_printed != 0U) {
        return;
    }

    debug_print("[MOTORTEST] summary begin\r\n");
    for (index = 0U; index < MOTOR_OL_STAGE_COUNT; index++) {
        const MotorOpenLoopStage *stage = &g_motor_ol.stage[index];
        int32_t left_avg = MotorOpenLoop_Avg(stage->left_sum, stage->samples);
        int32_t right_avg =
            MotorOpenLoop_Avg(stage->right_sum, stage->samples);

        debug_printf("[MOTORTEST] result pwm=%d Lavg=%ld Ravg=%ld Llast=%ld Rlast=%ld samples=%u\r\n",
            (int) stage->pwm,
            (long) left_avg,
            (long) right_avg,
            (long) stage->left_last,
            (long) stage->right_last,
            (unsigned int) stage->samples);
    }

    pwm300_avg =
        (MotorOpenLoop_Abs32(MotorOpenLoop_Avg(g_motor_ol.stage[0].left_sum,
             g_motor_ol.stage[0].samples)) +
         MotorOpenLoop_Abs32(MotorOpenLoop_Avg(g_motor_ol.stage[0].right_sum,
             g_motor_ol.stage[0].samples))) / 2L;
    pwm900_avg =
        (MotorOpenLoop_Abs32(MotorOpenLoop_Avg(g_motor_ol.stage[3].left_sum,
             g_motor_ol.stage[3].samples)) +
         MotorOpenLoop_Abs32(MotorOpenLoop_Avg(g_motor_ol.stage[3].right_sum,
             g_motor_ol.stage[3].samples))) / 2L;

    if (pwm900_avg < 5L) {
        debug_print("[MOTORTEST] judge: 900 pwm has almost no rpm, check PWM mapping, TB6612 STBY/VM, motor wiring, PA16/PA17\r\n");
    } else if (pwm900_avg < (pwm300_avg + 5L)) {
        debug_print("[MOTORTEST] judge: rpm does not rise with pwm, check PWM compare/period or power limit\r\n");
    } else {
        debug_print("[MOTORTEST] judge: open-loop motor output responds; if task1 crawls, check task1 base pwm, slew, correction, encoder balance\r\n");
    }

    debug_print("[MOTORTEST] summary end\r\n");
    g_motor_ol.summary_printed = 1U;
}

void MotorOpenLoopTest_Init(uint8_t oled_enabled)
{
    uint32_t now_ms = platform_time_ms();

    memset(&g_motor_ol, 0, sizeof(g_motor_ol));

    g_motor_ol.oled_enabled = (oled_enabled != 0U) ? 1U : 0U;
    g_motor_ol.stage[0].pwm = 300;
    g_motor_ol.stage[1].pwm = 500;
    g_motor_ol.stage[2].pwm = 700;
    g_motor_ol.stage[3].pwm = 900;

    Motor_Init();
    Encoder_Init();
    Encoder_Reset(ENCODER_LEFT);
    Encoder_Reset(ENCODER_RIGHT);
    Motor_StopAll();

    g_motor_ol.initialized = 1U;
    debug_print("[MOTORTEST] open loop test: lift wheels before power-on\r\n");
    MotorOpenLoop_ApplyStage(0U, now_ms);
}

void MotorOpenLoopTest_Update(uint32_t now_ms)
{
    if (g_motor_ol.initialized == 0U) {
        return;
    }

    if (g_motor_ol.done != 0U) {
        Motor_StopAll();
        MotorOpenLoop_PrintSummary();
        return;
    }

    MotorOpenLoop_Sample(now_ms);
    MotorOpenLoop_Log(now_ms);
    MotorOpenLoop_UpdateOled(now_ms);

    if ((uint32_t)(now_ms - g_motor_ol.stage_start_ms) >=
        MOTOR_OL_STAGE_MS) {
        uint8_t next_stage = (uint8_t)(g_motor_ol.stage_index + 1U);

        if (next_stage >= MOTOR_OL_STAGE_COUNT) {
            g_motor_ol.done = 1U;
            Motor_StopAll();
            debug_print("[MOTORTEST] stop\r\n");
            MotorOpenLoop_PrintSummary();
        } else {
            MotorOpenLoop_ApplyStage(next_stage, now_ms);
        }
    }
}

uint8_t MotorOpenLoopTest_IsDone(void)
{
    return (g_motor_ol.done != 0U) ? 1U : 0U;
}
