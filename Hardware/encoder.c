#include "encoder.h"

#include "ti_msp_dl_config.h"

#define ENCODER_LEFT_A_PORT      Left_Encoder_A_PORT
#define ENCODER_LEFT_A_PIN       Left_Encoder_A_PIN_Left_Encoder_A_PIN
#define ENCODER_LEFT_B_PORT      Left_Encoder_B_PORT
#define ENCODER_LEFT_B_PIN       Left_Encoder_B_PIN_Left_Encoder_B_PIN

#define ENCODER_RIGHT_A_PORT     Right_Encoder_A_PORT
#define ENCODER_RIGHT_A_PIN      Right_Encoder_A_PIN_Right_Encoder_A_PIN
#define ENCODER_RIGHT_B_PORT     Right_Encoder_B_PORT
#define ENCODER_RIGHT_B_PIN      Right_Encoder_B_PIN_Right_Encoder_B_PIN

#define ENCODER_GPIO_PORT        GPIOB
#define ENCODER_ALL_PINS         (ENCODER_LEFT_A_PIN | ENCODER_LEFT_B_PIN | \
                                  ENCODER_RIGHT_A_PIN | ENCODER_RIGHT_B_PIN)
#define ENCODER_ALL_EDGES        (DL_GPIO_PIN_0_EDGE_RISE_FALL | \
                                  DL_GPIO_PIN_1_EDGE_RISE_FALL | \
                                  DL_GPIO_PIN_2_EDGE_RISE_FALL | \
                                  DL_GPIO_PIN_3_EDGE_RISE_FALL)

typedef struct {
    GPIO_Regs *a_port;
    uint32_t a_pin;
    GPIO_Regs *b_port;
    uint32_t b_pin;
    int32_t counts_per_rev;
    uint8_t reverse;
} EncoderHw;

static const EncoderHw g_encoder_hw[] = {
    {
        ENCODER_LEFT_A_PORT,
        ENCODER_LEFT_A_PIN,
        ENCODER_LEFT_B_PORT,
        ENCODER_LEFT_B_PIN,
        ENCODER_LEFT_COUNTS_PER_REV,
        ENCODER_LEFT_REVERSE
    },
    {
        ENCODER_RIGHT_A_PORT,
        ENCODER_RIGHT_A_PIN,
        ENCODER_RIGHT_B_PORT,
        ENCODER_RIGHT_B_PIN,
        ENCODER_RIGHT_COUNTS_PER_REV,
        ENCODER_RIGHT_REVERSE
    }
};

static volatile int32_t g_encoder_count[2];
static volatile uint8_t g_encoder_prev_state[2];
static int32_t g_encoder_last_count[2];
static int32_t g_encoder_rpm[2];
static int32_t g_encoder_cps[2];

static const int8_t g_quad_table[16] = {
    0,  1, -1,  0,
   -1,  0,  0,  1,
    1,  0,  0, -1,
    0, -1,  1,  0
};

static const EncoderHw *encoder_get_hw(EncoderId id)
{
    if ((id != ENCODER_LEFT) && (id != ENCODER_RIGHT)) {
        return 0;
    }

    return &g_encoder_hw[(uint32_t) id];
}

static uint8_t encoder_read_state(const EncoderHw *hw)
{
    uint8_t state = 0U;

    if (hw == 0) {
        return 0U;
    }

    if (DL_GPIO_readPins(hw->a_port, hw->a_pin) != 0U) {
        state |= 0x02U;
    }
    if (DL_GPIO_readPins(hw->b_port, hw->b_pin) != 0U) {
        state |= 0x01U;
    }

    return state;
}

static void encoder_decode(EncoderId id)
{
    const EncoderHw *hw = encoder_get_hw(id);
    uint8_t previous;
    uint8_t current;
    int8_t step;

    if (hw == 0) {
        return;
    }

    previous = g_encoder_prev_state[(uint32_t) id] & 0x03U;
    current = encoder_read_state(hw);
    g_encoder_prev_state[(uint32_t) id] = current;

    step = g_quad_table[((previous << 2) | current) & 0x0FU];
    if (hw->reverse != 0U) {
        step = (int8_t) -step;
    }

    g_encoder_count[(uint32_t) id] += step;
}

static int32_t encoder_read_count_atomic(EncoderId id)
{
    uint32_t primask;
    int32_t count;

    if (encoder_get_hw(id) == 0) {
        return 0;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    count = g_encoder_count[(uint32_t) id];
    __set_PRIMASK(primask);

    return count;
}

void Encoder_Init(void)
{
    uint32_t primask;

    DL_GPIO_setLowerPinsPolarity(ENCODER_GPIO_PORT, ENCODER_ALL_EDGES);
    DL_GPIO_clearInterruptStatus(ENCODER_GPIO_PORT, ENCODER_ALL_PINS);

    primask = __get_PRIMASK();
    __disable_irq();
    g_encoder_prev_state[(uint32_t) ENCODER_LEFT] =
        encoder_read_state(&g_encoder_hw[(uint32_t) ENCODER_LEFT]);
    g_encoder_prev_state[(uint32_t) ENCODER_RIGHT] =
        encoder_read_state(&g_encoder_hw[(uint32_t) ENCODER_RIGHT]);
    g_encoder_count[(uint32_t) ENCODER_LEFT] = 0;
    g_encoder_count[(uint32_t) ENCODER_RIGHT] = 0;
    g_encoder_last_count[(uint32_t) ENCODER_LEFT] = 0;
    g_encoder_last_count[(uint32_t) ENCODER_RIGHT] = 0;
    g_encoder_rpm[(uint32_t) ENCODER_LEFT] = 0;
    g_encoder_rpm[(uint32_t) ENCODER_RIGHT] = 0;
    g_encoder_cps[(uint32_t) ENCODER_LEFT] = 0;
    g_encoder_cps[(uint32_t) ENCODER_RIGHT] = 0;
    __set_PRIMASK(primask);

    DL_GPIO_enableInterrupt(ENCODER_GPIO_PORT, ENCODER_ALL_PINS);
    NVIC_EnableIRQ(GPIO_MULTIPLE_GPIOB_INT_IRQN);
}

void Encoder_Reset(EncoderId id)
{
    uint32_t primask;

    if (encoder_get_hw(id) == 0) {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    g_encoder_count[(uint32_t) id] = 0;
    g_encoder_last_count[(uint32_t) id] = 0;
    g_encoder_rpm[(uint32_t) id] = 0;
    g_encoder_cps[(uint32_t) id] = 0;
    g_encoder_prev_state[(uint32_t) id] =
        encoder_read_state(&g_encoder_hw[(uint32_t) id]);
    __set_PRIMASK(primask);
}

int32_t Encoder_GetCount(EncoderId id)
{
    return encoder_read_count_atomic(id);
}

void Encoder_Update(uint32_t dt_ms)
{
    uint32_t id;

    if (dt_ms == 0U) {
        return;
    }

    for (id = 0U; id < 2U; id++) {
        int32_t count = encoder_read_count_atomic((EncoderId) id);
        int32_t delta = count - g_encoder_last_count[id];
        int32_t counts_per_rev = g_encoder_hw[id].counts_per_rev;

        g_encoder_last_count[id] = count;
        g_encoder_cps[id] = (int32_t)(((int64_t) delta * 1000LL) / dt_ms);

        if (counts_per_rev > 0) {
            g_encoder_rpm[id] = (int32_t)(
                ((int64_t) delta * 60000LL) /
                ((int64_t) counts_per_rev * (int64_t) dt_ms));
        } else {
            g_encoder_rpm[id] = 0;
        }
    }
}

int32_t Encoder_GetRPM(EncoderId id)
{
    if (encoder_get_hw(id) == 0) {
        return 0;
    }

    return g_encoder_rpm[(uint32_t) id];
}

int32_t Encoder_GetSpeedCPS(EncoderId id)
{
    if (encoder_get_hw(id) == 0) {
        return 0;
    }

    return g_encoder_cps[(uint32_t) id];
}

void GROUP1_IRQHandler(void)
{
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
        case GPIO_MULTIPLE_GPIOB_INT_IIDX: {
            uint32_t pending = DL_GPIO_getEnabledInterruptStatus(
                ENCODER_GPIO_PORT, ENCODER_ALL_PINS);

            DL_GPIO_clearInterruptStatus(ENCODER_GPIO_PORT, pending);

            if ((pending & (ENCODER_LEFT_A_PIN | ENCODER_LEFT_B_PIN)) != 0U) {
                encoder_decode(ENCODER_LEFT);
            }
            if ((pending & (ENCODER_RIGHT_A_PIN | ENCODER_RIGHT_B_PIN)) != 0U) {
                encoder_decode(ENCODER_RIGHT);
            }
            break;
        }
        default:
            break;
    }
}

int16_t Get_EncoderA_Count(void)
{
    return (int16_t) Encoder_GetCount(ENCODER_LEFT);
}

int16_t Get_EncoderB_Count(void)
{
    return (int16_t) Encoder_GetCount(ENCODER_RIGHT);
}
