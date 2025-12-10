/*
 * Proyecto: Dispensador de bebidas bare-metal (ESP32-C3)
 * Pinout:
 *  GPIO0  : LDR (sensor de vaso, ADC)
 *  GPIO1  : Potenciómetro (ADC)
 *  GPIO4  : Relé bebida A (salida)
 *  GPIO5  : Relé bebida B (salida)
 *  GPIO9  : Sensor de nivel de tanque (entrada digital)
 *  GPIO10 : LED de selección (salida)
 *  GPIO19 : Botón de dispensado (entrada digital)
 */

#include <stdint.h>

volatile uint32_t *GPIO_OUT_W1TS = (uint32_t *)0x60004008;
volatile uint32_t *GPIO_OUT_W1TC = (uint32_t *)0x6000400C;
volatile uint32_t *GPIO_ENABLE_W1TS = (uint32_t *)0x60004024;
volatile uint32_t *GPIO_ENABLE_W1TC = (uint32_t *)0x60004028;
volatile uint32_t *GPIO_IN = (uint32_t *)0x6000403C;

volatile uint32_t *APB_SARADC_ONETIME_SAMPLE_REG = (uint32_t *)0x60040020;
volatile uint32_t *APB_SARADC_1_DATA_STATUS_REG = (uint32_t *)0x6004002C;

volatile uint32_t *UART_FIFO = (uint32_t *)0x60000078;
volatile uint32_t *UART_STATUS = (uint32_t *)0x6000001C;

int PIN_LDR = 0;
int PIN_POT = 1;
int PIN_RELAY_A = 4;
int PIN_RELAY_B = 5;
int PIN_TANK_SENSOR = 9;
int PIN_LED = 10;
int PIN_BUTTON = 19;

static void uart_init(void) {
    // No se configura baudrate aquí, solo ejemplo de uso mínimo
}
static void uart_putc(char c) {
    while (!(*UART_STATUS & (1 << 1))) {} // Espera espacio en FIFO
    *UART_FIFO = (uint32_t)c;
}
static void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

static void delay(uint32_t n) { while (n--) { __asm__ volatile ("nop"); } }

static void gpio_set_output(int pin) { *GPIO_ENABLE_W1TS = (1U << pin); }
static void gpio_set_input(int pin)  { *GPIO_ENABLE_W1TC = (1U << pin); }
static void gpio_write(int pin, int val) {
    if (val) *GPIO_OUT_W1TS = (1U << pin);
    else     *GPIO_OUT_W1TC = (1U << pin);
}
static int gpio_read(int pin) {
    return ((*GPIO_IN) & (1U << pin)) ? 1 : 0;
}

static uint16_t adc_read(int channel) {
    uint32_t reg = *APB_SARADC_ONETIME_SAMPLE_REG;
    reg &= ~((0xFU << 25) | (0x3U << 23));
    reg |= ((channel & 0xF) << 25);
    reg |= (0U << 23); // Atenuación 0dB
    reg |= (1U << 30); // Habilita one-time ADC1
    *APB_SARADC_ONETIME_SAMPLE_REG = reg;
    *APB_SARADC_ONETIME_SAMPLE_REG |= (1U << 29); // Dispara conversión
    delay(1000);
    uint32_t raw = *APB_SARADC_1_DATA_STATUS_REG & 0xFFF;
    return (uint16_t)raw;
}

int main(void) {
    uart_init();
    gpio_set_output(PIN_RELAY_A);
    gpio_set_output(PIN_RELAY_B);
    gpio_set_output(PIN_LED);
    gpio_set_input(PIN_LDR);
    gpio_set_input(PIN_POT);
    gpio_set_input(PIN_TANK_SENSOR);
    gpio_set_input(PIN_BUTTON);

    int state = 0; // 0=WAIT_CUP, 1=SELECT_DRINK, 2=DISPENSE
    int cupPresent = 0;
    int drink = 0; // 0 = A, 1 = B
    int ldrValue = 0;
    int potValue = 0;
    int buttonPressed = 0;
    int tankOK = 1;

    while (1) {
        ldrValue = adc_read(PIN_LDR);
        potValue = adc_read(PIN_POT);
        buttonPressed = !gpio_read(PIN_BUTTON); // Pull-up
        tankOK = gpio_read(PIN_TANK_SENSOR);
        cupPresent = (ldrValue < 2000);

        switch (state) {
            case 0: // WAIT_CUP
                gpio_write(PIN_RELAY_A, 0);
                gpio_write(PIN_RELAY_B, 0);
                if (cupPresent) state = 1;
                break;
            case 1: // SELECT_DRINK
                if (potValue < 2048) {
                    drink = 0;
                    gpio_write(PIN_LED, 0);
                } else {
                    drink = 1;
                    gpio_write(PIN_LED, 1);
                }
                if (!cupPresent) { state = 0; break; }
                if (buttonPressed && tankOK) state = 2;
                break;
            case 2: // DISPENSE
                if (!cupPresent) {
                    gpio_write(PIN_RELAY_A, 0);
                    gpio_write(PIN_RELAY_B, 0);
                    state = 0;
                    break;
                }
                if (drink == 0) {
                    gpio_write(PIN_RELAY_A, 1);
                    gpio_write(PIN_RELAY_B, 0);
                } else {
                    gpio_write(PIN_RELAY_A, 0);
                    gpio_write(PIN_RELAY_B, 1);
                }
                if (!buttonPressed) {
                    gpio_write(PIN_RELAY_A, 0);
                    gpio_write(PIN_RELAY_B, 0);
                    state = 1;
                }
                break;
        }
        uart_puts("Tanque 1: ");
        uart_puts(tankOK ? "Lleno\n" : "Vacio\n");
        uart_puts("Tanque 2: Vacio\n");
        uart_puts("Tanque 3: Lleno\n");
        delay(10000);
    }
}
