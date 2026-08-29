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

# Patcha os #define no .ino ANTES de compilar
sed -i "s/#define CURRENT_FIRMWARE_VER .*/#define CURRENT_FIRMWARE_VER $VERSION/" codigo_esp/codigo_esp.ino
sed -i "s/#define CURRENT_FS_VER .*/#define CURRENT_FS_VER       $VERSION/" codigo_esp/codigo_esp.ino
sed -i "s/#define CURRENT_MEGA_VER .*/#define CURRENT_MEGA_VER     $VERSION/" codigo_esp/codigo_esp.ino

echo "[VERSION] codigo_esp.ino patchado:"
grep "CURRENT_.*_VER" codigo_esp/codigo_esp.ino

echo "=== [1/6] Instalando arduino-cli ==="
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=. sh
export PATH=$PATH:.

echo "=== [2/6] Configurando Cores (ESP8266 e Arduino AVR) ==="
arduino-cli config init --additional-urls https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266
arduino-cli core install arduino:avr

echo "=== [3/6] Instalando Bibliotecas ==="
arduino-cli lib install "WebSockets"
arduino-cli lib install "ArduinoJson"
arduino-cli lib install "LiquidCrystal I2C"

mkdir -p online

echo "=== [4/6] Compilando Firmware ESP8266 ==="
SKETCH_DIR="codigo_esp"
mkdir -p build_esp
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 \
  --output-dir ./build_esp \
  "$SKETCH_DIR"

mv ./build_esp/*.bin ./online/firmware.bin || true

echo "=== [5/6] Compilando Firmware Arduino Mega ==="
MEGA_SKETCH_DIR="codigo_arduino"
mkdir -p build_mega
arduino-cli compile --fqbn arduino:avr:mega \
  --output-dir ./build_mega \
  "$MEGA_SKETCH_DIR"

AVR_OBJCOPY=$(find /opt/buildhome/.arduino15 /root/.arduino15 "$HOME/.arduino15" \
  -name avr-objcopy -type f 2>/dev/null | head -n 1)

if [ -n "$AVR_OBJCOPY" ]; then
  echo "[DEBUG] avr-objcopy encontrado em: $AVR_OBJCOPY"
  HEX_FILE=$(find ./build_mega -name "*.hex" ! -name "*with_bootloader*" -type f 2>/dev/null | head -n 1)
  if [ -n "$HEX_FILE" ]; then
    echo "[DEBUG] Convertendo $HEX_FILE para binario..."
    "$AVR_OBJCOPY" -I ihex -O binary "$HEX_FILE" ./online/mega.bin
    echo "[DEBUG] mega.bin gerado com sucesso (tamanho: $(wc -c < ./online/mega.bin) bytes)."
  else
    echo "[ERRO] Arquivo .hex do Mega nao encontrado!"
    exit 1
  fi
else
  echo "[ERRO] avr-objcopy nao encontrado!"
  exit 1
fi

echo "=== [6/6] Gerando Imagem do LittleFS ==="
MKLITTLEFS_BIN=$(find /opt/buildhome/.arduino15 /root/.arduino15 "$HOME/.arduino15" \
  -name mklittlefs -type f 2>/dev/null | head -n 1)

if [ -n "$MKLITTLEFS_BIN" ]; then
  "$MKLITTLEFS_BIN" -c "$SKETCH_DIR/data" -p 256 -b 8192 -s 2072576 ./online/littlefs.bin
else
  echo "[ERRO] mklittlefs nao encontrado!"
  exit 1
fi

# Gera o version.json com a mesma versão gravada nos firmwares
cat > ./online/version.json << VERSIONJSON
{
  "firmware_version": $VERSION,
  "fs_version": $VERSION,
  "mega_version": $VERSION,
  "firmware_url": "https://makitaclicker.pages.dev/firmware.bin",
  "fs_url": "https://makitaclicker.pages.dev/littlefs.bin",
  "mega_url": "https://makitaclicker.pages.dev/mega.bin"
}
VERSIONJSON

echo "[VERSION] version.json gerado:"
cat ./online/version.json

echo "=== Pipeline concluido com sucesso ==="
