#include "platform_i2c.h"

#include "debug_uart.h"
#include "platform_time.h"
#include "ti_msp_dl_config.h"

#define PLATFORM_I2C_TIMEOUT_CYCLES  (1000000UL)
#define PLATFORM_I2C_START_DELAY_CYCLES (16U)
#define PLATFORM_I2C_LOG_MIN_INTERVAL_MS (1000U)
#define PLATFORM_I2C_RECOVER_PULSE_CYCLES (320U)
#define PLATFORM_I2C_RECOVER_PULSE_COUNT (9U)
#define PLATFORM_I2C_IRQ_MASK \
    (DL_I2C_INTERRUPT_CONTROLLER_NACK | \
        DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST | \
        DL_I2C_INTERRUPT_CONTROLLER_STOP)

static bool g_platform_i2c_initialized;
static uint32_t g_platform_i2c_last_log_ms;
static uint32_t g_platform_i2c_suppressed_logs;

static bool platform_i2c_addr_is_valid(uint8_t addr_7bit)
{
    return (addr_7bit >= 0x03U) && (addr_7bit <= 0x77U);
}

static uint32_t platform_i2c_get_raw_status(void)
{
    return DL_I2C_getRawInterruptStatus(I2C_0_INST, PLATFORM_I2C_IRQ_MASK);
}

static uint8_t platform_i2c_gpio_pin_is_high(GPIO_Regs *port, uint32_t pin)
{
    return (DL_GPIO_readPins(port, pin) != 0U) ? 1U : 0U;
}

static void platform_i2c_restore_i2c_pins(void)
{
    DL_GPIO_initPeripheralInputFunctionFeatures(
        GPIO_I2C_0_IOMUX_SDA, GPIO_I2C_0_IOMUX_SDA_FUNC,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralInputFunctionFeatures(
        GPIO_I2C_0_IOMUX_SCL, GPIO_I2C_0_IOMUX_SCL_FUNC,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(GPIO_I2C_0_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_I2C_0_IOMUX_SCL);
}

static void platform_i2c_config_gpio_inputs(void)
{
    DL_GPIO_initDigitalInputFeatures(GPIO_I2C_0_IOMUX_SDA,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(GPIO_I2C_0_IOMUX_SCL,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_disableOutput(GPIO_I2C_0_SDA_PORT, GPIO_I2C_0_SDA_PIN);
    DL_GPIO_disableOutput(GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);
}

static void platform_i2c_drive_scl_low(void)
{
    DL_GPIO_clearPins(GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);
    DL_GPIO_enableOutput(GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);
}

static void platform_i2c_release_scl(void)
{
    DL_GPIO_disableOutput(GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);
}

static void platform_i2c_drive_sda_low(void)
{
    DL_GPIO_clearPins(GPIO_I2C_0_SDA_PORT, GPIO_I2C_0_SDA_PIN);
    DL_GPIO_enableOutput(GPIO_I2C_0_SDA_PORT, GPIO_I2C_0_SDA_PIN);
}

static void platform_i2c_release_sda(void)
{
    DL_GPIO_disableOutput(GPIO_I2C_0_SDA_PORT, GPIO_I2C_0_SDA_PIN);
}

void platform_i2c_get_bus_state(platform_i2c_bus_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->sda_high = platform_i2c_gpio_pin_is_high(
        GPIO_I2C_0_SDA_PORT, GPIO_I2C_0_SDA_PIN);
    state->scl_high = platform_i2c_gpio_pin_is_high(
        GPIO_I2C_0_SCL_PORT, GPIO_I2C_0_SCL_PIN);
    state->controller_status = DL_I2C_getControllerStatus(I2C_0_INST);
    state->raw_interrupt = platform_i2c_get_raw_status();
}

static void platform_i2c_log_failure(
    const char *phase, const char *reason, platform_i2c_status_t status)
{
    uint32_t controller_status;
    uint32_t raw_interrupt;
    uint32_t tx_count;

    if (phase == NULL) {
        return;
    }

    {
        uint32_t now_ms = platform_time_ms();

        if ((g_platform_i2c_last_log_ms != 0U) &&
            ((uint32_t)(now_ms - g_platform_i2c_last_log_ms) <
                PLATFORM_I2C_LOG_MIN_INTERVAL_MS)) {
            g_platform_i2c_suppressed_logs++;
            return;
        }

        g_platform_i2c_last_log_ms = now_ms;
    }

    controller_status = DL_I2C_getControllerStatus(I2C_0_INST);
    raw_interrupt = platform_i2c_get_raw_status();
    tx_count = DL_I2C_getControllerTXFIFOCounter(I2C_0_INST);

    debug_printf("[I2C] %s %s st=%d msr=0x%08lX raw=0x%08lX txfifo=%lu skip=%lu\r\n",
        phase, reason, (int) status, (unsigned long) controller_status,
        (unsigned long) raw_interrupt, (unsigned long) tx_count,
        (unsigned long) g_platform_i2c_suppressed_logs);
    g_platform_i2c_suppressed_logs = 0U;
}

static void platform_i2c_clear_controller_flags(void)
{
    DL_I2C_clearInterruptStatus(I2C_0_INST, PLATFORM_I2C_IRQ_MASK);
}

static void platform_i2c_clear_controller_state(void)
{
    platform_i2c_clear_controller_flags();
    DL_I2C_resetControllerTransfer(I2C_0_INST);
    platform_i2c_clear_controller_flags();
}

static platform_i2c_status_t platform_i2c_decode_transfer_status(
    uint32_t status, bool check_data_nack)
{
    uint32_t raw_interrupt = platform_i2c_get_raw_status();

    (void) check_data_nack;

    /*
     * ADRACK/DATACK in MSR are set for NACK, but can reflect the previous
     * operation. Raw interrupt flags are cleared before each transaction.
     */
    if ((raw_interrupt & DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST) != 0U) {
        return PLATFORM_I2C_ERROR;
    }

    if ((raw_interrupt & DL_I2C_INTERRUPT_CONTROLLER_NACK) != 0U) {
        return PLATFORM_I2C_ERROR;
    }

    if ((status & DL_I2C_CONTROLLER_STATUS_ARBITRATION_LOST) != 0U) {
        return PLATFORM_I2C_ERROR;
    }

    if (((status & DL_I2C_CONTROLLER_STATUS_ERROR) != 0U) &&
        ((status & (DL_I2C_CONTROLLER_STATUS_ADDR_ACK |
                       DL_I2C_CONTROLLER_STATUS_DATA_ACK |
                       DL_I2C_CONTROLLER_STATUS_ARBITRATION_LOST)) == 0U)) {
        return PLATFORM_I2C_ERROR;
    }

    return PLATFORM_I2C_OK;
}

static void platform_i2c_log_transfer_error(const char *phase)
{
    uint32_t status = DL_I2C_getControllerStatus(I2C_0_INST);
    uint32_t raw_interrupt = platform_i2c_get_raw_status();

    if ((raw_interrupt & DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST) != 0U) {
        platform_i2c_log_failure(phase, "arb-lost", PLATFORM_I2C_ERROR);
    } else if ((raw_interrupt & DL_I2C_INTERRUPT_CONTROLLER_NACK) != 0U) {
        platform_i2c_log_failure(phase, "nack", PLATFORM_I2C_ERROR);
    } else if ((status & DL_I2C_CONTROLLER_STATUS_ARBITRATION_LOST) != 0U) {
        platform_i2c_log_failure(phase, "arb-lost", PLATFORM_I2C_ERROR);
    } else {
        platform_i2c_log_failure(phase, "controller-error",
            PLATFORM_I2C_ERROR);
    }
}

static platform_i2c_status_t platform_i2c_flush_tx_fifo(const char *phase)
{
    uint32_t timeout = PLATFORM_I2C_TIMEOUT_CYCLES;

    DL_I2C_startFlushControllerTXFIFO(I2C_0_INST);

    while (DL_I2C_isControllerTXFIFOEmpty(I2C_0_INST) == false) {
        if (timeout-- == 0U) {
            DL_I2C_stopFlushControllerTXFIFO(I2C_0_INST);
            platform_i2c_log_failure(phase, "fifo-flush-timeout",
                PLATFORM_I2C_TIMEOUT);
            return PLATFORM_I2C_TIMEOUT;
        }
    }

    DL_I2C_stopFlushControllerTXFIFO(I2C_0_INST);
    return PLATFORM_I2C_OK;
}

static platform_i2c_status_t platform_i2c_flush_rx_fifo(const char *phase)
{
    uint32_t timeout = PLATFORM_I2C_TIMEOUT_CYCLES;

    DL_I2C_startFlushControllerRXFIFO(I2C_0_INST);

    while (DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST) == false) {
        if (timeout-- == 0U) {
            DL_I2C_stopFlushControllerRXFIFO(I2C_0_INST);
            platform_i2c_log_failure(phase, "rx-fifo-flush-timeout",
                PLATFORM_I2C_TIMEOUT);
            return PLATFORM_I2C_TIMEOUT;
        }
    }

    DL_I2C_stopFlushControllerRXFIFO(I2C_0_INST);
    return PLATFORM_I2C_OK;
}

static platform_i2c_status_t platform_i2c_wait_until_bus_idle(
    const char *phase)
{
    uint32_t timeout = PLATFORM_I2C_TIMEOUT_CYCLES;

    while (timeout > 0U) {
        uint32_t status = DL_I2C_getControllerStatus(I2C_0_INST);
        platform_i2c_status_t result =
            platform_i2c_decode_transfer_status(status, false);

        if (result != PLATFORM_I2C_OK) {
            platform_i2c_log_transfer_error(phase);
            return result;
        }

        if (((status & DL_I2C_CONTROLLER_STATUS_BUSY) == 0U) &&
            ((status & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) == 0U)) {
            return PLATFORM_I2C_OK;
        }

        timeout--;
    }

    platform_i2c_log_failure(phase, "bus-busy-timeout",
        PLATFORM_I2C_TIMEOUT);
    return PLATFORM_I2C_TIMEOUT;
}

static platform_i2c_status_t platform_i2c_wait_transfer_done(
    bool allow_bus_busy, bool check_data_nack, const char *phase)
{
    uint32_t timeout = PLATFORM_I2C_TIMEOUT_CYCLES;

    while (timeout > 0U) {
        uint32_t status = DL_I2C_getControllerStatus(I2C_0_INST);
        uint32_t raw_interrupt = platform_i2c_get_raw_status();
        platform_i2c_status_t result =
            platform_i2c_decode_transfer_status(status, check_data_nack);

        if (result != PLATFORM_I2C_OK) {
            platform_i2c_log_transfer_error(phase);
            return result;
        }

        if ((allow_bus_busy == false) &&
            ((raw_interrupt & DL_I2C_INTERRUPT_CONTROLLER_STOP) != 0U)) {
            platform_i2c_clear_controller_flags();
            result = platform_i2c_wait_until_bus_idle(phase);
            if (result != PLATFORM_I2C_OK) {
                return result;
            }
            return PLATFORM_I2C_OK;
        }

        if ((status & DL_I2C_CONTROLLER_STATUS_BUSY) == 0U) {
            if (allow_bus_busy ||
                ((status & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) == 0U)) {
                status = DL_I2C_getControllerStatus(I2C_0_INST);
                result = platform_i2c_decode_transfer_status(
                    status, check_data_nack);
                if (result != PLATFORM_I2C_OK) {
                    platform_i2c_log_transfer_error(phase);
                }
                if (allow_bus_busy == false) {
                    platform_i2c_clear_controller_flags();
                }
                return result;
            }
        }

        timeout--;
    }

    platform_i2c_log_failure(phase, "stop-timeout", PLATFORM_I2C_TIMEOUT);
    return PLATFORM_I2C_TIMEOUT;
}

static platform_i2c_status_t platform_i2c_wait_for_tx_space(const char *phase)
{
    uint32_t timeout = PLATFORM_I2C_TIMEOUT_CYCLES;

    while (DL_I2C_isControllerTXFIFOFull(I2C_0_INST) == true) {
        uint32_t status = DL_I2C_getControllerStatus(I2C_0_INST);
        platform_i2c_status_t result =
            platform_i2c_decode_transfer_status(status, true);

        if (result != PLATFORM_I2C_OK) {
            platform_i2c_log_transfer_error(phase);
            return result;
        }

        if (timeout-- == 0U) {
            platform_i2c_log_failure(phase, "tx-fifo-ready-timeout",
                PLATFORM_I2C_TIMEOUT);
            return PLATFORM_I2C_TIMEOUT;
        }
    }

    return PLATFORM_I2C_OK;
}

static platform_i2c_status_t platform_i2c_wait_for_tx_empty(const char *phase)
{
    uint32_t timeout = PLATFORM_I2C_TIMEOUT_CYCLES;

    while (DL_I2C_isControllerTXFIFOEmpty(I2C_0_INST) == false) {
        uint32_t status = DL_I2C_getControllerStatus(I2C_0_INST);
        platform_i2c_status_t result =
            platform_i2c_decode_transfer_status(status, true);

        if (result != PLATFORM_I2C_OK) {
            platform_i2c_log_transfer_error(phase);
            return result;
        }

        if (timeout-- == 0U) {
            platform_i2c_log_failure(phase, "tx-fifo-empty-timeout",
                PLATFORM_I2C_TIMEOUT);
            return PLATFORM_I2C_TIMEOUT;
        }
    }

    return PLATFORM_I2C_OK;
}

static platform_i2c_status_t platform_i2c_prepare_transaction(
    const char *phase)
{
    platform_i2c_status_t status;

    platform_i2c_clear_controller_flags();

    status = platform_i2c_wait_until_bus_idle(phase);
    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
        return status;
    }

    platform_i2c_clear_controller_state();

    status = platform_i2c_flush_tx_fifo(phase);
    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
        return status;
    }

    platform_i2c_clear_controller_flags();
    return PLATFORM_I2C_OK;
}

static platform_i2c_status_t platform_i2c_finish_write_transaction(
    const char *phase)
{
    platform_i2c_status_t status = platform_i2c_wait_for_tx_empty(phase);

    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
        return status;
    }

    status = platform_i2c_wait_transfer_done(false, true, phase);
    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
        return status;
    }

    return PLATFORM_I2C_OK;
}

static platform_i2c_status_t platform_i2c_tx_bytes(
    const uint8_t *data, uint16_t len, const char *phase)
{
    uint16_t index = 0U;

    while (index < len) {
        platform_i2c_status_t status = platform_i2c_wait_for_tx_space(phase);

        if (status != PLATFORM_I2C_OK) {
            return status;
        }

        DL_I2C_transmitControllerData(I2C_0_INST, data[index]);
        index++;
    }

    return PLATFORM_I2C_OK;
}

void platform_i2c_init(void)
{
    /*
     * SYSCFG_DL_init() already initializes I2C_0 on PA0/PA1.
     * Keep this idempotent so shared devices do not reinitialize the bus.
     */
    if (g_platform_i2c_initialized == true) {
        return;
    }

    g_platform_i2c_initialized = true;
}

void platform_i2c_recover_bus(void)
{
    DL_I2C_disableController(I2C_0_INST);
    platform_i2c_clear_controller_flags();
    DL_I2C_resetControllerTransfer(I2C_0_INST);
    (void) platform_i2c_flush_tx_fifo(NULL);
    (void) platform_i2c_flush_rx_fifo(NULL);
    platform_i2c_clear_controller_flags();
    SYSCFG_DL_I2C_0_init();
    platform_i2c_clear_controller_flags();
    delay_cycles(PLATFORM_I2C_START_DELAY_CYCLES);
    (void) platform_i2c_wait_until_bus_idle(NULL);
    g_platform_i2c_initialized = true;
}

void platform_i2c_bus_recover(void)
{
    uint8_t pulse;
    platform_i2c_bus_state_t before;
    platform_i2c_bus_state_t after_gpio;
    platform_i2c_bus_state_t after;

    platform_i2c_get_bus_state(&before);
    debug_printf("[I2C] recover start SDA=%u SCL=%u msr=0x%08lX raw=0x%08lX\r\n",
        (unsigned int) before.sda_high,
        (unsigned int) before.scl_high,
        (unsigned long) before.controller_status,
        (unsigned long) before.raw_interrupt);

    DL_I2C_disableController(I2C_0_INST);
    platform_i2c_clear_controller_flags();
    DL_I2C_resetControllerTransfer(I2C_0_INST);
    platform_i2c_config_gpio_inputs();
    delay_cycles(PLATFORM_I2C_RECOVER_PULSE_CYCLES);

    platform_i2c_get_bus_state(&after_gpio);
    if ((after_gpio.sda_high == 0U) || (after_gpio.scl_high == 0U)) {
        debug_printf("[I2C] bus stuck SDA=%u SCL=%u\r\n",
            (unsigned int) after_gpio.sda_high,
            (unsigned int) after_gpio.scl_high);
    }

    for (pulse = 0U; pulse < PLATFORM_I2C_RECOVER_PULSE_COUNT; pulse++) {
        platform_i2c_release_scl();
        delay_cycles(PLATFORM_I2C_RECOVER_PULSE_CYCLES);
        platform_i2c_drive_scl_low();
        delay_cycles(PLATFORM_I2C_RECOVER_PULSE_CYCLES);
    }

    platform_i2c_drive_sda_low();
    delay_cycles(PLATFORM_I2C_RECOVER_PULSE_CYCLES);
    platform_i2c_release_scl();
    delay_cycles(PLATFORM_I2C_RECOVER_PULSE_CYCLES);
    platform_i2c_release_sda();
    delay_cycles(PLATFORM_I2C_RECOVER_PULSE_CYCLES);

    platform_i2c_restore_i2c_pins();
    platform_i2c_recover_bus();
    platform_i2c_get_bus_state(&after);

    debug_printf("[I2C] recover done SDA=%u SCL=%u msr=0x%08lX raw=0x%08lX\r\n",
        (unsigned int) after.sda_high,
        (unsigned int) after.scl_high,
        (unsigned long) after.controller_status,
        (unsigned long) after.raw_interrupt);
}

platform_i2c_status_t platform_i2c_probe(uint8_t addr_7bit)
{
    if (platform_i2c_addr_is_valid(addr_7bit) == false) {
        return PLATFORM_I2C_BAD_PARAM;
    }

    platform_i2c_init();

    platform_i2c_status_t status = platform_i2c_prepare_transaction(NULL);
    if (status != PLATFORM_I2C_OK) {
        return status;
    }

    DL_I2C_startControllerTransferAdvanced(I2C_0_INST, addr_7bit,
        DL_I2C_CONTROLLER_DIRECTION_TX, 0U, DL_I2C_CONTROLLER_START_ENABLE,
        DL_I2C_CONTROLLER_STOP_ENABLE, DL_I2C_CONTROLLER_ACK_DISABLE);

    delay_cycles(PLATFORM_I2C_START_DELAY_CYCLES);

    status = platform_i2c_wait_transfer_done(false, false, NULL);
    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
    }

    return status;
}

platform_i2c_status_t platform_i2c_probe_write_byte(
    uint8_t addr_7bit, uint8_t value)
{
    return platform_i2c_write(addr_7bit, &value, 1U);
}

platform_i2c_status_t platform_i2c_probe_read(uint8_t addr_7bit)
{
    uint8_t value = 0U;

    return platform_i2c_read(addr_7bit, &value, 1U);
}

platform_i2c_status_t platform_i2c_write(
    uint8_t addr_7bit, const uint8_t *data, uint16_t len)
{
    if ((platform_i2c_addr_is_valid(addr_7bit) == false) ||
        ((data == NULL) && (len > 0U))) {
        return PLATFORM_I2C_BAD_PARAM;
    }

    platform_i2c_init();

    platform_i2c_status_t status = platform_i2c_prepare_transaction("write");
    if (status != PLATFORM_I2C_OK) {
        return status;
    }

    DL_I2C_startControllerTransfer(I2C_0_INST, addr_7bit,
        DL_I2C_CONTROLLER_DIRECTION_TX, len);
    delay_cycles(PLATFORM_I2C_START_DELAY_CYCLES);

    status = platform_i2c_tx_bytes(data, len, "write");
    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
        return status;
    }

    return platform_i2c_finish_write_transaction("write");
}

platform_i2c_status_t platform_i2c_write_bytes(
    uint8_t addr_7bit, const uint8_t *data, uint16_t len)
{
    return platform_i2c_write(addr_7bit, data, len);
}

platform_i2c_status_t platform_i2c_write_control_bytes(
    uint8_t addr_7bit, uint8_t control, const uint8_t *data, uint16_t len)
{
    platform_i2c_status_t status;

    if ((platform_i2c_addr_is_valid(addr_7bit) == false) ||
        ((data == NULL) && (len > 0U))) {
        return PLATFORM_I2C_BAD_PARAM;
    }

    platform_i2c_init();

    status = platform_i2c_prepare_transaction("write-ctrl");
    if (status != PLATFORM_I2C_OK) {
        return status;
    }

    DL_I2C_startControllerTransfer(I2C_0_INST, addr_7bit,
        DL_I2C_CONTROLLER_DIRECTION_TX, (uint16_t)(len + 1U));
    delay_cycles(PLATFORM_I2C_START_DELAY_CYCLES);

    status = platform_i2c_tx_bytes(&control, 1U, "write-ctrl");
    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
        return status;
    }

    status = platform_i2c_tx_bytes(data, len, "write-ctrl");
    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
        return status;
    }

    return platform_i2c_finish_write_transaction("write-ctrl");
}

platform_i2c_status_t platform_i2c_read(
    uint8_t addr_7bit, uint8_t *data, uint16_t len)
{
    uint16_t index = 0U;
    uint32_t timeout = PLATFORM_I2C_TIMEOUT_CYCLES;

    if ((platform_i2c_addr_is_valid(addr_7bit) == false) ||
        ((data == NULL) && (len > 0U))) {
        return PLATFORM_I2C_BAD_PARAM;
    }

    platform_i2c_init();

    platform_i2c_status_t status = platform_i2c_prepare_transaction("read");
    if (status != PLATFORM_I2C_OK) {
        return status;
    }

    DL_I2C_startControllerTransfer(I2C_0_INST, addr_7bit,
        DL_I2C_CONTROLLER_DIRECTION_RX, len);
    delay_cycles(PLATFORM_I2C_START_DELAY_CYCLES);

    while (index < len) {
        uint32_t controller_status = DL_I2C_getControllerStatus(I2C_0_INST);

        if (platform_i2c_decode_transfer_status(controller_status, false) !=
            PLATFORM_I2C_OK) {
            platform_i2c_recover_bus();
            return PLATFORM_I2C_ERROR;
        }

        if (DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST) == false) {
            data[index] = DL_I2C_receiveControllerData(I2C_0_INST);
            index++;
            timeout = PLATFORM_I2C_TIMEOUT_CYCLES;
        } else if (timeout-- == 0U) {
            platform_i2c_recover_bus();
            return PLATFORM_I2C_TIMEOUT;
        }
    }

    status = platform_i2c_wait_transfer_done(false, false, "read");
    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
    }

    return status;
}

platform_i2c_status_t platform_i2c_read_bytes(
    uint8_t addr_7bit, uint8_t *data, uint16_t len)
{
    return platform_i2c_read(addr_7bit, data, len);
}

platform_i2c_status_t platform_i2c_write_reg(
    uint8_t addr_7bit, uint8_t reg, const uint8_t *data, uint16_t len)
{
    if ((platform_i2c_addr_is_valid(addr_7bit) == false) ||
        ((data == NULL) && (len > 0U))) {
        return PLATFORM_I2C_BAD_PARAM;
    }

    platform_i2c_init();

    platform_i2c_status_t status = platform_i2c_prepare_transaction("write-reg");
    if (status != PLATFORM_I2C_OK) {
        return status;
    }

    DL_I2C_startControllerTransfer(I2C_0_INST, addr_7bit,
        DL_I2C_CONTROLLER_DIRECTION_TX, (uint16_t)(len + 1U));
    delay_cycles(PLATFORM_I2C_START_DELAY_CYCLES);

    status = platform_i2c_tx_bytes(&reg, 1U, "write-reg");
    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
        return status;
    }

    status = platform_i2c_tx_bytes(data, len, "write-reg");
    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
        return status;
    }

    return platform_i2c_finish_write_transaction("write-reg");
}

platform_i2c_status_t platform_i2c_read_reg(
    uint8_t addr_7bit, uint8_t reg, uint8_t *data, uint16_t len)
{
    uint16_t index = 0U;
    uint32_t timeout = PLATFORM_I2C_TIMEOUT_CYCLES;

    if ((platform_i2c_addr_is_valid(addr_7bit) == false) ||
        ((data == NULL) && (len > 0U))) {
        return PLATFORM_I2C_BAD_PARAM;
    }

    platform_i2c_init();

    platform_i2c_status_t status = platform_i2c_prepare_transaction("read-reg");
    if (status != PLATFORM_I2C_OK) {
        return status;
    }

    DL_I2C_startControllerTransferAdvanced(I2C_0_INST, addr_7bit,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1U, DL_I2C_CONTROLLER_START_ENABLE,
        DL_I2C_CONTROLLER_STOP_DISABLE, DL_I2C_CONTROLLER_ACK_DISABLE);
    delay_cycles(PLATFORM_I2C_START_DELAY_CYCLES);

    status = platform_i2c_tx_bytes(&reg, 1U, "read-reg-tx");
    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
        return status;
    }

    status = platform_i2c_wait_transfer_done(true, true, "read-reg-tx");
    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
        return status;
    }

    DL_I2C_startControllerTransferAdvanced(I2C_0_INST, addr_7bit,
        DL_I2C_CONTROLLER_DIRECTION_RX, len, DL_I2C_CONTROLLER_START_ENABLE,
        DL_I2C_CONTROLLER_STOP_ENABLE, DL_I2C_CONTROLLER_ACK_DISABLE);
    delay_cycles(PLATFORM_I2C_START_DELAY_CYCLES);

    while (index < len) {
        uint32_t controller_status = DL_I2C_getControllerStatus(I2C_0_INST);

        if (platform_i2c_decode_transfer_status(controller_status, false) !=
            PLATFORM_I2C_OK) {
            platform_i2c_recover_bus();
            return PLATFORM_I2C_ERROR;
        }

        if (DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST) == false) {
            data[index] = DL_I2C_receiveControllerData(I2C_0_INST);
            index++;
            timeout = PLATFORM_I2C_TIMEOUT_CYCLES;
        } else if (timeout-- == 0U) {
            platform_i2c_recover_bus();
            return PLATFORM_I2C_TIMEOUT;
        }
    }

    status = platform_i2c_wait_transfer_done(false, false, "read-reg-rx");
    if (status != PLATFORM_I2C_OK) {
        platform_i2c_recover_bus();
    }

    return status;
}

platform_i2c_status_t platform_i2c_read_regs(
    uint8_t addr_7bit, uint8_t reg, uint8_t *data, uint16_t len)
{
    return platform_i2c_read_reg(addr_7bit, reg, data, len);
}

platform_i2c_status_t platform_i2c_write_u8(
    uint8_t addr_7bit, uint8_t reg, uint8_t value)
{
    return platform_i2c_write_reg(addr_7bit, reg, &value, 1U);
}

platform_i2c_status_t platform_i2c_read_u8(
    uint8_t addr_7bit, uint8_t reg, uint8_t *value)
{
    return platform_i2c_read_reg(addr_7bit, reg, value, 1U);
}
