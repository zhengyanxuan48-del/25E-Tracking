#include "task1.h"

#include "MPU6050.h"
#include "OLED.h"
#include "debug_uart.h"
#include "key.h"
#include "platform_time.h"
#include "track.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    TASK1_TURN_FINISH_ANGLE = 0,
    TASK1_TURN_FINISH_TIMEOUT,
    TASK1_TURN_FINISH_LINE
} Task1TurnFinishReason;

typedef struct {
    Task1State state;
    uint8_t initialized;
    uint8_t done;
    uint8_t corner_count;
    uint8_t corner_confirm;
    uint8_t line_lost;
    int8_t last_seen_side;
    int8_t line_search_dir;
    int16_t base_pwm;
    int16_t last_error;
    int16_t last_correction;
    int16_t last_left_pwm;
    int16_t last_right_pwm;
    uint32_t run_start_ms;
    uint32_t pre_turn_start_ms;
    uint32_t pre_turn_until_ms;
    uint32_t turn_start_ms;
    uint32_t turn_settle_until_ms;
    uint32_t corner_ignore_until_ms;
    uint32_t line_lost_start_ms;
    uint32_t last_log_ms;
    uint32_t last_oled_ms;
    uint32_t next_sensor_sample_ms;
    float turn_start_yaw;
    float turn_target_yaw;
    float turn_target_abs_deg;
    float turn_turned_deg;
    float turn_remaining_deg;
} Task1Context;

static Task1Context g_ctx;
static char g_task1_oled_line[4][18];

static void Task1_StartTurn90(uint32_t now_ms);

static int16_t Task1_ClampI16(int32_t value, int32_t min_value,
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

static int16_t Task1_ClampPwm(int32_t pwm)
{
    return Task1_ClampI16(pwm, TASK1_PWM_MIN, TASK1_PWM_MAX);
}

static int16_t Task1_RoundFloatToI16(float value)
{
    if (value >= 0.0f) {
        return (int16_t)(value + 0.5f);
    }
    return (int16_t)(value - 0.5f);
}

static void Task1_FormatBinary8(uint8_t value, char out[9])
{
    uint8_t bit;

    if (out == 0) {
        return;
    }

    for (bit = 0U; bit < 8U; bit++) {
        out[bit] = ((value & (uint8_t)(1U << (7U - bit))) != 0U) ?
            '1' : '0';
    }
    out[8] = '\0';
}

static const char *Task1_StateName(Task1State state)
{
    switch (state) {
        case TASK1_STATE_STOP:
            return "STOP";
        case TASK1_STATE_LINE_FOLLOW:
            return "LINE";
        case TASK1_STATE_PRE_TURN_STRAIGHT:
            return "PRE";
        case TASK1_STATE_TURN_90:
            return "TURN";
        case TASK1_STATE_TURN_SETTLE:
            return "SETL";
        case TASK1_STATE_DONE:
            return "DONE";
        case TASK1_STATE_FAULT:
            return "FAULT";
        default:
            return "UNK";
    }
}

static float Task1_NormalizeAngleDeg(float angle_deg)
{
    while (angle_deg > 180.0f) {
        angle_deg -= 360.0f;
    }
    while (angle_deg < -180.0f) {
        angle_deg += 360.0f;
    }
    return angle_deg;
}

static float Task1_SignedAngleDeltaDeg(float current_deg, float start_deg)
{
    return Task1_NormalizeAngleDeg(current_deg - start_deg);
}

static void Task1_UpdateTurnAngles(float current_yaw)
{
    float turned;
    float remaining;

    turned = Task1_SignedAngleDeltaDeg(current_yaw, g_ctx.turn_start_yaw);
    turned *= (float) TASK1_CCW_YAW_SIGN;
    if (turned < 0.0f) {
        turned = -turned;
    }

    remaining = Task1_SignedAngleDeltaDeg(g_ctx.turn_target_yaw,
        current_yaw);
    if (remaining < 0.0f) {
        remaining = -remaining;
    }

    g_ctx.turn_turned_deg = turned;
    g_ctx.turn_remaining_deg = remaining;
}

static uint8_t Task1_IsCornerCandidate(uint8_t bits)
{
    return (bits == 0U) ? 1U : 0U;
}

static uint8_t Task1_IsCenterStraight(uint8_t bits)
{
    return ((bits & (uint8_t) TASK1_CENTER_STRAIGHT_MASK) ==
        (uint8_t) TASK1_CENTER_STRAIGHT_VALUE) ? 1U : 0U;
}

static uint8_t Task1_IsTurnLineDetected(uint8_t bits)
{
    uint8_t x45_black =
        ((bits & 0x18U) == 0x18U) ? 1U : 0U; /* X4 + X5 */
    uint8_t x56_black =
        ((bits & 0x30U) == 0x30U) ? 1U : 0U; /* X5 + X6 */

    return ((x45_black != 0U) || (x56_black != 0U)) ? 1U : 0U;
}

static uint8_t Task1_ReadTrackSample(uint32_t now_ms)
{
    if ((int32_t)(now_ms - g_ctx.next_sensor_sample_ms) >= 0) {
        uint8_t retry;
        uint8_t bits;

        g_ctx.next_sensor_sample_ms = now_ms + TASK1_SENSOR_SAMPLE_MS;
        bits = Track_ReadBits();
        for (retry = 0U; retry < TASK1_SENSOR_LOST_RETRY_COUNT; retry++) {
            if (bits != 0U) {
                break;
            }
            bits = Track_ReadBits();
        }
        return bits;
    }
    return Track_GetBlackBits();
}

static void Task1_SetMotorBoth(int16_t left_pwm, int16_t right_pwm)
{
    left_pwm = Task1_ClampPwm(left_pwm);
    right_pwm = Task1_ClampPwm(right_pwm);
    g_ctx.last_left_pwm = left_pwm;
    g_ctx.last_right_pwm = right_pwm;
    Motor_SetBoth(left_pwm, right_pwm);
}

static void Task1_SetSpinTurn(int16_t turn_pwm)
{
    /*
     * Spin turn: both wheels rotate at the same time in opposite directions.
     * TASK1_TURN_SIGN only selects clockwise/counter-clockwise physical turn.
     */
    Task1_SetMotorBoth(
        (int16_t)(-TASK1_TURN_SIGN * turn_pwm),
        (int16_t)(TASK1_TURN_SIGN * turn_pwm));
}

static void Task1_StopAll(void)
{
    g_ctx.last_left_pwm = 0;
    g_ctx.last_right_pwm = 0;
    Motor_StopAll();
}

static const char *Task1_SearchSideName(int8_t side)
{
    if (side < 0) {
        return "LEFT";
    }
    if (side > 0) {
        return "RIGHT";
    }
    return "CENTER";
}

static int8_t Task1_DefaultSearchDir(void)
{
#if (TASK1_LINE_SEARCH_DEFAULT_LEFT != 0U)
    return -1;
#else
    return 1;
#endif
}

static void Task1_UpdateLastSeenSide(int16_t error)
{
    if (error < -(int16_t) TASK1_CENTER_ERR_THRESHOLD) {
        g_ctx.last_seen_side = -1;
    } else if (error > (int16_t) TASK1_CENTER_ERR_THRESHOLD) {
        g_ctx.last_seen_side = 1;
    } else {
        g_ctx.last_seen_side = 0;
    }
}

static int8_t Task1_SelectLineSearchDir(void)
{
    if (g_ctx.last_seen_side < 0) {
        return -1;
    }
    if (g_ctx.last_seen_side > 0) {
        return 1;
    }
    if (g_ctx.last_error < -(int16_t) TASK1_CENTER_ERR_THRESHOLD) {
        return -1;
    }
    if (g_ctx.last_error > (int16_t) TASK1_CENTER_ERR_THRESHOLD) {
        return 1;
    }
    return Task1_DefaultSearchDir();
}

static void Task1_ApplyLineSearch(void)
{
    if (g_ctx.line_search_dir == 0) {
        g_ctx.line_search_dir = Task1_SelectLineSearchDir();
    }

    g_ctx.last_correction = 0;
    if (g_ctx.line_search_dir < 0) {
        Task1_SetMotorBoth(
            (int16_t)(-TASK1_LINE_SEARCH_PWM_INNER),
            (int16_t) TASK1_LINE_SEARCH_PWM_OUTER);
    } else {
        Task1_SetMotorBoth(
            (int16_t) TASK1_LINE_SEARCH_PWM_OUTER,
            (int16_t)(-TASK1_LINE_SEARCH_PWM_INNER));
    }
}

static int16_t Task1_CalcLineCorrection(uint8_t bits)
{
    int16_t error = Track_GetLineError();
    int16_t d_error = (int16_t)(error - g_ctx.last_error);
    float correction_f;
    int16_t correction;

    if (Task1_IsCenterStraight(bits) != 0U) {
        Task1_UpdateLastSeenSide(0);
        g_ctx.last_error = 0;
        g_ctx.last_correction = 0;
        return 0;
    }

    correction_f =
        (TASK1_STEER_KP * (float) error) +
        (TASK1_STEER_KD * (float) d_error);
    correction_f *= (float) TASK1_STEER_SIGN;
    correction = Task1_RoundFloatToI16(correction_f);
    correction = Task1_ClampI16(correction,
        -(int32_t) TASK1_CORRECTION_MAX,
        (int32_t) TASK1_CORRECTION_MAX);

    g_ctx.last_error = error;
    Task1_UpdateLastSeenSide(error);
    g_ctx.last_correction = correction;
    return correction;
}

static void Task1_ApplyLineFollow(uint8_t bits)
{
    int16_t correction = Task1_CalcLineCorrection(bits);
    int16_t left_pwm = Task1_ClampPwm((int32_t) g_ctx.base_pwm - correction);
    int16_t right_pwm = Task1_ClampPwm((int32_t) g_ctx.base_pwm + correction);

    Task1_SetMotorBoth(left_pwm, right_pwm);
}

static void Task1_StartRun(uint32_t now_ms, uint8_t restart_by_key)
{
    MPU6050_ResetYaw();
    g_ctx.done = 0U;
    g_ctx.corner_count = 0U;
    g_ctx.corner_confirm = 0U;
    g_ctx.line_lost = 0U;
    g_ctx.last_seen_side = 0;
    g_ctx.line_search_dir = Task1_DefaultSearchDir();
    g_ctx.base_pwm = TASK1_BASE_PWM_DEFAULT;
    g_ctx.last_error = 0;
    g_ctx.last_correction = 0;
    g_ctx.run_start_ms = now_ms;
    g_ctx.pre_turn_start_ms = 0U;
    g_ctx.pre_turn_until_ms = 0U;
    g_ctx.turn_start_ms = 0U;
    g_ctx.turn_settle_until_ms = 0U;
    g_ctx.corner_ignore_until_ms = now_ms + TASK1_CORNER_IGNORE_MS;
    g_ctx.line_lost_start_ms = now_ms;
    g_ctx.last_log_ms = 0U;
    g_ctx.last_oled_ms = 0U;
    g_ctx.next_sensor_sample_ms = now_ms;
    g_ctx.turn_start_yaw = 0.0f;
    g_ctx.turn_target_yaw = 0.0f;
    g_ctx.turn_target_abs_deg = 0.0f;
    g_ctx.turn_turned_deg = 0.0f;
    g_ctx.turn_remaining_deg = 0.0f;
    g_ctx.state = TASK1_STATE_LINE_FOLLOW;
    Task1_StopAll();

    debug_print((restart_by_key != 0U) ?
        "[T1] restart by KEY1\r\n" : "[T1] auto start\r\n");
}

static void Task1_EnterStop(void)
{
    g_ctx.state = TASK1_STATE_STOP;
    g_ctx.done = 0U;
    g_ctx.corner_confirm = 0U;
    g_ctx.line_lost = 0U;
    g_ctx.line_search_dir = Task1_DefaultSearchDir();
    Task1_StopAll();
}

static void Task1_StartPreTurnStraight(uint32_t now_ms)
{
    g_ctx.corner_confirm = 0U;
    g_ctx.last_error = 0;
    g_ctx.last_correction = 0;
    g_ctx.line_lost = 0U;
    g_ctx.line_search_dir = Task1_DefaultSearchDir();

    if (TASK1_PRE_TURN_STRAIGHT_MS == 0U) {
        debug_print("[T1] corner detected -> turn immediately\r\n");
        Task1_StartTurn90(now_ms);
        return;
    }

    g_ctx.state = TASK1_STATE_PRE_TURN_STRAIGHT;
    g_ctx.pre_turn_start_ms = now_ms;
    g_ctx.pre_turn_until_ms = now_ms + TASK1_PRE_TURN_STRAIGHT_MS;
    Task1_SetMotorBoth(TASK1_PRE_TURN_PWM, TASK1_PRE_TURN_PWM);

    debug_printf("[T1] corner detected -> pre straight ms=%u pwm=%d until=%lu\r\n",
        (unsigned int) TASK1_PRE_TURN_STRAIGHT_MS,
        (int) TASK1_PRE_TURN_PWM,
        (unsigned long) g_ctx.pre_turn_until_ms);
}

static void Task1_StartTurn90(uint32_t now_ms)
{
    float current_yaw = MPU6050_GetYaw();
    uint8_t corner_in_lap =
        (uint8_t)((g_ctx.corner_count % TASK1_CORNERS_PER_LAP) + 1U);

    g_ctx.state = TASK1_STATE_TURN_90;
    g_ctx.turn_start_ms = now_ms;
    g_ctx.turn_start_yaw = current_yaw;
    g_ctx.turn_target_abs_deg =
        (float) corner_in_lap * (float) TASK1_TURN_TARGET_DEG;
    g_ctx.turn_target_yaw = Task1_NormalizeAngleDeg(
        (float) TASK1_CCW_YAW_SIGN * g_ctx.turn_target_abs_deg);
    g_ctx.turn_turned_deg = 0.0f;
    Task1_UpdateTurnAngles(current_yaw);
    g_ctx.last_error = 0;
    g_ctx.last_correction = 0;

    debug_printf("[T1] turn start corner_next=%u base_yaw=%+.1f target_abs=%.1f target_yaw=%+.1f rem=%.1f\r\n",
        (unsigned int) corner_in_lap,
        current_yaw,
        g_ctx.turn_target_abs_deg,
        g_ctx.turn_target_yaw,
        g_ctx.turn_remaining_deg);
}

static const char *Task1_TurnFinishReasonName(Task1TurnFinishReason reason)
{
    switch (reason) {
        case TASK1_TURN_FINISH_ANGLE:
            return "angle";
        case TASK1_TURN_FINISH_TIMEOUT:
            return "timeout";
        case TASK1_TURN_FINISH_LINE:
            return "line";
        default:
            return "unknown";
    }
}

static void Task1_CompleteTask(uint32_t now_ms)
{
    g_ctx.state = TASK1_STATE_DONE;
    g_ctx.done = 1U;
    Task1_StopAll();
    debug_printf("[T1] done laps=%u total_time_ms=%lu\r\n",
        (unsigned int) TASK1_TARGET_LAPS,
        (unsigned long)(now_ms - g_ctx.run_start_ms));
}

static void Task1_FinishTurn(uint32_t now_ms, float current_yaw,
    Task1TurnFinishReason reason)
{
    uint8_t count_corner = 1U;

    Task1_UpdateTurnAngles(current_yaw);
    Task1_StopAll();

    if ((reason == TASK1_TURN_FINISH_TIMEOUT) &&
        (TASK1_COUNT_TIMEOUT_TURN == 0U)) {
        count_corner = 0U;
    }

    if (count_corner != 0U) {
        g_ctx.corner_count++;
    }

    g_ctx.corner_confirm = 0U;
    g_ctx.corner_ignore_until_ms = now_ms + TASK1_CORNER_IGNORE_MS;
    g_ctx.line_lost = 0U;
    g_ctx.line_search_dir = Task1_DefaultSearchDir();
    g_ctx.line_lost_start_ms = now_ms;
    g_ctx.state = TASK1_STATE_TURN_SETTLE;
    g_ctx.turn_settle_until_ms = now_ms + TASK1_TURN_SETTLE_MS;

    debug_printf("[T1] turn done reason=%s corner=%u start=%+.1f current=%+.1f target_abs=%.1f target_yaw=%+.1f turned=%.1f rem=%.1f\r\n",
        Task1_TurnFinishReasonName(reason),
        (unsigned int) g_ctx.corner_count,
        g_ctx.turn_start_yaw,
        current_yaw,
        g_ctx.turn_target_abs_deg,
        g_ctx.turn_target_yaw,
        g_ctx.turn_turned_deg,
        g_ctx.turn_remaining_deg);

    if (g_ctx.corner_count >= TASK1_TARGET_CORNERS) {
        Task1_CompleteTask(now_ms);
    }
}

static void Task1_OledUpdateLine(uint8_t index, const char *text)
{
    uint8_t y;

    if ((index >= 4U) || (text == 0)) {
        return;
    }

    if (strncmp(g_task1_oled_line[index], text,
            sizeof(g_task1_oled_line[index])) == 0) {
        return;
    }

    (void) snprintf(g_task1_oled_line[index],
        sizeof(g_task1_oled_line[index]), "%s", text);
    y = (uint8_t)(index * 16U);
    OLED_ClearArea(0, y, 128U, 16U);
    OLED_ShowString(0, y, g_task1_oled_line[index], OLED_8X16);
    (void) OLED_UpdateAreaStatus(0, y, 128U, 16U);
}

static void Task1_UpdateOled(uint8_t bits)
{
    char bits_text[9];
    char line0[18];
    char line1[18];
    char line2[18];
    char line3[18];
    float yaw = MPU6050_GetYaw();

    (void) bits;
    Task1_FormatBinary8(Track_GetRawBits(), bits_text);

    (void) snprintf(line0, sizeof(line0), "T1:%s C%03u",
        Task1_StateName(g_ctx.state), (unsigned int) TASK1_OLED_CODE);
    if (g_ctx.state == TASK1_STATE_PRE_TURN_STRAIGHT) {
        uint32_t now_ms = platform_time_ms();
        uint32_t remain_ms = ((int32_t)(g_ctx.pre_turn_until_ms - now_ms) > 0) ?
            (g_ctx.pre_turn_until_ms - now_ms) : 0U;
        (void) snprintf(line1, sizeof(line1), "Y:%+05.1f P:%04lu",
            yaw, (unsigned long) remain_ms);
    } else {
        (void) snprintf(line1, sizeof(line1), "Y:%+06.1f R:%03.0f",
            yaw, g_ctx.turn_remaining_deg);
    }
    if (g_ctx.state == TASK1_STATE_TURN_90) {
        (void) snprintf(line2, sizeof(line2), "A:%03.0f C:%u/%u",
            g_ctx.turn_target_abs_deg,
            (unsigned int) g_ctx.corner_count,
            (unsigned int) TASK1_TARGET_CORNERS);
    } else {
        (void) snprintf(line2, sizeof(line2), "C:%u/%u E:%+03d",
            (unsigned int) g_ctx.corner_count,
            (unsigned int) TASK1_TARGET_CORNERS,
            (int) Track_GetLineError());
    }
    (void) snprintf(line3, sizeof(line3), "IR:%s", bits_text);

    Task1_OledUpdateLine(0U, line0);
    Task1_OledUpdateLine(1U, line1);
    Task1_OledUpdateLine(2U, line2);
    Task1_OledUpdateLine(3U, line3);
}

static void Task1_LogConfig(void)
{
    debug_printf("[T1CFG] code=C%03u base=%d kp=%.1f kd=%.1f sign=%d corr_max=%d\r\n",
        (unsigned int) TASK1_OLED_CODE,
        (int) TASK1_BASE_PWM_DEFAULT,
        (float) TASK1_STEER_KP,
        (float) TASK1_STEER_KD,
        (int) TASK1_STEER_SIGN,
        (int) TASK1_CORRECTION_MAX);
    debug_printf("[T1CFG] sensor_period=%ums lost_retry=%u lost_pwm=%d lost_timeout=disabled\r\n",
        (unsigned int) TASK1_SENSOR_SAMPLE_MS,
        (unsigned int) TASK1_SENSOR_LOST_RETRY_COUNT,
        (int) TASK1_LINE_LOST_PWM);
    debug_printf("[T1CFG] masked-corner line_search inner=%d outer=%d default=%s center_th=%d\r\n",
        (int) TASK1_LINE_SEARCH_PWM_INNER,
        (int) TASK1_LINE_SEARCH_PWM_OUTER,
        Task1_SearchSideName(Task1_DefaultSearchDir()),
        (int) TASK1_CENTER_ERR_THRESHOLD);
    debug_printf("[T1CFG] corner mode=white confirm=%u pre=%ums ignore=%ums\r\n",
        (unsigned int) TASK1_CORNER_CONFIRM_COUNT,
        (unsigned int) TASK1_PRE_TURN_STRAIGHT_MS,
        (unsigned int) TASK1_CORNER_IGNORE_MS);
    debug_printf("[T1CFG] turn absolute step=%.1f targets=90/180/270/360 done=%.1f fast=%d slow=%d sign=%d yaw_sign=%d timeout=%ums\r\n",
        (float) TASK1_TURN_TARGET_DEG,
        (float) TASK1_TURN_DONE_DEG,
        (int) TASK1_TURN_PWM_FAST,
        (int) TASK1_TURN_PWM_SLOW,
        (int) TASK1_TURN_SIGN,
        (int) TASK1_CCW_YAW_SIGN,
        (unsigned int) TASK1_TURN_TIMEOUT_MS);
    debug_printf("[T1CFG] turn motor mode=spin lpwm=-sign*pwm rpwm=+sign*pwm\r\n");
    debug_printf("[T1CFG] turn line_exit pair=X45/X56 mask=0x%02X min=%ums\r\n",
        (unsigned int) TASK1_TURN_LINE_EXIT_MASK,
        (unsigned int) TASK1_TURN_LINE_EXIT_MIN_MS);
}

static void Task1_LogLine(uint32_t now_ms, uint8_t bits,
    uint8_t corner_candidate)
{
    if ((int32_t)(now_ms - g_ctx.last_log_ms) < 0) {
        return;
    }

    debug_printf("[T1] state=%s bits=0x%02X err=%d corr=%d lpwm=%d rpwm=%d corner=%u/%u lost=%u sdir=%s yaw=%.1f\r\n",
        Task1_StateName(g_ctx.state),
        (unsigned int) bits,
        (int) Track_GetLineError(),
        (int) g_ctx.last_correction,
        (int) g_ctx.last_left_pwm,
        (int) g_ctx.last_right_pwm,
        (unsigned int) corner_candidate,
        (unsigned int) g_ctx.corner_confirm,
        (unsigned int) g_ctx.line_lost,
        Task1_SearchSideName(g_ctx.line_search_dir),
        MPU6050_GetYaw());

    g_ctx.last_log_ms = now_ms + TASK1_DEBUG_LOG_PERIOD_MS;
}

static void Task1_LogTurn(uint32_t now_ms, uint8_t bits, float yaw,
    int16_t turn_pwm)
{
    if ((int32_t)(now_ms - g_ctx.last_log_ms) < 0) {
        return;
    }

    debug_printf("[T1] turn bits=0x%02X line=%u yaw0=%+.1f yaw=%+.1f target_abs=%.1f target_yaw=%+.1f turned=%.1f rem=%.1f pwm=%d lpwm=%d rpwm=%d\r\n",
        (unsigned int) bits,
        (unsigned int) Task1_IsTurnLineDetected(bits),
        g_ctx.turn_start_yaw,
        yaw,
        g_ctx.turn_target_abs_deg,
        g_ctx.turn_target_yaw,
        g_ctx.turn_turned_deg,
        g_ctx.turn_remaining_deg,
        (int) turn_pwm,
        (int) g_ctx.last_left_pwm,
        (int) g_ctx.last_right_pwm);

    g_ctx.last_log_ms = now_ms + TASK1_TURN_LOG_PERIOD_MS;
}

static void Task1_UpdateLineFollow(uint32_t now_ms, uint8_t bits)
{
    uint8_t corner_candidate = Task1_IsCornerCandidate(bits);
    uint8_t may_detect_corner =
        ((int32_t)(now_ms - g_ctx.corner_ignore_until_ms) >= 0) ? 1U : 0U;

    if (bits == 0U) {
        if (g_ctx.line_lost == 0U) {
            g_ctx.line_lost = 1U;
            g_ctx.line_lost_start_ms = now_ms;
            g_ctx.line_search_dir = Task1_SelectLineSearchDir();
            g_ctx.last_correction = 0;
            debug_printf("[T1] line lost corner_masked=%u search=%s last_err=%d\r\n",
                (unsigned int)(may_detect_corner == 0U),
                Task1_SearchSideName(g_ctx.line_search_dir),
                (int) g_ctx.last_error);
        }

        if (may_detect_corner != 0U) {
            if (g_ctx.corner_confirm < TASK1_CORNER_CONFIRM_COUNT) {
                g_ctx.corner_confirm++;
            }
            if (g_ctx.corner_confirm >= TASK1_CORNER_CONFIRM_COUNT) {
                Task1_StartPreTurnStraight(now_ms);
                return;
            }

            Task1_SetMotorBoth(TASK1_LINE_LOST_PWM, TASK1_LINE_LOST_PWM);
        } else {
            g_ctx.corner_confirm = 0U;
            Task1_ApplyLineSearch();
        }
        Task1_LogLine(now_ms, bits, corner_candidate);
        return;
    }

    if (g_ctx.line_lost != 0U) {
        g_ctx.line_lost = 0U;
        g_ctx.last_error = Track_GetLineError();
        Task1_UpdateLastSeenSide(g_ctx.last_error);
        debug_printf("[T1] line reacquired bits=0x%02X err=%d side=%s\r\n",
            (unsigned int) bits,
            (int) g_ctx.last_error,
            Task1_SearchSideName(g_ctx.last_seen_side));
        g_ctx.line_search_dir = Task1_DefaultSearchDir();
    }

    if ((corner_candidate != 0U) && (may_detect_corner != 0U)) {
        if (g_ctx.corner_confirm < TASK1_CORNER_CONFIRM_COUNT) {
            g_ctx.corner_confirm++;
        }
    } else {
        g_ctx.corner_confirm = 0U;
    }

    if (g_ctx.corner_confirm >= TASK1_CORNER_CONFIRM_COUNT) {
        Task1_StartPreTurnStraight(now_ms);
        return;
    }

    Task1_ApplyLineFollow(bits);
    Task1_LogLine(now_ms, bits, corner_candidate);
}

static void Task1_UpdateTurn90(uint32_t now_ms, uint8_t bits)
{
    uint32_t elapsed_ms = now_ms - g_ctx.turn_start_ms;
    float yaw = MPU6050_GetYaw();
    int16_t turn_pwm;

    Task1_UpdateTurnAngles(yaw);

    if ((elapsed_ms >= TASK1_TURN_LINE_EXIT_MIN_MS) &&
        (Task1_IsTurnLineDetected(bits) != 0U)) {
        Task1_FinishTurn(now_ms, yaw, TASK1_TURN_FINISH_LINE);
        return;
    }

    if (g_ctx.turn_remaining_deg <= TASK1_TURN_DONE_DEG) {
        Task1_FinishTurn(now_ms, yaw, TASK1_TURN_FINISH_ANGLE);
        return;
    }

    if (elapsed_ms >= TASK1_TURN_TIMEOUT_MS) {
        Task1_FinishTurn(now_ms, yaw, TASK1_TURN_FINISH_TIMEOUT);
        return;
    }

    turn_pwm = (g_ctx.turn_remaining_deg > TASK1_TURN_SLOW_ZONE_DEG) ?
        TASK1_TURN_PWM_FAST : TASK1_TURN_PWM_SLOW;
    Task1_SetSpinTurn(turn_pwm);
    Task1_LogTurn(now_ms, bits, yaw, turn_pwm);
}

void Task1_SetBaseSpeed(int16_t pwm)
{
    g_ctx.base_pwm = Task1_ClampPwm(pwm);
}

uint8_t Task1_IsDone(void)
{
    return (g_ctx.done != 0U) ? 1U : 0U;
}

void Task1_Init(void)
{
    uint32_t now_ms = platform_time_ms();

    memset(&g_ctx, 0, sizeof(g_ctx));
    memset(g_task1_oled_line, 0, sizeof(g_task1_oled_line));

    g_ctx.state = TASK1_STATE_STOP;
    g_ctx.base_pwm = TASK1_BASE_PWM_DEFAULT;
    g_ctx.corner_ignore_until_ms = now_ms;
    g_ctx.line_lost_start_ms = now_ms;

    Motor_Init();
    Key_Init();
    Track_Init();
    Task1_StopAll();

    g_ctx.initialized = 1U;
    Task1_LogConfig();
    Task1_StartRun(now_ms, 0U);
}

void Task1_Update(uint32_t now_ms)
{
    uint8_t bits;
    uint8_t key2_pressed;
    uint8_t key1_pressed;

    if (g_ctx.initialized == 0U) {
        return;
    }

    key2_pressed = Key_WasPressed(KEY_ID_2);
    key1_pressed = Key_WasPressed(KEY_ID_1);
    bits = Task1_ReadTrackSample(now_ms);

    if (key2_pressed != 0U) {
        Task1_EnterStop();
        debug_print("[T1] stop by KEY2\r\n");
        return;
    }

    if ((g_ctx.state == TASK1_STATE_STOP) ||
        (g_ctx.state == TASK1_STATE_DONE) ||
        (g_ctx.state == TASK1_STATE_FAULT)) {
        Task1_StopAll();

        if (key1_pressed != 0U) {
            Task1_StartRun(now_ms, 1U);
        }

        if ((int32_t)(now_ms - g_ctx.last_oled_ms) >= 0) {
            Task1_UpdateOled(bits);
            g_ctx.last_oled_ms = now_ms + TASK1_OLED_INTERVAL_MS;
        }
        return;
    }

    if (g_ctx.state == TASK1_STATE_LINE_FOLLOW) {
        Task1_UpdateLineFollow(now_ms, bits);
    } else if (g_ctx.state == TASK1_STATE_PRE_TURN_STRAIGHT) {
        if ((int32_t)(now_ms - g_ctx.pre_turn_until_ms) >= 0) {
            debug_printf("[T1] pre straight done elapsed=%lums\r\n",
                (unsigned long)(now_ms - g_ctx.pre_turn_start_ms));
            Task1_StartTurn90(now_ms);
        } else {
            Task1_SetMotorBoth(TASK1_PRE_TURN_PWM, TASK1_PRE_TURN_PWM);
        }
    } else if (g_ctx.state == TASK1_STATE_TURN_90) {
        Task1_UpdateTurn90(now_ms, bits);
    } else if (g_ctx.state == TASK1_STATE_TURN_SETTLE) {
        if ((int32_t)(now_ms - g_ctx.turn_settle_until_ms) >= 0) {
            if (g_ctx.state != TASK1_STATE_DONE) {
                g_ctx.state = TASK1_STATE_LINE_FOLLOW;
                g_ctx.last_error = Track_GetLineError();
                Task1_UpdateLastSeenSide(g_ctx.last_error);
                g_ctx.last_correction = 0;
                debug_print("[T1] back to line follow\r\n");
            }
        } else {
            Task1_StopAll();
        }
    }

    if ((int32_t)(now_ms - g_ctx.last_oled_ms) >= 0) {
        Task1_UpdateOled(bits);
        g_ctx.last_oled_ms = now_ms + TASK1_OLED_INTERVAL_MS;
    }
}
