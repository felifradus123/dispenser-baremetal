/*
 * crt0.S - Runtime startup for ESP32-C3 RISC-V bare-metal
 * 
 * Este archivo implementa el punto de entrada del programa (_start)
 * y configura el entorno básico antes de llamar a main().
 * 
 * Tareas realizadas:
 * 1. Inicializar el stack pointer
 * 2. Limpiar la sección .bss
 * 3. Copiar datos inicializados de flash a RAM
 * 4. Llamar a main()
 * 5. Loop infinito si main() retorna
 */

    .section .text._start
    .global _start
    .type _start, @function

_start:
    /* 1. Configurar stack pointer */
    la sp, _stack_top
    
    /* 1.5. Configurar global pointer (importante para RISC-V) */
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    
    /* 2. Limpiar sección .bss (variables globales no inicializadas) */
    la t0, _bss_start
    la t1, _bss_end
    
clear_bss:
    beq t0, t1, bss_done
    sw zero, 0(t0)
    addi t0, t0, 4
    j clear_bss
    
bss_done:
    
    /* 3. Copiar datos inicializados de flash a RAM */
    la t0, _data_start      /* Inicio de .data en RAM */
    la t1, _data_end        /* Fin de .data en RAM */
    la t2, _data_load_start /* Inicio de .data en flash */
    
copy_data:
    beq t0, t1, data_done
    lw t3, 0(t2)            /* Leer de flash */
    sw t3, 0(t0)            /* Escribir a RAM */
    addi t0, t0, 4
    addi t2, t2, 4
    j copy_data
    
data_done:
    
    /* 4. Llamar a main() */
    call main
    
    /* 5. Si main() retorna, loop infinito */
infinite_loop:
    j infinite_loop

    .size _start, .-_start