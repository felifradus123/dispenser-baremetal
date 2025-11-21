# Dispenser Bare-Metal ESP32-C3

**Proyecto 100% bare-metal para ESP32-C3 que implementa un sistema de heartbeat por UART.**

Este proyecto demuestra programación bare-metal pura en ESP32-C3, sin frameworks ni librerías estándar, usando acceso directo a registros para transmisión UART.

## Características

- **Totalmente bare-metal**: Sin ESP-IDF framework, sin librerías estándar, sin HAL
- **Acceso directo a registros**: Control directo de hardware UART y watchdogs
- **Startup personalizado**: Código de arranque en assembly RISC-V (crt0.S)
- **Linker script personalizado**: Definición propia de memoria y secciones
- **Sistema de build independiente**: Compilación con toolchain RISC-V puro

## Estructura del Proyecto

```
dispenser-baremetal/
├── main.c         # Código principal con UART heartbeat
├── crt0.S         # Startup assembly RISC-V
├── linker.ld      # Script de enlazado para ESP32-C3
├── build.sh       # Script de compilación
├── flash.sh       # Script de flasheo
└── README.md      # Este archivo
```

## Funcionalidad

El dispenser transmite un mensaje de heartbeat numerado cada segundo por UART0:

```
HEARTBEAT 1
HEARTBEAT 2
HEARTBEAT 3
...
```

## Requisitos

- **Toolchain RISC-V**: `riscv32-esp-elf-gcc`
- **esptool**: Para generar binarios y flashear
- **Hardware**: ESP32-C3 DevKitC-02 o compatible

## Instalación

1. Instalar ESP-IDF (para obtener el toolchain):
   ```bash
   git clone --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh
   . ./export.sh
   ```

## Compilación

```bash
# Configurar entorno (una vez por terminal)
. $HOME/esp/esp-idf/export.sh

# Compilar
./build.sh
```

## Flasheo

```bash
# Flashear al dispositivo (ajustar puerto si necesario)
./flash.sh [/dev/ttyUSB0]
```

## Monitoreo

Para ver los mensajes de heartbeat:

```bash
python3 -m serial.tools.miniterm /dev/ttyUSB0 115200
```

## Arquitectura Bare-Metal

### Startup Sequence (crt0.S)
1. Inicialización del stack pointer
2. Limpieza de sección .bss (variables no inicializadas)
3. Copia de datos inicializados de flash a RAM
4. Llamada a main()

### Memory Layout (linker.ld)
- **IRAM** (0x4037C000): RAM para instrucciones (400KB)
- **DRAM** (0x3FC80000): RAM para datos y stack (384KB)
- **Flash** (0x42000000): Código principal mapeado
- **DROM** (0x3C000000): Datos readonly en flash

### Register Access (main.c)
- **UART0** (0x60000000): Control directo del FIFO de transmisión
- **Watchdogs** (TIMG0/1, RTC): Deshabilitación para evitar resets
- **Timing**: Busy-wait loops con conteo de ciclos

## Diferencias con ESP-IDF Framework

| Aspecto | Framework ESP-IDF | Bare-Metal |
|---------|-------------------|------------|
| Startup | app_main() | main() con crt0.S |
| Build | idf.py build | build.sh directo |
| Memory | Automático | linker.ld manual |
| UART | driver_uart | Registros directos |
| Libs | FreeRTOS, newlib | Solo typedefs locales |

## Notas Técnicas

- **Bootloader**: Utiliza bootloader ESP-IDF para arranque inicial, pero el código de usuario es 100% bare-metal
- **UART Config**: El bootloader ya configura UART0 a 115200 baud, solo escribimos al FIFO
- **Watchdogs**: Se deshabilitan explícitamente para evitar resets automáticos
- **Optimización**: Compilado con `-Os` para minimizar tamaño

## Troubleshooting

### Error de toolchain no encontrado
```bash
# Asegurarse que ESP-IDF esté exportado
. $HOME/esp/esp-idf/export.sh
```

### Error de puerto serie
```bash
# Verificar dispositivo conectado
ls /dev/ttyUSB*
# o
ls /dev/serial/by-id/
```

### No aparecen mensajes
- Verificar baudrate (115200)
- Verificar conexión USB
- Probar con `esptool.py --port /dev/ttyUSB0 chip_id`

## Extensiones Posibles

- Agregar GPIO para LEDs de estado
- Implementar protocolo de comunicación serial
- Agregar sensores y actuadores para dispensador real
- Implementar timers hardware en lugar de busy-wait

---

**Proyecto académico demostrando programación bare-metal en ESP32-C3 RISC-V**
