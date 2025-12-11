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


// ================= GPIO: registros de control =================
// Puntero al registro Write-1-to-set: al escribir un 1 en el bit correspondiente, pone en alto el pin (salida lógica '1')
volatile uint32_t *GPIO_OUT_W1TS = (uint32_t *)0x60004008;   // Dirección física: 0x60004008

// Puntero al registro Write-1-to-clear: al escribir un 1 en el bit correspondiente, pone en bajo el pin (salida lógica '0')
volatile uint32_t *GPIO_OUT_W1TC = (uint32_t *)0x6000400C;   // Dirección física: 0x6000400C

// Puntero al registro para habilitar el pin como salida: al escribir un 1 en el bit correspondiente, el pin se configura como salida
volatile uint32_t *GPIO_ENABLE_W1TS = (uint32_t *)0x60004024; // Dirección física: 0x60004024

// Puntero al registro para habilitar el pin como entrada: al escribir un 1 en el bit correspondiente, el pin se configura como entrada
volatile uint32_t *GPIO_ENABLE_W1TC = (uint32_t *)0x60004028; // Dirección física: 0x60004028

// Puntero al registro de entrada digital: permite leer el estado lógico de todos los pines GPIO
volatile uint32_t *GPIO_IN = (uint32_t *)0x6000403C;         // Dirección física: 0x6000403C

// ================= ADC: registros de conversión =================
// Puntero al registro para configurar y disparar una conversión ADC (analógico a digital)
volatile uint32_t *APB_SARADC_ONETIME_SAMPLE_REG = (uint32_t *)0x60040020; // Dirección física: 0x60040020

// Puntero al registro donde se almacena el resultado de la conversión ADC
volatile uint32_t *APB_SARADC_1_DATA_STATUS_REG = (uint32_t *)0x6004002C; // Dirección física: 0x6004002C

// ================= UART: registros de transmisión =================
// Puntero al registro FIFO de transmisión UART: escribir aquí envía un byte por el puerto serie
volatile uint32_t *UART_FIFO = (uint32_t *)0x60000078;      // Dirección física: 0x60000078

// Puntero al registro de estado UART: permite verificar si hay espacio disponible en el FIFO para transmitir
volatile uint32_t *UART_STATUS = (uint32_t *)0x6000001C;    // Dirección física: 0x6000001C

// ================= Pinout explícito =================
// Cada variable representa el número de GPIO utilizado para cada función del dispensador
uint32_t PIN_LDR = 0U;         // GPIO0: Sensor de vaso (LDR, entrada analógica ADC)
uint32_t PIN_POT = 1U;         // GPIO1: Potenciómetro (entrada analógica ADC)
uint32_t PIN_RELAY_A = 4U;     // GPIO4: Relé bebida A (salida digital)
uint32_t PIN_RELAY_B = 5U;     // GPIO5: Relé bebida B (salida digital)
uint32_t PIN_TANK_SENSOR = 9U; // GPIO9: Sensor de nivel de tanque (entrada digital)
uint32_t PIN_LED = 10U;        // GPIO10: LED de selección de bebida (salida digital)
uint32_t PIN_BUTTON = 19U;     // GPIO19: Botón de dispensado (entrada digital)

static void uart_init(void) {
    // Baudrate y configuración mínima (no implementado)
}
static void uart_putc(char c) {
    while (!(*UART_STATUS & (1U << 1U))) {}
    *UART_FIFO = (uint32_t)c;
}
static void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

static void delay(uint32_t n) { while (n--) { __asm__ volatile ("nop"); } }

static void gpio_set_output(uint32_t pin) { *GPIO_ENABLE_W1TS = (1U << pin); }
static void gpio_set_input(uint32_t pin)  { *GPIO_ENABLE_W1TC = (1U << pin); }
static void gpio_write(uint32_t pin, uint32_t val) {
    if (val) *GPIO_OUT_W1TS = (1U << pin);
    else     *GPIO_OUT_W1TC = (1U << pin);
}
static uint32_t gpio_read(uint32_t pin) {
    return ((*GPIO_IN) & (1U << pin)) ? 1U : 0U;
}

static uint16_t adc_read(uint32_t channel) {
    uint32_t reg = *APB_SARADC_ONETIME_SAMPLE_REG;
    reg &= ~((0xFU << 25U) | (0x3U << 23U));
    reg |= ((channel & 0xFU) << 25U);
    reg |= (0U << 23U); // Atenuación 0dB
    reg |= (1U << 30U); // Habilita one-time ADC1
    *APB_SARADC_ONETIME_SAMPLE_REG = reg;
    *APB_SARADC_ONETIME_SAMPLE_REG |= (1U << 29U); // Dispara conversión
    delay(1000U);
    uint32_t raw = *APB_SARADC_1_DATA_STATUS_REG & 0xFFFU;
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

    uint32_t state = 0U; // 0=WAIT_CUP, 1=SELECT_DRINK, 2=DISPENSE
    uint32_t cupPresent = 0U;
    uint32_t drink = 0U; // 0 = A, 1 = B
    uint16_t ldrValue = 0U;
    uint16_t potValue = 0U;
    uint32_t buttonPressed = 0U;
    uint32_t tankOK = 1U;

    while (1) {
        ldrValue = adc_read(PIN_LDR);
        potValue = adc_read(PIN_POT);
        buttonPressed = !gpio_read(PIN_BUTTON); // Pull-up
        tankOK = gpio_read(PIN_TANK_SENSOR);
        cupPresent = (ldrValue < 2000U) ? 1U : 0U;

        switch (state) {
            case 0U: // WAIT_CUP
                gpio_write(PIN_RELAY_A, 0U);
                gpio_write(PIN_RELAY_B, 0U);
                if (cupPresent) state = 1U;
                break;
            case 1U: // SELECT_DRINK
                // Selección de bebida según el valor del potenciómetro leído por ADC
                // Si el valor es menor a la mitad del rango, selecciona bebida A y apaga el LED
                // Si el valor es mayor o igual a la mitad, selecciona bebida B y enciende el LED
                // La función gpio_write(PIN_LED, val) pone el pin LED en alto (LED encendido) si val=1, o en bajo (LED apagado) si val=0
                if (potValue < 2048U) {
                    drink = 0U;
                    gpio_write(PIN_LED, 0U); // LED apagado (se prende el otro LED si está conectado en configuración opuesta)
                } else {
                    drink = 1U;
                    gpio_write(PIN_LED, 1U); // LED encendido (el otro LED se apaga si está en configuración opuesta)
                }
                if (!cupPresent) { state = 0U; break; }
                if (buttonPressed && tankOK) state = 2U;
                break;
            case 2U: // DISPENSE
                if (!cupPresent) {
                    gpio_write(PIN_RELAY_A, 0U);
                    gpio_write(PIN_RELAY_B, 0U);
                    state = 0U;
                    break;
                }
                if (drink == 0U) {
                    gpio_write(PIN_RELAY_A, 1U);
                    gpio_write(PIN_RELAY_B, 0U);
                } else {
                    gpio_write(PIN_RELAY_A, 0U);
                    gpio_write(PIN_RELAY_B, 1U);
                }
                if (!buttonPressed) {
                    gpio_write(PIN_RELAY_A, 0U);
                    gpio_write(PIN_RELAY_B, 0U);
                    state = 1U;
                }
                break;
        }
        uart_puts("Tanque 1: ");
        uart_puts(tankOK ? "Lleno\n" : "Vacio\n");
        uart_puts("Tanque 2: Vacio\n");
        uart_puts("Tanque 3: Lleno\n");
        delay(10000U);
    }
}
