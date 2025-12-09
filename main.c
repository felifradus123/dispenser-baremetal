/* Minimal GPIO sweep diagnostic for dispenser hardware.
 * Cycles these outputs high one at a time so you can observe LEDs/relays:
 *  - GPIO2, GPIO3, GPIO18 (valves)
 *  - GPIO8, GPIO9, GPIO19 (leds)
 * Build & flash this, then observe which pins toggle or measure voltages.
 */

#include <stdint.h>
#include <stddef.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define DR_REG_GPIO_BASE 0x60004000UL
#define DR_REG_IO_MUX_BASE 0x60009000UL

#define GPIO_OUT_W1TS_REG   (DR_REG_GPIO_BASE + 0x0008)
#define GPIO_OUT_W1TC_REG   (DR_REG_GPIO_BASE + 0x000C)
#define GPIO_ENABLE_W1TS_REG (DR_REG_GPIO_BASE + 0x0024)

#define IO_MUX_MCU_SEL_MASK (0x7U << 12)
#define IO_MUX_MCU_SEL_GPIO (1U << 12)

/* Pins to test */
static const uint32_t test_pins[] = {2, 3, 18, 8, 9, 19};

static void delay(volatile uint32_t n) { while (n--) __asm__ volatile ("nop"); }

static void set_pin_output(uint32_t gpio) {
    uint32_t reg_addr = DR_REG_IO_MUX_BASE + (gpio * 4U);
    uint32_t v = REG32(reg_addr);
    v &= ~IO_MUX_MCU_SEL_MASK;
    v |= IO_MUX_MCU_SEL_GPIO;
    REG32(reg_addr) = v;
    REG32(GPIO_ENABLE_W1TS_REG) = (1U << gpio);
}

static inline void pin_set(uint32_t gpio) { REG32(GPIO_OUT_W1TS_REG) = (1U << gpio); }
static inline void pin_clear(uint32_t gpio) { REG32(GPIO_OUT_W1TC_REG) = (1U << gpio); }

int main(void) {
    /* Configure test pins as outputs and clear them */
    for (size_t i = 0; i < (sizeof(test_pins)/sizeof(test_pins[0])); ++i) {
        set_pin_output(test_pins[i]);
        pin_clear(test_pins[i]);
    }

    /* Cycle each pin high for ~1s (busy wait) */
    while (1) {
        for (size_t i = 0; i < (sizeof(test_pins)/sizeof(test_pins[0])); ++i) {
            pin_set(test_pins[i]);
            delay(4000000);
            pin_clear(test_pins[i]);
            delay(1000000);
        }
    }

    return 0;
}