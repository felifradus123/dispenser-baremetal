/*
 * linker.ld - Script de enlazado para ESP32-C3 bare-metal
 * 
 * Define el mapa de memoria y ubicaciones de secciones para ESP32-C3.
 * 
 * Memoria ESP32-C3:
 * - IRAM: RAM para instrucciones (0x4037C000-0x403E0000, 400KB)
 * - DRAM: RAM para datos (0x3FC80000-0x3FCE0000, 384KB)  
 * - Flash: Memoria de programa (0x42000000+, mapeada)
 * - DROM: Datos readonly en flash (0x3C000000+, mapeada)
 * 
 * Nota: Este proyecto usa el bootloader ESP-IDF que carga la aplicación
 * en la dirección 0x42000000 (flash mapeada), no directamente en IRAM.
 */

ENTRY(_start)

/* Configuración de memoria del ESP32-C3 */
MEMORY
{
    /* RAM para instrucciones - ejecutamos desde aquí después del bootloader */
    iram0_0_seg (RX)  : org = 0x4037C000, len = 0x64000  /* 400KB IRAM */
    
    /* RAM para datos - variables globales y stack */
    dram0_0_seg (RW)  : org = 0x3FC80000, len = 0x60000  /* 384KB DRAM */
    
    /* Flash mapeada - código principal (donde bootloader carga la app) */
    irom0_0_seg (RX)  : org = 0x42000000, len = 0x400000 /* 4MB flash code */
    
    /* Datos readonly en flash mapeada */
    drom0_0_seg (R)   : org = 0x3C000000, len = 0x400000 /* 4MB flash data */
}

/* Definir ubicación del stack (tope de DRAM) */
_stack_top = ORIGIN(dram0_0_seg) + LENGTH(dram0_0_seg);

SECTIONS
{
    /* Sección de código en flash mapeada */
    .text : ALIGN(4)
    {
        _text_start = ABSOLUTE(.);
        
        /* Punto de entrada debe ir primero */
        *(.text._start)
        
        /* Resto del código */
        *(.text .text.*)
        *(.literal .literal.*)
        
        /* Asegurar alineación */
        . = ALIGN(4);
        _text_end = ABSOLUTE(.);
    } > irom0_0_seg
    
    /* Datos readonly en flash */
    .rodata : ALIGN(4)
    {
        _rodata_start = ABSOLUTE(.);
        *(.rodata .rodata.*)
        . = ALIGN(4);
        _rodata_end = ABSOLUTE(.);
    } > drom0_0_seg
    
    /* Datos inicializados: almacenados en flash, copiados a RAM */
    .data : ALIGN(4)
    {
        _data_start = ABSOLUTE(.);
        *(.data .data.*)
        . = ALIGN(4);
        _data_end = ABSOLUTE(.);
    } > dram0_0_seg AT > drom0_0_seg
    
    /* Global pointer para RISC-V */
    __global_pointer$ = MIN(_data_start + 0x800, MAX(_data_start + _data_end, _bss_start + _bss_end)) - 0x800;
    
    /* Ubicación de los datos en flash (para copiar a RAM) */
    _data_load_start = LOADADDR(.data);
    
    /* Datos no inicializados (limpiados a cero) */
    .bss (NOLOAD) : ALIGN(4)
    {
        _bss_start = ABSOLUTE(.);
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = ABSOLUTE(.);
    } > dram0_0_seg
    
    /* Información de debug (no cargada al dispositivo) */
    .debug_info     0 : { *(.debug_info) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_ranges   0 : { *(.debug_ranges) }
    .debug_loc      0 : { *(.debug_loc) }
    
    /* Descartar secciones no necesarias */
    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.gnu.attributes)
    }
}
ENTRY(_start)
MEMORY {
    IROM (rx)  : ORIGIN = 0x42000000, LENGTH = 2M
    DRAM (rwx) : ORIGIN = 0x3FC80000, LENGTH = 400K
}
SECTIONS {
    .text : {
        _stext = .;
        *(.init)
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > IROM
    .data : AT (LOADADDR(.text) + SIZEOF(.text)) {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > DRAM
    .bss (NOLOAD) : {
        . = ALIGN(8);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _ebss = .;
    } > DRAM
    _stack_top = ORIGIN(DRAM) + LENGTH(DRAM);
    _sheap = _ebss;
    _eheap = _stack_top - 0x2000;
}