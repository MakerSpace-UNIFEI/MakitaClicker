#!/usr/bin/env bash
set -e

echo "=== [1/5] Instalando arduino-cli ==="
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=. sh
export PATH=$PATH:.

echo "=== [2/5] Configurando Core ESP8266 ==="
arduino-cli config init --additional-urls https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266

echo "=== [3/5] Instalando Bibliotecas ==="
arduino-cli lib install "WebSockets"
arduino-cli lib install "ArduinoJson"

echo "=== [4/5] Compilando Firmware ==="
mkdir -p online
SKETCH_DIR="codigo_esp"

arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 \
  --output-dir ./online \
  "$SKETCH_DIR"

# Padroniza o nome do binario de saída
mv ./online/*.bin ./online/firmware.bin || true

echo "=== [5/5] Gerando Imagem do LittleFS ==="
MKLITTLEFS_BIN=$(find /root/.arduino15 -name mklittlefs 2>/dev/null | head -n 1 || find ~/.arduino15 -name mklittlefs 2>/dev/null | head -n 1)

if [ -n "$MKLITTLEFS_BIN" ]; then
  # 2072576 bytes = 2MB de particao FS para Flash de 4MB
  $MKLITTLEFS_BIN -c "$SKETCH_DIR/data" -p 256 -b 8192 -s 2072576 ./online/littlefs.bin
else
  echo "[ERRO] mklittlefs nao encontrado!"
  exit 1
fi

echo "=== Pipeline concluido com sucesso ==="
