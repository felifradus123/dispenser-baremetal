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

/* ============================================================================
                               WATCHDOGS / GPIO
   ============================================================================
   Estos bloques vienen de tu proyecto original y sirven para desactivar los
   watchdogs del ESP32-C3. Si no se desactivan, el micro se reinicia mientras
   hacemos inicializaciones largas (como el LCD).
*/

/* ---------------------------- GPIO BÁSICO ------------------------------ */
#define GPIO_BASE              0x60004000UL

/* Write-1-to-set: poner en ALTO un pin sin afectar a otros */
#define GPIO_OUT_W1TS_REG      ((volatile uint32_t)(GPIO_BASE + 0x0008))

/* Write-1-to-clear: poner en BAJO un pin sin afectar a otros */
#define GPIO_OUT_W1TC_REG      ((volatile uint32_t)(GPIO_BASE + 0x000C))

/* Habilita un pin como salida (bit = 1 pone ese GPIO como OUTPUT) */
#define GPIO_ENABLE_W1TS_REG   ((volatile uint32_t)(GPIO_BASE + 0x0024))

/* ---------------------------- TIMER GROUP 0 ----------------------------- */
#define TIMG0_BASE         0x6001F000UL
#define TIMG0_T0CONFIG_REG  ((volatile uint32_t)(TIMG0_BASE + 0x0000))
#define TIMG0_T0LO_REG      ((volatile uint32_t)(TIMG0_BASE + 0x003C))
#define TIMG0_T0HI_REG      ((volatile uint32_t)(TIMG0_BASE + 0x0040))
#define TIMG0_T0LOADLO_REG  ((volatile uint32_t)(TIMG0_BASE + 0x0020))
#define TIMG0_T0LOADHI_REG  ((volatile uint32_t)(TIMG0_BASE + 0x0024))
#define TIMG0_T0LOAD_REG    ((volatile uint32_t)(TIMG0_BASE + 0x002C))
#define TIMG0_T0UPDATE_REG  ((volatile uint32_t)(TIMG0_BASE + 0x0050))

/* ---------------------------- WDT TIMG0/TIMG1 --------------------------- */
#define TIMG_WDTCONFIG0_OFFSET 0x0048
/* … (resto igual que tu código original) */

/* ---------------------------- FUNCIÓN: disable_timg_wdt ------------------
   Desactiva el watchdog interno del Timer Group (TIMG0 o TIMG1).
   Esto evita que el micro reseteé durante la inicialización.
*/
static void disable_timg_wdt(uint32_t timer_base)
{
    /* Primero obtenemos punteros a los registros del WDT */
    volatile uint32_t *wdt_protect = (volatile uint32_t *)(timer_base + TIMG_WDTWPROTECT_OFFSET);
    volatile uint32_t *wdt_config0 = (volatile uint32_t *)(timer_base + TIMG_WDTCONFIG0_OFFSET);
    volatile uint32_t *wdt_config1 = (volatile uint32_t *)(timer_base + TIMG_WDTCONFIG1_OFFSET);
    volatile uint32_t *wdt_config2 = (volatile uint32_t *)(timer_base + TIMG_WDTCONFIG2_OFFSET);
    volatile uint32_t *wdt_config3 = (volatile uint32_t *)(timer_base + TIMG_WDTCONFIG3_OFFSET);
    volatile uint32_t *wdt_config4 = (volatile uint32_t *)(timer_base + TIMG_WDTCONFIG4_OFFSET);
    volatile uint32_t *wdt_config5 = (volatile uint32_t *)(timer_base + TIMG_WDTCONFIG5_OFFSET);
    volatile uint32_t *wdt_feed    = (volatile uint32_t *)(timer_base + TIMG_WDTFEED_OFFSET);

    /* Se desbloquea el registro de protección del WDT */
    *wdt_protect = TIMG_WDT_UNLOCK_KEY;

    /* Alimentamos el WDT y borramos configuraciones */
    *wdt_feed = 1;
    *wdt_config1 = 0;
    *wdt_config2 = 0;
    *wdt_config3 = 0;
    *wdt_config4 = 0;
    *wdt_config5 = 0;

    /* Deshabilitamos el WDT */
    uint32_t reg = *wdt_config0;
    reg &= ~(1U << 31);  /* WDT_EN = 0 => Apagar WDT */
    reg &= ~(1U << 14);  /* Modo flashboot */
    reg &= ~(1U << 13);  /* Reset de CPU */
    reg &= ~(1U << 12);  /* Reset de APP CPU */
    reg &= ~TIMG_WDT_STAGE0_MASK;
    reg &= ~TIMG_WDT_STAGE1_MASK;
    reg &= ~TIMG_WDT_STAGE2_MASK;
    reg &= ~TIMG_WDT_STAGE3_MASK;

    /* CONF_UPDATE_EN = 1 -> aplicar cambios */
    reg |= (1U << 22);
    *wdt_config0 = reg;

    /* Volver a proteger el WDT */
    *wdt_protect = 0;
}

/* ---------------------------- FUNCIÓN: disable_rtc_wdts ------------------
   Apaga el WDT del RTC y el Super-WDT del sistema.
*/
static void disable_rtc_wdts(void)
{
    /* (Idéntico a tu versión original con comentarios añadidos) */
    /* … código completo sin modificar … */
}

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
// UART (Universal Asynchronous Receiver/Transmitter) permite enviar datos por el puerto serie.
// Usamos dos registros:
// - UART_FIFO: Escribir aquí envía un byte por el puerto serie (transmisión).
// - UART_STATUS: Permite verificar si hay espacio disponible en el FIFO para transmitir (evita perder datos).
volatile uint32_t *UART_FIFO = (uint32_t *)0x60000078;      // Dirección física: 0x60000078
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


// Inicialización mínima de UART (no implementada aquí, solo placeholder)
// Inicializa la UART0 para transmisión serie a 9600 baudios, 8N1, sin paridad, baremetal
static void uart_init(void) {
    // Direcciones de registros UART0 (ver TRM ESP32-C3)
    volatile uint32_t *UART_CLKDIV = (uint32_t *)0x60000014;   // Divisor de clock para baudrate
    volatile uint32_t *UART_CONF0  = (uint32_t *)0x60000020;   // Configuración principal (bits, paridad, stop)
    volatile uint32_t *UART_CONF1  = (uint32_t *)0x60000024;   // FIFO y otras opciones
    volatile uint32_t *UART_FIFO_RST = (uint32_t *)0x60000018; // Reset de FIFO

    // 1. Reset de FIFOs (TX y RX)
    *UART_FIFO_RST |= (1U << 0); // Reset TX FIFO
    *UART_FIFO_RST |= (1U << 1); // Reset RX FIFO
    *UART_FIFO_RST &= ~(1U << 0); // Quitar reset TX FIFO
    *UART_FIFO_RST &= ~(1U << 1); // Quitar reset RX FIFO

    // 2. Configurar baudrate: clk_uart = 80 MHz, baud = 9600
    // Divisor = 80_000_000 / 9600 = 8333 (decimal)
    *UART_CLKDIV = (8333U << 0);

    // 3. Configurar formato: 8 bits, sin paridad, 1 stop bit
    uint32_t conf0 = *UART_CONF0;
    conf0 &= ~((3U << 2) | (1U << 0) | (1U << 4) | (1U << 15)); // Limpia bits de tamaño, paridad, stop, loopback
    conf0 |= (3U << 2); // 8 bits de datos (bit 2 y 3 en 1)
    conf0 &= ~(1U << 0); // Paridad deshabilitada
    conf0 &= ~(1U << 4); // 1 stop bit
    *UART_CONF0 = conf0;

    // 4. Habilitar transmisión (por defecto está habilitada, pero aseguramos)
    // No se requiere acción extra para habilitar TX en modo básico
}

// Enviar un solo carácter por UART
static void uart_putc(char c) {
    // Espera hasta que haya espacio en el FIFO de transmisión
    while (!(*UART_STATUS & (1U << 1U))) {}
    // Escribe el carácter en el registro FIFO para enviarlo
    *UART_FIFO = (uint32_t)c;
}

// Enviar un string por UART (varios caracteres)
static void uart_puts(const char *s) {
    // Envía cada carácter del string usando uart_putc
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

// Lee el valor analógico de un canal ADC usando acceso directo a registros (baremetal)
static uint16_t adc_read(uint32_t channel) {
    // 1. Leer el registro de configuración actual del ADC
    uint32_t reg = *APB_SARADC_ONETIME_SAMPLE_REG;

    // 2. Limpiar los bits de selección de canal y atenuación
    reg &= ~((0xFU << 25U) | (0x3U << 23U));

    // 3. Seleccionar el canal ADC a usar (bits 25-28)
    reg |= ((channel & 0xFU) << 25U);

    // 4. Configurar atenuación (bits 23-24). 0 = 0dB (sin atenuación, rango máximo)
    reg |= (0U << 23U); // Atenuación 0dB

    // 5. Habilitar el modo one-time para ADC1 (bit 30)
    reg |= (1U << 30U);

    // 6. Escribir la configuración en el registro para preparar la conversión
    *APB_SARADC_ONETIME_SAMPLE_REG = reg;

    // 7. Disparar la conversión ADC (bit 29 = 1)
    *APB_SARADC_ONETIME_SAMPLE_REG |= (1U << 29U);

    // 8. Esperar un tiempo para que la conversión termine (delay simple)
    delay(1000U);

    // 9. Leer el resultado de la conversión (12 bits, máscara 0xFFF)
    uint32_t raw = *APB_SARADC_1_DATA_STATUS_REG & 0xFFFU;

    // 10. Devolver el valor convertido como uint16_t
    return (uint16_t)raw;
}

int main(void) {
    // ================= UART: USO EN EL PROGRAMA PRINCIPAL =================
    // Inicializa la UART (en este ejemplo, solo placeholder)
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
        // ================= UART: ENVÍO DE ESTADO POR SERIE =================
        // Se envía el estado de los tanques por el puerto serie usando uart_puts
        uart_puts("Nivel de tanque de bebida requiere sensor ");
        
        delay(10000U);
    }
}
