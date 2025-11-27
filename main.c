/*
 * ================================================================
 * DISPENSER BAREMETAL - ESP32-C3 RISC-V 100% BARE METAL
 * ================================================================
 * 
 * Control de dispensador de bebidas SIN framework:
 * - Acceso directo a registros GPIO, UART, IO_MUX
 * - Sin librerías, sin RTOS, sin HAL
 * - Control manual de válvulas mediante botones físicos
 * - Startup propio (crt0.S) + linker script personalizado
 * 
 * Hardware:
 *   GPIO2,3,4  -> Válvulas A,B,C (relés)
 *   GPIO5,6,7  -> Botones A,B,C (pull-up interno)  
 *   GPIO8,9,10 -> LEDs indicadores A,B,C
 * 
 * ================================================================
 */

#include <stdint.h>

/* ==================== Definiciones de Registros ==================== */

/* UART0 Base: 0x60000000 */
#define UART0_BASE             0x60000000UL
#define UART_FIFO_REG          (*(volatile uint32_t*)(UART0_BASE + 0x0000))

/* GPIO Base: 0x60004000 - Copiado de led-polling para compatibilidad */
#define GPIO_BASE              0x60004000UL
#define GPIO_OUT_REG           (*(volatile uint32_t*)(GPIO_BASE + 0x0004))
#define GPIO_OUT_W1TS_REG      (*(volatile uint32_t*)(GPIO_BASE + 0x0008))
#define GPIO_OUT_W1TC_REG      (*(volatile uint32_t*)(GPIO_BASE + 0x000C))
#define GPIO_ENABLE_W1TS_REG   (*(volatile uint32_t*)(GPIO_BASE + 0x0024))
#define GPIO_IN_REG            (*(volatile uint32_t*)(GPIO_BASE + 0x003C))

/* IO_MUX Base: 0x60009000 */
#define IO_MUX_BASE            0x60009000UL
/* En ESP32-C3 los registros IO_MUX están espaciados cada 0x4 por GPIO.
 * Dirección = IO_MUX_BASE + (gpio * 4).
 */
static inline volatile uint32_t* IO_MUX_REG_PTR(uint8_t gpio)
{
    return (volatile uint32_t*)(IO_MUX_BASE + ((uint32_t)gpio * 4U));
}

/* Timer Group Watchdog (TIMG0 y TIMG1) */
#define TIMG0_BASE             0x6001F000UL
#define TIMG1_BASE             0x60020000UL
#define TIMG_WDTCONFIG0_OFFSET 0x0048
#define TIMG_WDTFEED_OFFSET    0x0060
#define TIMG_WDTWPROTECT_OFFSET 0x0064
#define TIMG_WDT_UNLOCK_KEY    0x50D83AA1U

/* RTC Watchdog */
#define RTC_CNTL_BASE          0x60008000UL
#define RTC_CNTL_WDTCONFIG0_OFFSET   0x0090
#define RTC_CNTL_WDTFEED_OFFSET      0x00A4
#define RTC_CNTL_WDTWPROTECT_OFFSET  0x00A8
#define RTC_CNTL_SWD_CONF_OFFSET     0x00AC
#define RTC_CNTL_SWD_WPROTECT_OFFSET 0x00B0
#define RTC_CNTL_WDT_UNLOCK_KEY      0x50D83AA1U
#define RTC_CNTL_SWD_UNLOCK_KEY      0x8F1D312AU

/* ==================== Configuración del Dispenser ==================== */

/* GPIOs para válvulas (salidas - conectar a relés) */
#define VALVE_A_GPIO    2    /* Válvula Bebida A (Coca Cola) */
#define VALVE_B_GPIO    3    /* Válvula Bebida B (Sprite) */

/* GPIOs para botones (entradas con pull-up) */
#define BOTON_A_GPIO    5    /* Botón Bebida A */
#define BOTON_B_GPIO    6    /* Botón Bebida B */

/* Configuración de timing */
#define CPU_FREQ_MHZ 160              /* Frecuencia CPU en MHz */
#define POLLING_DELAY_MS 20           /* Delay entre lecturas de botones (50Hz) */
#define POLLING_CYCLES ((uint32_t)(POLLING_DELAY_MS) * (CPU_FREQ_MHZ * 1000UL))

static volatile uint32_t* iomux_reg_for_pin(uint8_t gpio)
{
    return IO_MUX_REG_PTR(gpio);
}

/* ==================== Funciones Auxiliares ==================== */

/*
 * delay() - Espera ocupada (busy-wait)
 * Nota: No es precisa, depende de la frecuencia de CPU y optimizaciones.
 */
static void delay(volatile uint32_t cycles) {
    while (cycles--) {
        __asm__ volatile ("nop");
    }
}

/*
 * disable_timg_wdt() - Deshabilita watchdog de un timer group
 */
static void disable_timg_wdt(uint32_t timer_base) {
    volatile uint32_t *wdt_protect = (volatile uint32_t *)(timer_base + TIMG_WDTWPROTECT_OFFSET);
    volatile uint32_t *wdt_config0 = (volatile uint32_t *)(timer_base + TIMG_WDTCONFIG0_OFFSET);
    volatile uint32_t *wdt_feed    = (volatile uint32_t *)(timer_base + TIMG_WDTFEED_OFFSET);

    /* Desbloquear registros WDT */
    *wdt_protect = TIMG_WDT_UNLOCK_KEY;
    
    /* Alimentar watchdog (reset counter) */
    *wdt_feed = 1;
    
    /* Deshabilitar watchdog */
    uint32_t reg = *wdt_config0;
    reg &= ~(1U << 31);  /* TIMG_WDT_EN = 0 */
    reg |= (1U << 22);   /* TIMG_WDT_CONF_UPDATE_EN = 1 */
    *wdt_config0 = reg;
    
    /* Bloquear registros */
    *wdt_protect = 0;
}

/*
 * disable_rtc_wdts() - Deshabilita watchdogs RTC (MWDT y SWD)
 */
static void disable_rtc_wdts(void) {
    volatile uint32_t *wdt_protect = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_WDTWPROTECT_OFFSET);
    volatile uint32_t *wdt_config0 = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_WDTCONFIG0_OFFSET);
    volatile uint32_t *wdt_feed    = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_WDTFEED_OFFSET);
    volatile uint32_t *swd_protect = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_SWD_WPROTECT_OFFSET);
    volatile uint32_t *swd_conf    = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_SWD_CONF_OFFSET);

    /* Deshabilitar MWDT (Main WDT) */
    *wdt_protect = RTC_CNTL_WDT_UNLOCK_KEY;
    *wdt_feed = (1U << 31);
    
    uint32_t reg = *wdt_config0;
    reg &= ~(1U << 31);  /* RTC_CNTL_WDT_EN = 0 */
    *wdt_config0 = reg;
    *wdt_protect = 0;

    /* Deshabilitar SWD (Super WDT) */
    *swd_protect = RTC_CNTL_SWD_UNLOCK_KEY;
    *swd_conf |= (1U << 30);  /* Deshabilitar SWD */
    *swd_protect = 0;
}

/*
 * uart_putc() - Enviar un byte por UART0 
 */
static void uart_putc(char c) {
    /* Delay más largo para transmisión estable */
    for (volatile int i = 0; i < 15000; i++);
    UART_FIFO_REG = (uint32_t)c;
    for (volatile int i = 0; i < 15000; i++);
}

/*
 * uart_puts() - Enviar string por UART0
 */
static void uart_puts(const char* str) {
    while (*str) {
        uart_putc(*str++);
    }
}

/*
 * configurar_gpio_salida() - Configurar GPIO como salida
 */
static void configurar_gpio_salida(uint8_t gpio) {
    /* Configurar IO_MUX para función GPIO */
    volatile uint32_t* mux = iomux_reg_for_pin(gpio);
    if (mux) {
        uint32_t v = *mux;
        /* FUN_PD/FUN_PU off, FUN_IE off, MCU_SEL=GPIO(1) */
        v &= ~((1U << 7) | (1U << 8) | (1U << 9) | (0x7U << 12));
        v |= (1U << 12);
        *mux = v;
    }
    /* Habilitar como salida */
    GPIO_ENABLE_W1TS_REG = (1U << gpio);
}

/*
 * configurar_gpio_entrada() - Configurar GPIO como entrada con pull-up
 */
static void configurar_gpio_entrada(uint8_t gpio) {
    /* Configurar IO_MUX para función GPIO con pull-up */
    volatile uint32_t* mux = iomux_reg_for_pin(gpio);
    if (mux) {
        uint32_t v = *mux;
        /* FUN_PD off, FUN_PU on, FUN_IE on, MCU_SEL=GPIO(1) */
        v &= ~((1U << 7) | (0x7U << 12));  /* Clear FUN_PD y MCU_SEL */
        v |= (1U << 8) | (1U << 9) | (1U << 12);  /* Set FUN_PU, FUN_IE, MCU_SEL=1 */
        *mux = v;
    }
    /* NO habilitar como salida (entrada por defecto) */
}

/*
 * gpio_set() - Setear GPIO alto
 */
static inline void gpio_set(uint8_t gpio) {
    GPIO_OUT_W1TS_REG = (1U << gpio);
}

/*
 * gpio_clear() - Limpiar GPIO bajo
 */
static inline void gpio_clear(uint8_t gpio) {
    GPIO_OUT_W1TC_REG = (1U << gpio);
}

/*
 * gpio_read() - Leer estado de GPIO
 */
static inline uint8_t gpio_read(uint8_t gpio) {
    return (GPIO_IN_REG >> gpio) & 0x1;
}

/* ==================== Función Principal ==================== */

int main(void) {
    /* Deshabilitar todos los watchdogs para evitar resets */
    disable_timg_wdt(TIMG0_BASE);
    disable_timg_wdt(TIMG1_BASE);
    disable_rtc_wdts();

    /* Mensaje de inicio por UART */
    uart_puts("\r\n");
    uart_puts("================================================================\r\n");
    uart_puts("DISPENSER BAREMETAL ESP32-C3 - 100% BARE METAL\r\n");
    uart_puts("================================================================\r\n");
    uart_puts("Sin framework - Acceso directo a registros\r\n");
    uart_puts("Startup: crt0.S + linker.ld personalizado\r\n");
    uart_puts("================================================================\r\n");
    uart_puts("Hardware:\r\n");
    uart_puts("  GPIO2,3,18 -> Valvulas A,B,C (reles)\r\n");
    uart_puts("  GPIO5,6,7  -> Botones A,B,C (pull-up)\r\n");
    uart_puts("================================================================\r\n");
    uart_puts("Control: Presiona y manten boton -> Valvula ON\r\n");
    uart_puts("         Suelta boton -> Valvula OFF\r\n");
    uart_puts("================================================================\r\n\r\n");

    /* Configurar GPIOs como salidas (solo válvulas) */
    configurar_gpio_salida(VALVE_A_GPIO);
    configurar_gpio_salida(VALVE_B_GPIO);

    /* Configurar GPIOs como entradas con pull-up (botones) */
    configurar_gpio_entrada(BOTON_A_GPIO);
    configurar_gpio_entrada(BOTON_B_GPIO);

    /* Inicializar válvulas en OFF */
    gpio_clear(VALVE_A_GPIO);
    gpio_clear(VALVE_B_GPIO);

    uart_puts("Hardware configurado. Sistema listo!\r\n\r\n");

    /* Estados anteriores para detección de cambios */
    uint8_t boton_A_anterior = 1;  /* Pull-up: no presionado = 1 */
    uint8_t boton_B_anterior = 1;

    /* Bucle infinito: control directo de válvulas por botones */
    while (1) {
        /* Leer estado actual de botones */
        uint8_t boton_A_actual = gpio_read(BOTON_A_GPIO);
        uint8_t boton_B_actual = gpio_read(BOTON_B_GPIO);

        /* Control Botón A -> Válvula A */
        if (boton_A_actual == 0) {  /* Presionado (pull-up: 0 = presionado) */
            if (boton_A_anterior == 1) {  /* Cambió de no presionado a presionado */
                uart_puts("COCA COLA: Valvula ON\r\n");
                gpio_set(VALVE_A_GPIO);
            }
        } else {  /* No presionado */
            if (boton_A_anterior == 0) {  /* Cambió de presionado a no presionado */
                uart_puts("COCA COLA: Valvula OFF\r\n");
                gpio_clear(VALVE_A_GPIO);
            }
        }

        /* Control Botón B -> Válvula B */
        if (boton_B_actual == 0) {  /* Presionado */
            if (boton_B_anterior == 1) {
                uart_puts("SPRITE: Valvula ON\r\n");
                gpio_set(VALVE_B_GPIO);
            }
        } else {  /* No presionado */
            if (boton_B_anterior == 0) {
                uart_puts("SPRITE: Valvula OFF\r\n");
                gpio_clear(VALVE_B_GPIO);
            }
        }

        /* Actualizar estados anteriores */
        boton_A_anterior = boton_A_actual;
        boton_B_anterior = boton_B_actual;

        /* Delay para polling eficiente */
        delay(POLLING_CYCLES);
    }

    return 0;  /* Nunca se alcanza */
}