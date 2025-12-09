/*
 * main.c - Lógica de dispensador, versión bare-metal, sin defines ni librerías estándar.
 * Todo está comentado y explícito.
 * UART implementada a mano, sin printf ni Arduino.
 */

#include <stdint.h>

// --- Direcciones base de periféricos ---
// GPIO: 0x60004000
// UART0: 0x60000000
// ADC:   0x60040000

// --- Pines usados ---
// int PIN_LDR = 0;      // LDR para detectar vaso
// int PIN_POT = 1;      // Potenciómetro para seleccionar bebida
// int PIN_LED = 10;     // LED de selección
// int PIN_BUTTON = 19;  // Botón para dispensar
// int PIN_RELAY_A = 4;  // Relé bebida A
// int PIN_RELAY_B = 5;  // Relé bebida B
// int PIN_TANK_SENSOR = 9; // Sensor de nivel de tanque

// --- Estados de la máquina ---
// enum State { WAIT_CUP, SELECT_DRINK, DISPENSE };
// State state = WAIT_CUP;
// bool cupPresent = false;
// bool drinkSelected = false;
// int drink = 0; // 0 = A, 1 = B

// --- UART mínimo ---
static void uart_init(void) {
    // Configurar divisor de baudrate, pines, etc. (no implementado)
}
static void uart_putc(char c) {
    // Escribir en FIFO TX de UART0 (no implementado)
    (void)c;
}
static void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

// --- Delay simple ---
static void delay(uint32_t n) {
    while (n--) {
        __asm__ volatile ("nop");
    }
}

int main(void) {
    uart_init();
    // Configurar pines como entrada/salida según corresponda (no implementado)
    // Inicializar estados
    // state = WAIT_CUP;
    // cupPresent = false;
    // drinkSelected = false;
    // drink = 0;

    while (1) {
        // int ldrValue = ...; // Leer ADC para LDR (no implementado)
        // int potValue = ...; // Leer ADC para potenciómetro (no implementado)
        // bool buttonPressed = ...; // Leer GPIO botón (no implementado)
        // bool tankOK = ...; // Leer GPIO sensor de tanque (no implementado)

        // cupPresent = (ldrValue < 2000);

        // switch (state) {
        //     case WAIT_CUP:
        //         // Apagar relés
        //         // if (cupPresent) state = SELECT_DRINK;
        //         break;
        //     case SELECT_DRINK:
        //         // Interpretar potenciómetro
        //         // if (potValue < 2048) { drink = 0; /* LED off */ }
        //         // else { drink = 1; /* LED on */ }
        //         // if (!cupPresent) break;
        //         // if (buttonPressed && tankOK) state = DISPENSE;
        //         break;
        //     case DISPENSE:
        //         // if (!cupPresent) { /* Apagar relés, volver a WAIT_CUP */ break; }
        //         // if (drink == 0) { /* Relé A on, B off */ }
        //         // else { /* Relé A off, B on */ }
        //         // if (!buttonPressed) { /* Apagar relés, volver a SELECT_DRINK */ }
        //         break;
        // }

        // uart_puts("Tanque 1: ");
        // uart_puts(tankOK ? "Lleno\n" : "Vacio\n");
        // uart_puts("Tanque 2: Vacio\n");
        // uart_puts("Tanque 3: Lleno\n");

        delay(10000); // Simula delay(10)
    }
}
