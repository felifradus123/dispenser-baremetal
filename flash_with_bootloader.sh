#!/bin/bash
# flash_with_bootloader.sh - Script para flashear usando bootloader v6.0 que funciona

set -e

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Puerto serie (argumento o por defecto)
PORT="${1:-/dev/ttyUSB0}"

# Archivos
BIN_FILE="build/dispenser_app.bin"
BOOTLOADER_FILE="bootloader.bin"
PARTITION_FILE="partition-table.bin"

# Verificar que todos los archivos existen
if [ ! -f "$BIN_FILE" ]; then
    echo -e "${RED}Error: $BIN_FILE no existe${NC}"
    echo "Ejecuta ./build.sh primero"
    exit 1
fi

if [ ! -f "$BOOTLOADER_FILE" ]; then
    echo -e "${RED}Error: $BOOTLOADER_FILE no existe${NC}"
    exit 1
fi

if [ ! -f "$PARTITION_FILE" ]; then
    echo -e "${RED}Error: $PARTITION_FILE no existe${NC}"
    exit 1
fi

echo -e "${YELLOW}=== Flasheando Dispenser con Bootloader v6.0 ===${NC}"
echo "Puerto: $PORT"
echo "Bootloader: $BOOTLOADER_FILE"
echo "Partitions: $PARTITION_FILE"
echo "App: $BIN_FILE"
echo ""

# Flashear usando esptool completo como ESP-IDF
ESPTOOL_CMD=""
if command -v esptool >/dev/null 2>&1; then
    ESPTOOL_CMD="esptool"
elif python3 -c "import esptool" >/dev/null 2>&1; then
    ESPTOOL_CMD="python3 -m esptool"
elif [ -n "$IDF_PATH" ] && [ -f "$IDF_PATH/components/esptool_py/esptool/esptool.py" ]; then
    ESPTOOL_CMD="env PYTHONPATH=$IDF_PATH/components/esptool_py python3 -m esptool"
elif [ -f "/home/f/esp/esp-idf/components/esptool_py/esptool/esptool.py" ]; then
    ESPTOOL_CMD="env PYTHONPATH=/home/f/esp/esp-idf/components/esptool_py python3 -m esptool"
fi

if [ -z "$ESPTOOL_CMD" ]; then
    echo -e "${RED}No se encontró esptool.${NC}"
    exit 1
fi

# Flash completo: bootloader + partitions + app (como hello_world)
$ESPTOOL_CMD \
    --chip esp32c3 \
    --port "$PORT" \
    --baud 460800 \
    --before default_reset \
    --after hard_reset \
    write_flash \
    --flash_mode dio \
    --flash_freq 80m \
    --flash_size 2MB \
    0x0 "$BOOTLOADER_FILE" \
    0x8000 "$PARTITION_FILE" \
    0x10000 "$BIN_FILE"

echo ""
echo -e "${GREEN}✓ Flash completado con bootloader v6.0!${NC}"
echo ""
echo "El dispenser debería comenzar a transmitir por UART."
echo ""
echo "Para ver logs/heartbeat:"
echo "  python3 -m serial.tools.miniterm $PORT 115200"