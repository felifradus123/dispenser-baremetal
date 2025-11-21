/*
 * Dispenser Bare-Metal para ESP32-C3
 * 
 * Basado en LED Polling Bare-Metal. Acceso directo a registros UART 
 * sin librerías, sin RTOS, sin HAL. Solo polling puro con busy-wait.
 * 
 * NOTA: Este proyecto usa el bootloader de ESP-IDF para arrancar,
 * pero el código de usuario es 100% bare-metal con acceso directo a registros.
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

/* ==================== Configuración ==================== */

#define LED_GPIO     2                /* LED objetivo: GPIO2 - mantenido para compatibilidad */

/* Configuración de timing */
#define CPU_FREQ_MHZ 160              /* Frecuencia CPU en MHz (típicamente 160 MHz) */
#define DELAY_MS     1000             /* Delay en milisegundos para UART */

/* Cálculo automático de ciclos: ms * (MHz * 1000) */
#define DELAY_CYCLES ((uint32_t)(DELAY_MS) * (CPU_FREQ_MHZ * 1000UL))

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
    /* Delay mínimo para transmisión */
    for (volatile int i = 0; i < 5000; i++);
    UART_FIFO_REG = (uint32_t)c;
    for (volatile int i = 0; i < 5000; i++);
}

/* ==================== Función Principal ==================== */

int main(void) {
    /* Deshabilitar todos los watchdogs para evitar resets */
    disable_timg_wdt(TIMG0_BASE);
    disable_timg_wdt(TIMG1_BASE);
    disable_rtc_wdts();

    /* Configurar IO_MUX del GPIO objetivo como función GPIO y habilitar salida */
    /* Mantenido por compatibilidad - no se usa pero evita cambios en el linker */
    volatile uint32_t* mux = iomux_reg_for_pin(LED_GPIO);
    if (mux) {
        uint32_t v = *mux;
        /* FUN_PD/FUN_PU off, FUN_IE off, MCU_SEL=GPIO(1) */
        v &= ~((1U << 7) | (1U << 8) | (1U << 9) | (0x7U << 12));
        v |= (1U << 12);
        *mux = v;
    }

    /* Habilitar salida para el pin */
    const uint32_t bit = (1U << LED_GPIO);
    GPIO_ENABLE_W1TS_REG = bit;

    uint32_t counter = 1;

    /* Bucle infinito: UART polling directo */
    while (1) {
        /* Enviar "pulso X" por UART */
        uart_putc('p');
        uart_putc('u');
        uart_putc('l');
        uart_putc('s');
        uart_putc('o');
        uart_putc(' ');
        uart_putc('0' + counter);
        uart_putc('\r');
        uart_putc('\n');
        
        counter++;
        if (counter > 9) counter = 1;
        
        delay(DELAY_CYCLES);   /* Usar mismo timing que LED */
    }

    return 0;  /* Nunca se alcanza */
}