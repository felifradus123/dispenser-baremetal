/*
 * Dispenser Bare-Metal para ESP32-C3
 * 
 * Acceso directo a registros UART sin librerías, sin RTOS, sin HAL.
 * Solo polling puro con busy-wait para transmisión de heartbeat.
 * 
 * NOTA: Este proyecto NO usa bootloader ESP-IDF. Es 100% bare-metal.
 */

/* ==================== Tipos de Datos Locales ==================== */
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

/* ==================== Definiciones de Registros ==================== */

/* UART0 Base: 0x60000000 */
#define UART0_BASE             0x60000000UL
#define UART_FIFO_REG          (*(volatile uint32_t*)(UART0_BASE + 0x0000))

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

/* Configuración de timing */
#define CPU_FREQ_MHZ 160              /* Frecuencia CPU en MHz */
#define DELAY_MS     1000             /* Delay en milisegundos */

/* Cálculo automático de ciclos */
#define DELAY_CYCLES ((uint32_t)(DELAY_MS) * (CPU_FREQ_MHZ * 1000UL))

/* ==================== Funciones Auxiliares ==================== */

/*
 * delay() - Espera ocupada (busy-wait)
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
 * disable_rtc_wdts() - Deshabilita watchdogs RTC
 */
static void disable_rtc_wdts(void) {
    volatile uint32_t *wdt_protect = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_WDTWPROTECT_OFFSET);
    volatile uint32_t *wdt_config0 = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_WDTCONFIG0_OFFSET);
    volatile uint32_t *wdt_feed    = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_WDTFEED_OFFSET);
    volatile uint32_t *swd_protect = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_SWD_WPROTECT_OFFSET);
    volatile uint32_t *swd_conf    = (volatile uint32_t *)(RTC_CNTL_BASE + RTC_CNTL_SWD_CONF_OFFSET);

    /* Deshabilitar MWDT */
    *wdt_protect = RTC_CNTL_WDT_UNLOCK_KEY;
    *wdt_feed = (1U << 31);
    
    uint32_t reg = *wdt_config0;
    reg &= ~(1U << 31);  /* RTC_CNTL_WDT_EN = 0 */
    *wdt_config0 = reg;
    *wdt_protect = 0;

    /* Deshabilitar SWD */
    *swd_protect = RTC_CNTL_SWD_UNLOCK_KEY;
    *swd_conf |= (1U << 30);
    *swd_protect = 0;
}

/*
 * uart_putc() - Enviar un byte por UART0
 */
static void uart_putc(char c) {
    /* Delay para transmisión estable */
    for (volatile int i = 0; i < 8000; i++);
    UART_FIFO_REG = (uint32_t)c;
    for (volatile int i = 0; i < 8000; i++);
}

/* ==================== Función Principal ==================== */

int main(void) {
    /* Deshabilitar watchdogs */
    disable_timg_wdt(TIMG0_BASE);
    disable_timg_wdt(TIMG1_BASE);
    disable_rtc_wdts();

    uint32_t counter = 1;
    
    /* Delay inicial grande */
    delay(DELAY_CYCLES * 4);

    /* Bucle infinito */
    while (1) {
        /* Enviar "pulso X" */
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
        
        /* Delay entre mensajes */
        delay(DELAY_CYCLES * 2);
    }

    return 0;
}