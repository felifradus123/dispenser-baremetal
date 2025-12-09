/*
 * Proyecto: Dispensador de bebidas bare-metal (ESP32-C3)
 *
 * Pinout del proyecto:
 *  - GPIO0  : LDR (sensor de vaso)
 *  - GPIO1  : Potenciómetro (selección de bebida)
 *  - GPIO4  : Relé bebida A
 *  - GPIO5  : Relé bebida B
 *  - GPIO9  : Sensor de nivel de tanque
 *  - GPIO10 : LED de selección
 *  - GPIO19 : Botón de dispensado
 *
 * Resumen:
 *  - Controla relés y LED según sensores y selección de usuario.
 *  - Usa máquina de estados para el flujo lógico.
 *  - Simula lecturas de ADC y GPIO (en hardware serían registros).
 *  - UART mínima para monitoreo (simulada).
 *  - Todo el código es bare-metal, sin librerías ni defines.
 */

#include <stdint.h>

// --- Pinout explícito ---
int PIN_LDR = 0;      // LDR para detectar vaso
int PIN_POT = 1;      // Potenciómetro para seleccionar bebida
int PIN_RELAY_A = 4;  // Relé bebida A
int PIN_RELAY_B = 5;  // Relé bebida B
int PIN_TANK_SENSOR = 9; // Sensor de nivel de tanque
int PIN_LED = 10;     // LED de selección
int PIN_BUTTON = 19;  // Botón para dispensar

// --- UART mínimo (simulado, sin implementación real) ---
static void uart_init(void) {}
static void uart_putc(char c) { (void)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

// --- Delay simple ---
static void delay(uint32_t n) { while (n--) { __asm__ volatile ("nop"); } }

int main(void) {
    uart_init();

    // Inicialización de pines (simulada, en hardware sería acceso a registros)
    // Ejemplo: configurar como entrada/salida
    // gpio_set_output(PIN_RELAY_A);
    // gpio_set_output(PIN_RELAY_B);
    // gpio_set_output(PIN_LED);
    // gpio_set_input(PIN_LDR);
    // gpio_set_input(PIN_POT);
    // gpio_set_input(PIN_TANK_SENSOR);
    // gpio_set_input(PIN_BUTTON);

    // Variables de estado
    int state = 0; // 0=WAIT_CUP, 1=SELECT_DRINK, 2=DISPENSE
    int cupPresent = 0;
    int drink = 0; // 0 = A, 1 = B
    int ldrValue = 0;
    int potValue = 0;
    int buttonPressed = 0;
    int tankOK = 1;

    while (1) {
        // Lectura de sensores (simulada, en hardware sería acceso a registros)
        ldrValue = 1500;      // Simula vaso presente (ADC GPIO0)
        potValue = 3000;      // Simula potenciómetro en bebida B (ADC GPIO1)
        buttonPressed = 1;    // Simula botón presionado (GPIO19)
        tankOK = 1;           // Simula tanque lleno (GPIO9)

        cupPresent = (ldrValue < 2000);

        switch (state) {
            case 0: // WAIT_CUP
                // Apagar relés
                // gpio_write(PIN_RELAY_A, 0);
                // gpio_write(PIN_RELAY_B, 0);
                if (cupPresent) {
                    state = 1; // SELECT_DRINK
                }
                break;
            case 1: // SELECT_DRINK
                if (potValue < 2048) {
                    drink = 0; // Bebida A
                    // gpio_write(PIN_LED, 0); // LED off
                } else {
                    drink = 1; // Bebida B
                    // gpio_write(PIN_LED, 1); // LED on
                }
                if (!cupPresent) {
                    state = 0; // WAIT_CUP
                    break;
                }
                if (buttonPressed && tankOK) {
                    state = 2; // DISPENSE
                }
                break;
            case 2: // DISPENSE
                if (!cupPresent) {
                    // Apagar relés
                    // gpio_write(PIN_RELAY_A, 0);
                    // gpio_write(PIN_RELAY_B, 0);
                    state = 0; // WAIT_CUP
                    break;
                }
                if (drink == 0) {
                    // gpio_write(PIN_RELAY_A, 1); // Relé A on
                    // gpio_write(PIN_RELAY_B, 0); // Relé B off
                } else {
                    // gpio_write(PIN_RELAY_A, 0); // Relé A off
                    // gpio_write(PIN_RELAY_B, 1); // Relé B on
                }
                if (!buttonPressed) {
                    // Apagar relés
                    // gpio_write(PIN_RELAY_A, 0);
                    // gpio_write(PIN_RELAY_B, 0);
                    state = 1; // SELECT_DRINK
                }
                break;
        }

        uart_puts("Tanque 1: ");
        if (tankOK) {
            uart_puts("Lleno\n");
        } else {
            uart_puts("Vacio\n");
        }
        uart_puts("Tanque 2: Vacio\n");
        uart_puts("Tanque 3: Lleno\n");

        delay(10000); // Simula delay(10)
    }
}
