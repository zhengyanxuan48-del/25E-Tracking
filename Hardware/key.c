#include "key.h"

#include "ti_msp_dl_config.h"

#define KEY_DEBOUNCE_MS       (30U)
#define KEY_ACTIVE_LEVEL_HIGH (1U)

#if !defined(KEY1_PORT) || !defined(KEY1_PIN_B6_PIN)
#error "KEY1 must be configured in SysConfig as GPIO input PB6 with pin name PIN_B6."
#endif

#if !defined(KEY2_PORT) || !defined(KEY2_PIN_B7_PIN)
#error "KEY2 must be configured in SysConfig as GPIO input PB7 with pin name PIN_B7."
#endif

typedef struct {
    GPIO_Regs *port;
    uint32_t pin;
} KeyHw;

typedef struct {
    uint8_t last_raw_pressed;
    uint8_t stable_pressed;
    uint8_t press_event;
    uint32_t last_change_ms;
} KeyState;

static const KeyHw g_key_hw[KEY_ID_COUNT] = {
    {KEY1_PORT, KEY1_PIN_B6_PIN},
    {KEY2_PORT, KEY2_PIN_B7_PIN}
};

static KeyState g_key_state[KEY_ID_COUNT];

static uint8_t key_id_valid(KeyId id)
{
    return (((uint32_t) id) < ((uint32_t) KEY_ID_COUNT)) ? 1U : 0U;
}

static uint8_t key_read_pressed(KeyId id)
{
    uint8_t high_level;

    if (key_id_valid(id) == 0U) {
        return 0U;
    }

    high_level =
        (DL_GPIO_readPins(g_key_hw[(uint32_t) id].port,
             g_key_hw[(uint32_t) id].pin) != 0U) ? 1U : 0U;

#if KEY_ACTIVE_LEVEL_HIGH
    return high_level;
#else
    return (high_level == 0U) ? 1U : 0U;
#endif
}

void Key_Init(void)
{
    uint32_t id;

    for (id = 0U; id < ((uint32_t) KEY_ID_COUNT); id++) {
        uint8_t pressed = key_read_pressed((KeyId) id);

        g_key_state[id].last_raw_pressed = pressed;
        g_key_state[id].stable_pressed = pressed;
        g_key_state[id].press_event = 0U;
        g_key_state[id].last_change_ms = 0U;
    }
}

void Key_Update(uint32_t now_ms)
{
    uint32_t id;

    for (id = 0U; id < ((uint32_t) KEY_ID_COUNT); id++) {
        uint8_t raw_pressed = key_read_pressed((KeyId) id);
        KeyState *state = &g_key_state[id];

        if (raw_pressed != state->last_raw_pressed) {
            state->last_raw_pressed = raw_pressed;
            state->last_change_ms = now_ms;
        }

        if (((uint32_t)(now_ms - state->last_change_ms) >=
                KEY_DEBOUNCE_MS) &&
            (raw_pressed != state->stable_pressed)) {
            state->stable_pressed = raw_pressed;
            if (state->stable_pressed != 0U) {
                state->press_event = 1U;
            }
        }
    }
}

uint8_t Key_WasPressed(KeyId id)
{
    uint32_t index = (uint32_t) id;
    uint8_t was_pressed;

    if (key_id_valid(id) == 0U) {
        return 0U;
    }

    was_pressed = g_key_state[index].press_event;
    g_key_state[index].press_event = 0U;

    return was_pressed;
}
