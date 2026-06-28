#ifndef HARDWARE_TASK1_H
#define HARDWARE_TASK1_H

#include "motor.h"

#include <stdint.h>

#ifndef APP_MODE_TASK1
#define APP_MODE_TASK1 (1U) // 调试参数：使能 task1 主程序模式
#endif

// ===== Task1 tunable parameters =====

#ifndef TASK1_OLED_CODE
#define TASK1_OLED_CODE (49U) // 调试参数：OLED 显示代码号，每次修改后加 1，便于确认烧录版本
#endif

#ifndef TASK1_BASE_PWM_DEFAULT
#define TASK1_BASE_PWM_DEFAULT (MOTOR_PWM_MAX) // 调试参数：直线循迹基础 PWM，数值越大直线越快
#endif

#ifndef TASK1_STEER_KP
#define TASK1_STEER_KP (20.0f) // 调试参数：循迹比例系数，用于控制偏差响应强度
#endif

#ifndef TASK1_STEER_KD
#define TASK1_STEER_KD (18.0f) // 调试参数：循迹微分系数，用于抑制高速循迹抖动
#endif

#ifndef TASK1_STEER_SIGN
#define TASK1_STEER_SIGN (-1) // 调试参数：循迹修正方向，越纠越偏时改为 -1
#endif

#ifndef TASK1_CORRECTION_MAX
#define TASK1_CORRECTION_MAX (160) // 调试参数：循迹最大 PWM 差值，用于限制左右轮差速
#endif

#ifndef TASK1_LINE_LOST_PWM
#define TASK1_LINE_LOST_PWM (TASK1_BASE_PWM_DEFAULT) // 调试参数：可判直角时全白状态保持直行的 PWM
#endif

#ifndef TASK1_LINE_SEARCH_PWM_INNER
#define TASK1_LINE_SEARCH_PWM_INNER (350) // 调试参数：丢线找线内侧轮 PWM，保持正向行驶并向丢线方向靠
#endif

#ifndef TASK1_LINE_SEARCH_PWM_OUTER
#define TASK1_LINE_SEARCH_PWM_OUTER (750) // 调试参数：丢线找线外侧轮 PWM，高于内侧轮用于形成前进弧线
#endif

#ifndef TASK1_LINE_SEARCH_DEFAULT_LEFT
#define TASK1_LINE_SEARCH_DEFAULT_LEFT (-1U) // 调试参数：无法判断丢线方向时默认向左找线
#endif

#ifndef TASK1_CENTER_ERR_THRESHOLD
#define TASK1_CENTER_ERR_THRESHOLD (1) // 调试参数：偏差绝对值不大于该值时认为线在中间
#endif

#ifndef TASK1_PWM_MIN
#define TASK1_PWM_MIN (MOTOR_PWM_MIN) // 调试参数：Task1 PWM 下限，通常保持 MOTOR_PWM_MIN
#endif

#ifndef TASK1_PWM_MAX
#define TASK1_PWM_MAX (MOTOR_PWM_MAX) // 调试参数：Task1 PWM 上限，通常保持 MOTOR_PWM_MAX
#endif

#ifndef TASK1_SENSOR_SAMPLE_MS
#define TASK1_SENSOR_SAMPLE_MS (1U) // 调试参数：Task1 主动读取八路红外周期，1ms 用于提高直角识别速度
#endif

#ifndef TASK1_SENSOR_LOST_RETRY_COUNT
#define TASK1_SENSOR_LOST_RETRY_COUNT (0U) // 调试参数：全白作为直角补充条件，保持 0 可避免额外 I2C 重读延迟
#endif

#ifndef TASK1_CORNER_CONFIRM_COUNT
#define TASK1_CORNER_CONFIRM_COUNT (1U) // 调试参数：直角候选确认次数，用于过滤单次误触发
#endif

#ifndef TASK1_CORNER_CONFIRM_WINDOW_MS
#define TASK1_CORNER_CONFIRM_WINDOW_MS (35U) // 调试参数：直角候选锁存窗口，防止高速经过角点时漏判
#endif

#ifndef TASK1_CORNER_IGNORE_MS
#define TASK1_CORNER_IGNORE_MS (2500U) // 调试参数：转弯完成后直角屏蔽时间，防止重复计数且避免错过下一角
#endif

#ifndef TASK1_CENTER_STRAIGHT_MASK
#define TASK1_CENTER_STRAIGHT_MASK (0x18U) // 调试参数：中心直行通道掩码，0x18 对应 X4/X5
#endif

#ifndef TASK1_CENTER_STRAIGHT_VALUE
#define TASK1_CENTER_STRAIGHT_VALUE (0x18U) // 调试参数：中心命中判定值，等于掩码时取消纠偏
#endif

#ifndef TASK1_PRE_TURN_ENCODER_COUNTS
#define TASK1_PRE_TURN_ENCODER_COUNTS (0L) // 调试参数：直角确认后继续直行的编码器平均脉冲数
#endif

#ifndef TASK1_PRE_TURN_TIMEOUT_MS
#define TASK1_PRE_TURN_TIMEOUT_MS (500U) // 调试参数：编码器前探超时保护，编码器异常时避免卡住
#endif

#ifndef TASK1_PRE_TURN_PWM
#define TASK1_PRE_TURN_PWM (0) // 调试参数：直角确认后前探直行 PWM
#endif

#ifndef TASK1_CCW_YAW_SIGN
#define TASK1_CCW_YAW_SIGN (1) // 调试参数：逆时针 yaw 增大为 +1，减小为 -1
#endif

#ifndef TASK1_TURN_SIGN
#define TASK1_TURN_SIGN (+1) // 调试参数：双轮原地转向方向，实际转向反了只改成 +1
#endif

#ifndef TASK1_TURN_TARGET_DEG
#define TASK1_TURN_TARGET_DEG (75.0f) // 调试参数：每个直角目标角度，正方形固定为 90 度
#endif

#ifndef TASK1_TURN_DONE_DEG
#define TASK1_TURN_DONE_DEG (3.0f) // 调试参数：剩余角小于该值进入到角确认区
#endif

#ifndef TASK1_TURN_DONE_HOLD_MS
#define TASK1_TURN_DONE_HOLD_MS (0U) // 调试参数：到角后刹车保持确认时间，防止惯性过冲和角度采样抖动
#endif

#ifndef TASK1_TURN_KP
#define TASK1_TURN_KP (12.0f) // 调试参数：90 度转向角度环比例系数，越大转向响应越强
#endif

#ifndef TASK1_TURN_PWM_MIN
#define TASK1_TURN_PWM_MIN (180) // 调试参数：角度环最小双轮转向 PWM，防止接近目标时电机停转
#endif

#ifndef TASK1_TURN_PWM_MAX
#define TASK1_TURN_PWM_MAX (550) // 调试参数：角度环最大双轮转向 PWM，降低过冲提高 90 度稳定性
#endif

#ifndef TASK1_TURN_SLOW_ZONE_DEG
#define TASK1_TURN_SLOW_ZONE_DEG (35.0f) // 调试参数：剩余角小于该值进入低速比例转向区
#endif

#ifndef TASK1_TURN_TIMEOUT_MS
#define TASK1_TURN_TIMEOUT_MS (2500U) // 调试参数：转向最大允许时间，只作为兜底保护
#endif

#ifndef TASK1_TURN_SETTLE_MS
#define TASK1_TURN_SETTLE_MS (0U) // 调试参数：转向完成后的额外停稳时间，角度保持已完成时设 0 以快速切回循迹
#endif

#ifndef TASK1_TURN_LINE_STOP_MS
#define TASK1_TURN_LINE_STOP_MS (30U) // 调试参数：转向中识别到黑线后的停止时间，停止结束后再切入循迹
#endif

#ifndef TASK1_TURN_LINE_DETECT_MASK
#define TASK1_TURN_LINE_DETECT_MASK (0x7FU) // 调试参数：转向中用于识别切回循迹黑线的通道掩码，0x7F 对应 X1~X7
#endif

#ifndef TASK1_POST_TURN_SEARCH_PWM
#define TASK1_POST_TURN_SEARCH_PWM (900) // 调试参数：90 度转完后未找到黑线时保持直行找线的 PWM
#endif

#ifndef TASK1_COUNT_TIMEOUT_TURN
#define TASK1_COUNT_TIMEOUT_TURN (1U) // 调试参数：转向超时是否也计入一个角，1 可避免任务卡死
#endif

#ifndef TASK1_CORNERS_PER_LAP
#define TASK1_CORNERS_PER_LAP (4U) // 调试参数：正方形每圈直角数量，固定为 4
#endif

#ifndef TASK1_TARGET_LAPS
#define TASK1_TARGET_LAPS (4U) // 调试参数：目标圈数，当前题目设为 2 圈
#endif

#ifndef TASK1_TARGET_CORNERS
#define TASK1_TARGET_CORNERS (TASK1_CORNERS_PER_LAP * TASK1_TARGET_LAPS) // 调试参数：目标总直角数
#endif

#ifndef TASK1_DEBUG_LOG_PERIOD_MS
#define TASK1_DEBUG_LOG_PERIOD_MS (40U) // 调试参数：LINE/SRCH 状态 UART 日志周期
#endif

#ifndef TASK1_TURN_LOG_PERIOD_MS
#define TASK1_TURN_LOG_PERIOD_MS (100U) // 调试参数：TURN 状态 UART 日志周期
#endif

#ifndef TASK1_OLED_INTERVAL_MS
#define TASK1_OLED_INTERVAL_MS (100U) // 调试参数：OLED 刷新周期
#endif

typedef enum {
    TASK1_STATE_STOP = 0,
    TASK1_STATE_LINE_FOLLOW,
    TASK1_STATE_PRE_TURN_STRAIGHT,
    TASK1_STATE_TURN_90,
    TASK1_STATE_TURN_SETTLE,
    TASK1_STATE_POST_TURN_SEARCH,
    TASK1_STATE_DONE
} Task1State;

void Task1_Init(void);
void Task1_Update(uint32_t now_ms);

#endif /* HARDWARE_TASK1_H */
