#!/bin/bash
# build.sh - Script para compilar el proyecto dispenser bare-metal

set -e

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Configuración del proyecto
PROJECT_NAME="dispenser_app"
BUILD_DIR="build"
SOURCE_FILES="crt0.S main.c"

# Toolchain RISC-V para ESP32-C3
TOOLCHAIN_PREFIX="riscv32-esp-elf"
CC="${TOOLCHAIN_PREFIX}-gcc"
OBJCOPY="${TOOLCHAIN_PREFIX}-objcopy"
SIZE="${TOOLCHAIN_PREFIX}-size"

# Verificar que el toolchain existe
if ! command -v "$CC" >/dev/null 2>&1; then
    echo -e "${RED}Error: Toolchain RISC-V no encontrado ($CC)${NC}"
    echo "Instala esp-idf y ejecuta:"
    echo "  . \$HOME/esp/esp-idf/export.sh"
    exit 1
fi

echo -e "${YELLOW}=== Compilando $PROJECT_NAME ===${NC}"

# Crear directorio build
mkdir -p "$BUILD_DIR"

# Flags de compilación para ESP32-C3
CFLAGS=(
    -march=rv32imc          # Arquitectura RISC-V para ESP32-C3
    -mabi=ilp32             # ABI para 32-bit
    -Wall                   # Warnings
    -Wextra                 # Warnings adicionales
    -Os                     # Optimización para tamaño
    -ffunction-sections     # Una función por sección (para linker)
    -fdata-sections         # Datos por sección (para linker)
    -nostdlib               # No usar librerías estándar
    -nostartfiles           # No usar startup files del sistema
    -ffreestanding          # Código freestanding (sin OS)
    -T linker.ld            # Usar nuestro linker script
)

# Compilar y enlazar
echo "Compilando con ${TOOLCHAIN_PREFIX}-gcc..."
$CC "${CFLAGS[@]}" $SOURCE_FILES -o "$BUILD_DIR/${PROJECT_NAME}.elf"

# Mostrar información del binario
echo ""
echo -e "${GREEN}Compilación exitosa!${NC}"
echo ""
$SIZE "$BUILD_DIR/${PROJECT_NAME}.elf"
echo ""

# Generar binario para flash usando esptool
echo "Generando binario para flash..."

# Usar esptool desde ESP-IDF
ESPTOOL_CMD="python3 /home/f/esp/esp-idf/components/esptool_py/esptool/esptool.py"

if [ -z "$ESPTOOL_CMD" ]; then
    echo -e "${RED}Advertencia: No se encontró esptool.${NC}"
    echo "Instala esptool (pip install esptool) o exporta el entorno de ESP-IDF"
    echo "El archivo ELF está listo en: $BUILD_DIR/${PROJECT_NAME}.elf"
    exit 0
fi

# Convertir ELF a binario compatible con ESP32-C3
$ESPTOOL_CMD \
    --chip esp32c3 \
    elf2image \
    --flash_mode dio \
    --flash_freq 80m \
    --flash_size 4MB \
    --output "$BUILD_DIR/${PROJECT_NAME}.bin" \
    "$BUILD_DIR/${PROJECT_NAME}.elf"

echo ""
echo -e "${GREEN}✓ Build completado!${NC}"
echo ""
echo "Archivos generados:"
echo "  ELF: $BUILD_DIR/${PROJECT_NAME}.elf"
echo "  BIN: $BUILD_DIR/${PROJECT_NAME}.bin"
echo ""
echo "Para flashear:"
echo "  ./flash.sh"