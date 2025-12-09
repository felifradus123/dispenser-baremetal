/*
 * startup.S - Rutina mínima de arranque para ESP32-C3 bare-metal
 * Inicializa stack, limpia .bss, copia .data y llama a main.
 */
    .section .init
    .globl _start
    .option norvc
_start:
    la   sp, _stack_top
    la   t0, _sbss
    la   t1, _ebss
1:  bge  t0, t1, 2f
    sw   zero, 0(t0)
    addi t0, t0, 4
    j    1b
2:
    la   t0, _sdata
    la   t1, _edata
    la   t2, _etext
3:  bge  t0, t1, 4f
    lw   t3, 0(t2)
    sw   t3, 0(t0)
    addi t0, t0, 4
    addi t2, t2, 4
    j    3b
4:
    call main
5:  j 5b
    .size _start, .-_start
