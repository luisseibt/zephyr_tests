/* This macro MUST perfectly match the compatible string, replacing ',' and '-' with '_' */
#define DT_DRV_COMPAT pydrofoil_nrf_uart

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>

/* Dynamically grab the base address from the Device Tree */
#define NRF51_UART_BASE   ((uintptr_t)DT_INST_REG_ADDR(0))
#define NRF51_UART_START  (*(volatile uint32_t*)(NRF51_UART_BASE + 0x008))
#define NRF51_UART_ENABLE (*(volatile uint32_t*)(NRF51_UART_BASE + 0x500))
#define NRF51_UART_TXD    (*(volatile uint32_t*)(NRF51_UART_BASE + 0x51C))

static int uart_pydrofoil_poll_in(const struct device *dev, unsigned char *p_char) {
    return -1; /* RX not implemented yet */
}

static void uart_pydrofoil_poll_out(const struct device *dev, unsigned char c) {
    /* Trigger the VCML start task, write the character, and clear it */
    NRF51_UART_START = 1;
    NRF51_UART_TXD = c;
    NRF51_UART_START = 0;
}

static int uart_pydrofoil_init(const struct device *dev) {
    NRF51_UART_ENABLE = 4; /* Value 4 enables TX and RX in the nRF51 */
    return 0;
}

static const struct uart_driver_api uart_pydrofoil_api = {
    .poll_in = uart_pydrofoil_poll_in,
    .poll_out = uart_pydrofoil_poll_out,
};

/* Package it all up into a Zephyr device */
DEVICE_DT_INST_DEFINE(0,
                      uart_pydrofoil_init,
                      NULL, NULL, NULL,
                      PRE_KERNEL_1, CONFIG_SERIAL_INIT_PRIORITY,
                      &uart_pydrofoil_api);