#ifndef HARDWARE_TASK1_H
#define HARDWARE_TASK1_H

#include "motor.h"

#include <stdint.h>

#ifndef APP_MODE_TASK1
#define APP_MODE_TASK1 (1U) // 使能 task1 主程序模式
#endif

// ===== Task1 tunable parameters =====

#ifndef TASK1_OLED_CODE
#define TASK1_OLED_CODE (35U) // 范围:0~999；OLED 显示代码号，每次修改代码后加 1
#endif

#ifndef TASK1_BASE_PWM_DEFAULT
#define TASK1_BASE_PWM_DEFAULT (850) // 范围:250~550；正常循迹基础 PWM
#endif

#ifndef TASK1_STEER_KP
#define TASK1_STEER_KP (25.0f) // 范围:15.0~70.0；循迹比例，纠偏不够就加大，蛇形就减小
#endif

#ifndef TASK1_STEER_KD
#define TASK1_STEER_KD (12.0f) // 范围:0.0~25.0；循迹微分，抑制摆动，过大容易抖
#endif

#ifndef TASK1_STEER_SIGN
#define TASK1_STEER_SIGN (+1) // 可选:+1/-1；循迹修正方向，越纠越偏就反过来
#endif

#ifndef TASK1_CORRECTION_MAX
#define TASK1_CORRECTION_MAX (420) // 范围:80~300；循迹修正最大 PWM 差值
#endif

#ifndef TASK1_LINE_LOST_PWM
#define TASK1_LINE_LOST_PWM (TASK1_BASE_PWM_DEFAULT) // 范围:建议等于基础 PWM；无黑线时保持直行，不搜索、不停车
#endif

#ifndef TASK1_LINE_LOST_TIMEOUT_MS
#define TASK1_LINE_LOST_TIMEOUT_MS (0U) // 保留参数但当前不用；无黑线时不因丢线超时停车
#endif

#ifndef TASK1_LINE_SEARCH_PWM_INNER
#define TASK1_LINE_SEARCH_PWM_INNER (0) // 范围:0~300；直角屏蔽期丢线找线内侧轮 PWM，0 表示单轮找线
#endif

#ifndef TASK1_LINE_SEARCH_PWM_OUTER
#define TASK1_LINE_SEARCH_PWM_OUTER (520) // 范围:300~700；直角屏蔽期丢线找线外侧轮 PWM
#endif

#ifndef TASK1_LINE_SEARCH_DEFAULT_LEFT
#define TASK1_LINE_SEARCH_DEFAULT_LEFT (1U) // 可选:0/1；无法判断丢线方向时默认向左找线
#endif

#ifndef TASK1_CENTER_ERR_THRESHOLD
#define TASK1_CENTER_ERR_THRESHOLD (1) // 范围:0~3；误差绝对值不大于该值时认为线在中间
#endif

#ifndef TASK1_PWM_MIN
#define TASK1_PWM_MIN (MOTOR_PWM_MIN) // 范围:通常保持 MOTOR_PWM_MIN；task1 PWM 下限
#endif

#ifndef TASK1_PWM_MAX
#define TASK1_PWM_MAX (MOTOR_PWM_MAX) // 范围:通常保持 MOTOR_PWM_MAX；task1 PWM 上限
#endif

#ifndef TASK1_SENSOR_SAMPLE_MS
#define TASK1_SENSOR_SAMPLE_MS (1U) // 范围:1~10ms；task1 主动刷新循迹模块周期，1ms 约 1000Hz，提升直角响应
#endif

#ifndef TASK1_SENSOR_LOST_RETRY_COUNT
#define TASK1_SENSOR_LOST_RETRY_COUNT (0U) // 范围:0~2；全白作为直角触发时设 0，避免重读滤掉瞬时全白
#endif

#ifndef TASK1_CORNER_MASK
#define TASK1_CORNER_MASK (0x00U) // 保留参数；当前直角判断改为连续全白，不再按 X1~X5 黑线数量判断
#endif

#ifndef TASK1_CORNER_VALUE
#define TASK1_CORNER_VALUE (0x00U) // 保留参数；当前直角判断不用
#endif

#ifndef TASK1_CORNER_MIN_ACTIVE_COUNT
#define TASK1_CORNER_MIN_ACTIVE_COUNT (0U) // 保留参数；当前直角判断不用
#endif

#ifndef TASK1_CORNER_CONFIRM_COUNT
#define TASK1_CORNER_CONFIRM_COUNT (1U) // 范围:1~4；连续识别到全白/无黑线的次数，达到后认为直角
#endif

#ifndef TASK1_CORNER_IGNORE_MS
#define TASK1_CORNER_IGNORE_MS (3000U) // 范围:300~1200ms；转弯完成后忽略直角检测时间，太大会错过下一直角
#endif

#ifndef TASK1_CENTER_STRAIGHT_MASK
#define TASK1_CENTER_STRAIGHT_MASK (0x18U) // 推荐:0x18(X4/X5)；中心直行通道
#endif

#ifndef TASK1_CENTER_STRAIGHT_VALUE
#define TASK1_CENTER_STRAIGHT_VALUE (0x18U) // 推荐:等于 TASK1_CENTER_STRAIGHT_MASK；中心命中时取消纠偏
#endif

#ifndef TASK1_PRE_TURN_STRAIGHT_MS
#define TASK1_PRE_TURN_STRAIGHT_MS (0U) // 范围:0~1200ms；检测到直角后继续直行时间
#endif

#ifndef TASK1_PRE_TURN_PWM
#define TASK1_PRE_TURN_PWM (700) // 范围:220~500；直角后直行阶段 PWM
#endif

#ifndef TASK1_CCW_YAW_SIGN
#define TASK1_CCW_YAW_SIGN (1) // 可选:+1/-1；逆时针 yaw 增大为 +1，减小为 -1
#endif

#ifndef TASK1_TURN_SIGN
#define TASK1_TURN_SIGN (-1) // 可选:+1/-1；原地差速转向方向，TURN 时左右轮一正一反，车实际转向反了就改成 -1
#endif

#ifndef TASK1_TURN_TARGET_DEG
#define TASK1_TURN_TARGET_DEG (90.0f) // 范围:80~105deg；以 0 度为参照的每个直角步进，90 表示目标 90/180/270/360
#endif

#ifndef TASK1_TURN_DONE_DEG
#define TASK1_TURN_DONE_DEG (6.0f) // 范围:4~12deg；剩余角小于该值认为转向完成
#endif

#ifndef TASK1_TURN_SLOW_ZONE_DEG
#define TASK1_TURN_SLOW_ZONE_DEG (25.0f) // 范围:15~45deg；剩余多少度进入慢速转向
#endif

#ifndef TASK1_TURN_PWM_FAST
#define TASK1_TURN_PWM_FAST (700) // 范围:450~800；大角度转向 PWM
#endif

#ifndef TASK1_TURN_PWM_SLOW
#define TASK1_TURN_PWM_SLOW (320) // 范围:300~600；接近目标角时转向 PWM
#endif

#ifndef TASK1_TURN_TIMEOUT_MS
#define TASK1_TURN_TIMEOUT_MS (3000U) // 范围:1500~4000ms；转向最大允许时间，只作兜底
#endif

#ifndef TASK1_TURN_LINE_EXIT_MASK
#define TASK1_TURN_LINE_EXIT_MASK (0x38U) // 固定:X4/X5/X6；转向中需 X4+X5 或 X5+X6 同时黑线才回循迹
#endif

#ifndef TASK1_TURN_LINE_EXIT_MIN_MS
#define TASK1_TURN_LINE_EXIT_MIN_MS (200U) // 范围:0~500ms；转向中识别到 X4/X5/X6 直接回循迹，0 表示立即判断
#endif

#ifndef TASK1_TURN_SETTLE_MS
#define TASK1_TURN_SETTLE_MS (20U) // 范围:0~150ms；转向完成后的短暂停稳时间
#endif

#ifndef TASK1_COUNT_TIMEOUT_TURN
#define TASK1_COUNT_TIMEOUT_TURN (1U) // 可选:0/1；转向超时是否也计入一个角，1 避免任务卡死
#endif

#ifndef TASK1_CORNERS_PER_LAP
#define TASK1_CORNERS_PER_LAP (4U) // 范围:通常固定 4；正方形每圈 4 个角
#endif

#ifndef TASK1_TARGET_LAPS
#define TASK1_TARGET_LAPS (2U) // 范围:1~2；目标圈数，调试单圈可设 1
#endif

#ifndef TASK1_TARGET_CORNERS
#define TASK1_TARGET_CORNERS (TASK1_CORNERS_PER_LAP * TASK1_TARGET_LAPS) // 自动计算；目标总直角数
#endif

#ifndef TASK1_DEBUG_LOG_PERIOD_MS
#define TASK1_DEBUG_LOG_PERIOD_MS (50U) // 范围:100~500ms；UART 调试日志周期
#endif

#ifndef TASK1_TURN_LOG_PERIOD_MS
#define TASK1_TURN_LOG_PERIOD_MS (100U) // 范围:50~200ms；TURN 状态日志周期
#endif

#ifndef TASK1_OLED_INTERVAL_MS
#define TASK1_OLED_INTERVAL_MS (100U) // 范围:50~200ms；OLED 刷新周期
#endif

typedef enum {
    TASK1_STATE_STOP = 0,
    TASK1_STATE_LINE_FOLLOW,
    TASK1_STATE_PRE_TURN_STRAIGHT,
    TASK1_STATE_TURN_90,
    TASK1_STATE_TURN_SETTLE,
    TASK1_STATE_DONE,
    TASK1_STATE_FAULT
} Task1State;

void Task1_Init(void);
void Task1_Update(uint32_t now_ms);
void Task1_SetBaseSpeed(int16_t pwm);
uint8_t Task1_IsDone(void);

#endif /* HARDWARE_TASK1_H */
