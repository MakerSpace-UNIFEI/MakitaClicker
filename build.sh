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

echo "[VERSION] codigo_esp.ino patchado:"
grep "CURRENT_.*_VER" codigo_esp/codigo_esp.ino

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
echo "[DEBUG] HOME=$HOME, buscando mklittlefs..."

MKLITTLEFS_BIN=$(find /opt/buildhome/.arduino15 /root/.arduino15 "$HOME/.arduino15" \
  -name mklittlefs -type f 2>/dev/null | head -n 1)

echo "[DEBUG] mklittlefs encontrado em: $MKLITTLEFS_BIN"

if [ -n "$MKLITTLEFS_BIN" ]; then
  # 2072576 bytes = 2MB de particao FS para Flash de 4MB
  "$MKLITTLEFS_BIN" -c "$SKETCH_DIR/data" -p 256 -b 8192 -s 2072576 ./online/littlefs.bin
else
  echo "[ERRO] mklittlefs nao encontrado! Listando .arduino15:"
  find /opt/buildhome/.arduino15 /root/.arduino15 "$HOME/.arduino15" \
    -name "mklittlefs*" 2>/dev/null || true
  exit 1
fi

# Gera o version.json com a mesma versão gravada no firmware
cat > ./online/version.json << VERSIONJSON
{
  "firmware_version": $VERSION,
  "fs_version": $VERSION,
  "firmware_url": "https://makitaclicker.pages.dev/firmware.bin",
  "fs_url": "https://makitaclicker.pages.dev/littlefs.bin"
}
VERSIONJSON

echo "[VERSION] version.json gerado:"
cat ./online/version.json

echo "=== Pipeline concluido com sucesso ==="
