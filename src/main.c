/*
 * main.c - Lógica de dispensador, versión bare-metal, sin defines ni librerías estándar.
 * UART implementada a mano, sin printf ni Arduino.
 */

#include <stdint.h>

// --- UART mínimo (simulado, sin implementación real) ---
static void uart_init(void) {}
static void uart_putc(char c) { (void)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

// --- Delay simple ---
static void delay(uint32_t n) { while (n--) { __asm__ volatile ("nop"); } }

int main(void) {
    uart_init();

    // Variables de estado
    int state = 0; // 0=WAIT_CUP, 1=SELECT_DRINK, 2=DISPENSE
    int cupPresent = 0;
    int drink = 0; // 0 = A, 1 = B
    int ldrValue = 0;
    int potValue = 0;
    int buttonPressed = 0;
    int tankOK = 1;

    while (1) {
        // Simulación de lecturas (en uso real serían registros de hardware)
        ldrValue = 1500;      // Simula vaso presente
        potValue = 3000;      // Simula potenciómetro en bebida B
        buttonPressed = 1;    // Simula botón presionado
        tankOK = 1;           // Simula tanque lleno

        cupPresent = (ldrValue < 2000);

        switch (state) {
            case 0: // WAIT_CUP
                // Apagar relés (no implementado)
                if (cupPresent) {
                    state = 1; // SELECT_DRINK
                }
                break;
            case 1: // SELECT_DRINK
                if (potValue < 2048) {
                    drink = 0; // Bebida A
                    // Apagar LED (no implementado)
                } else {
                    drink = 1; // Bebida B
                    // Encender LED (no implementado)
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
                    // Apagar relés (no implementado)
                    state = 0; // WAIT_CUP
                    break;
                }
                if (drink == 0) {
                    // Relé A on, B off (no implementado)
                } else {
                    // Relé A off, B on (no implementado)
                }
                if (!buttonPressed) {
                    // Apagar relés (no implementado)
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
