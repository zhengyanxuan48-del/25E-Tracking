#include "platform_time.h"

#include "ti_msp_dl_config.h"

static volatile uint32_t g_platform_ms_ticks;

void SysTick_Handler(void)
{
    g_platform_ms_ticks++;
}

void platform_time_init(void)
{
    DL_TimerA_disableInterrupt(TIMER_SPEED_INST, DL_TIMERA_INTERRUPT_ZERO_EVENT);
    DL_TimerA_clearInterruptStatus(TIMER_SPEED_INST, DL_TIMERA_INTERRUPT_ZERO_EVENT);
    NVIC_DisableIRQ(TIMER_SPEED_INST_INT_IRQN);

    (void) SysTick_Config(CPUCLK_FREQ / 1000U);
    __enable_irq();
}

uint32_t platform_time_ms(void)
{
    return g_platform_ms_ticks;
}

void platform_delay_ms(uint32_t ms)
{
    delay_cycles((CPUCLK_FREQ / 1000U) * ms);
}
