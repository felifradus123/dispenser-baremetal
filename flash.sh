#!/bin/bash
# flash.sh - Script para flashear el binario del dispenser al ESP32-C3

set -e

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Puerto serie (argumento o por defecto)
PORT="${1:-/dev/ttyUSB0}"

# Archivo binario
BIN_FILE="build/dispenser_app.bin"

# Verificar que el binario existe
if [ ! -f "$BIN_FILE" ]; then
    echo -e "${RED}Error: $BIN_FILE no existe${NC}"
    echo "Ejecuta ./build.sh primero"
    exit 1
fi

echo -e "${YELLOW}=== Flasheando Dispenser ESP32-C3 ===${NC}"
echo "Puerto: $PORT"
echo "Binario: $BIN_FILE"
echo ""

# Flashear usando esptool (con varias rutas de fallback)
# Offset 0x10000 = partición de app por defecto
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
    echo -e "${RED}No se encontró esptool. Instala esptool (pip install esptool) o exporta el entorno de ESP-IDF.${NC}"
    exit 1
fi

$ESPTOOL_CMD \
    --chip esp32c3 \
    --port "$PORT" \
    --baud 460800 \
    --before default_reset \
    --after hard_reset \
    write_flash \
    --flash_mode dio \
    --flash_freq 80m \
    --flash_size detect \
    0x10000 "$BIN_FILE"

echo ""
echo -e "${GREEN}✓ Flash completado!${NC}"
echo ""
echo "El dispenser debería comenzar a transmitir por UART."
echo ""
echo "Para ver logs/heartbeat:"
echo "  python3 -m serial.tools.miniterm $PORT 115200"