#!/usr/bin/env bash
set -e

# =============================================================
# VERSIONAMENTO AUTOMÁTICO
# Unshallow no clone do Cloudflare Pages (que usa --depth=1)
# para obter o total real de commits ou timestamp
# =============================================================
git fetch --unshallow 2>/dev/null || true

VERSION=$(git rev-list --count HEAD 2>/dev/null || echo 0)
if [ "$VERSION" -le 1 ]; then
  # Fallback caso não consiga unshallow: timestamp unix do commit
  VERSION=$(git log -1 --format=%ct 2>/dev/null || date +%s)
fi

echo "=== Versão deste build: $VERSION ==="

# Patcha a versão do firmware no .ino ANTES de compilar
sed -i "s/#define CURRENT_FIRMWARE_VER .*/#define CURRENT_FIRMWARE_VER $VERSION/" firmware/codigo_esp/codigo_esp.ino

echo "[VERSION] codigo_esp.ino patchado:"
grep "CURRENT_FIRMWARE_VER" firmware/codigo_esp/codigo_esp.ino

echo "=== [1/3] Instalando arduino-cli ==="
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=. sh
export PATH=$PATH:.

echo "=== [2/3] Configurando Core ESP8266 ==="
arduino-cli config init --additional-urls https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266

echo "=== [3/3] Instalando Bibliotecas ESP ==="
arduino-cli lib install "ArduinoJson" "LiquidCrystal I2C" || true

mkdir -p dist

echo "=== Compilando Firmware ESP8266 ==="
SKETCH_DIR="firmware/codigo_esp"
mkdir -p build_esp
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 \
  --output-dir ./build_esp \
  "$SKETCH_DIR"

mv ./build_esp/*.bin ./dist/firmware.bin || true

# Gera o version.json com a mesma versão gravada no firmware
cat > ./dist/version.json << VERSIONJSON
{
  "firmware_version": $VERSION,
  "firmware_url": "https://makitaclicker.pages.dev/firmware.bin"
}
VERSIONJSON

echo "[VERSION] version.json gerado:"
cat ./dist/version.json

echo "=== Pipeline concluido com sucesso ==="
