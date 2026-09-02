#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <WebSocketsServer.h>
#include <SoftwareSerial.h>
#include <math.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

const int NUM_UPGRADES = 24;

struct UpgradeConfig {
  const char* id;
  double baseCost;
  float growth;
  double mps;
};

#define EEPROM_MAGIC 0x4D4B5433 // "MKT3"

struct EEPROMState {
  uint32_t magic;
  double makitas;
  uint8_t owned[NUM_UPGRADES]; // 24 upgrades (24 bytes)
  uint32_t perms; // bitmask para até 32 perms (20 em uso)
  uint8_t checksum;
};

// ===== VERSÃO LOCAL — gerenciado automaticamente pelo build.sh =====
// NÃO edite manualmente. O Cloudflare Pages injeta o valor correto antes de compilar.
#define CURRENT_FIRMWARE_VER 0
#define CURRENT_FS_VER       0
#define CURRENT_MEGA_VER     0

#define MEGA_RESET_PIN D5

const char* VERSION_URL = "https://makitaclicker.pages.dev/version.json";

const char* ssid = "MakerSpace UNIFEI";
const char* password = "makerspace@23";
const char* hostName = "esp-painel";

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

#define GAME_BAUD_RATE 38400
#define BOOTLOADER_BAUD_RATE 115200

// D6 = RX (Mega TX0 / Pino 1), D7 = TX (Mega RX0 / Pino 0)
SoftwareSerial megaSerial(D6, D7);

// Reinicia o Arduino Mega via pulso LOW em modo Open-Drain seguro
void resetMega() {
  Serial.println("[MEGA] Reiniciando Arduino Mega...");
  pinMode(MEGA_RESET_PIN, OUTPUT);
  digitalWrite(MEGA_RESET_PIN, LOW);
  delay(60);
  pinMode(MEGA_RESET_PIN, INPUT); // Retorna imediatamente para Hi-Z (alta impedancia)
  delay(1500); // Aguarda bootloader e inicializacao do Mega
  Serial.println("[MEGA] Reset concluido.");
}

// ===== ROTINA STK500v2 PARA GRAVAÇÃO DO ARDUINO MEGA =====
bool sendStk500v2(Stream &s, const uint8_t *payload, uint16_t len, uint8_t *resp, uint16_t &respLen, uint8_t &seqNum, uint32_t timeoutMs = 2000) {
  // Limpa qualquer byte residual na serial antes de enviar
  while (s.available()) s.read();
  if (resp) memset(resp, 0xFF, 64);

  uint8_t header[5];
  header[0] = 0x1B; // MESSAGE_START
  header[1] = seqNum;
  header[2] = (len >> 8) & 0xFF;
  header[3] = len & 0xFF;
  header[4] = 0x0E; // TOKEN

  uint8_t checksum = 0;
  for (int i = 0; i < 5; i++) checksum ^= header[i];
  for (uint16_t i = 0; i < len; i++) checksum ^= payload[i];

  s.write(header, 5);
  s.write(payload, len);
  s.write(checksum);
  s.flush();

  // Aguarda resposta sincronizando no 0x1B inicial
  unsigned long start = millis();
  int discarded = 0;
  uint8_t discBuf[8];

  while (millis() - start < timeoutMs) {
    while (s.available() && s.peek() != 0x1B) {
      uint8_t b = s.read();
      if (discarded < (int)sizeof(discBuf)) discBuf[discarded] = b;
      discarded++;
    }
    if (s.available() >= 5 && s.peek() == 0x1B) break;
    delay(1);
    yield();
  }

  if (s.available() < 5 || s.read() != 0x1B) {
    int avail = s.available();
    Serial.printf("[STK-DBG] Timeout/No 0x1B (avail=%d disc=%d", avail, discarded);
    if (discarded > 0) {
      Serial.print(" disc=[");
      for (int i = 0; i < min(discarded, (int)sizeof(discBuf)); i++) Serial.printf("0x%02X ", discBuf[i]);
      Serial.print("]");
    }
    if (avail > 0) {
      Serial.print(" buf=[");
      int show = min(avail, 4);
      for (int i = 0; i < show; i++) Serial.printf("0x%02X ", s.read());
      Serial.print("]");
    }
    Serial.println(")");
    return false;
  }
  uint8_t rSeq = s.read();
  uint8_t rLenH = s.read();
  uint8_t rLenL = s.read();
  uint8_t rTok = s.read();
  if (rTok != 0x0E) {
    Serial.printf("[STK-DBG] Bad token: 0x%02X\n", rTok);
    return false;
  }

  uint16_t rLen = ((uint16_t)rLenH << 8) | rLenL;
  start = millis();
  while (s.available() < (rLen + 1)) {
    if (millis() - start > timeoutMs) {
      Serial.printf("[STK-DBG] Body timeout (avail=%d need=%d)\n", s.available(), rLen + 1);
      return false;
    }
    delay(1);
    yield();
  }

  uint8_t rCheck = 0x1B ^ rSeq ^ rLenH ^ rLenL ^ rTok;
  for (uint16_t i = 0; i < rLen; i++) {
    uint8_t b = s.read();
    if (resp && i < 64) resp[i] = b;
    rCheck ^= b;
  }
  uint8_t expCheck = s.read();
  seqNum++;

  if (rCheck != expCheck) {
    Serial.printf("[STK-DBG] Checksum mismatch: calc 0x%02X != exp 0x%02X\n", rCheck, expCheck);
    return false;
  }
  respLen = rLen;
  return true;
}

bool updateMega(WiFiClientSecure &client, const String &url) {
  Serial.println("[OTA-MEGA] Baixando binario do Arduino Mega...");

  client.stop(); // Libera buffers SSL e fecha conexões residuais
  HTTPClient http;
  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[OTA-MEGA] Falha HTTP: %d\n", httpCode);
    http.end();
    client.stop();
    return false;
  }

  int totalBytes = http.getSize();
  Serial.printf("[OTA-MEGA] Tamanho: %d bytes\n", totalBytes);

  // Salva no LittleFS temporariamente
  File f = LittleFS.open("/mega_temp.bin", "w");
  if (!f) {
    Serial.println("[OTA-MEGA] Erro ao abrir LittleFS para salvar temporario.");
    http.end();
    client.stop();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t tempBuf[512];
  int written = 0;
  unsigned long dlStart = millis();

  while (http.connected() && (written < totalBytes || totalBytes == -1)) {
    ESP.wdtFeed();
    yield();
    size_t size = stream->available();
    if (size) {
      int c = stream->readBytes(tempBuf, min(size, sizeof(tempBuf)));
      f.write(tempBuf, c);
      written += c;
      dlStart = millis();
    }
    if (written == totalBytes) break;
    if (millis() - dlStart > 8000) {
      Serial.println("[OTA-MEGA] Timeout no download HTTP.");
      f.close();
      LittleFS.remove("/mega_temp.bin");
      http.end();
      client.stop();
      return false;
    }
    delay(1);
    yield();
  }
  f.close();
  http.end();
  client.stop(); // Desconecta e libera RAM do BearSSL antes de gravar via Serial
  Serial.printf("[OTA-MEGA] Download concluido: %d bytes salvos no LittleFS.\n", written);

  // Abre o arquivo local para gravacao via Serial
  f = LittleFS.open("/mega_temp.bin", "r");
  if (!f) {
    Serial.println("[OTA-MEGA] Erro ao reabrir arquivo temporario.");
    return false;
  }

  // 1. Abre a serial a 115200 ANTES do reset para armar a recepção
  megaSerial.begin(BOOTLOADER_BAUD_RATE);
  delay(10);
  while (megaSerial.available()) megaSerial.read();

  // Pulso de reset no Mega
  digitalWrite(MEGA_RESET_PIN, LOW);
  pinMode(MEGA_RESET_PIN, OUTPUT);
  delay(100);
  pinMode(MEGA_RESET_PIN, INPUT); // Solta em Hi-Z
  delay(80);                      // 80ms: bootloader inicializa a UART0

  while (megaSerial.available()) megaSerial.read();

  // Handshake imediato (CMD_SIGN_ON) dentro da janela de escuta do bootloader
  bool syncOk = false;
  uint8_t signOnCmd[] = { 0x01 };
  uint16_t respLen = 0;
  uint8_t resp[64];
  uint8_t seq = 1;

  Serial.println("[OTA-MEGA] Tentando handshake STK500v2 @ 115200 baud...");
  for (int attempt = 0; attempt < 12 && !syncOk; attempt++) {
    ESP.wdtFeed();
    yield();
    if (sendStk500v2(megaSerial, signOnCmd, sizeof(signOnCmd), resp, respLen, seq, 150)) {
      if (respLen >= 2 && resp[0] == 0x01 && resp[1] == 0x00) {
        syncOk = true;
        Serial.println("[OTA-MEGA] Handshake STK500v2 OK @ 115200!");
        break;
      }
    }
    delay(20);
  }

  // Se não respondeu a 115200, reseta o Mega novamente e tenta 57600 baud (bootloader de clones)
  if (!syncOk) {
    Serial.println("[OTA-MEGA] Sem resposta a 115200. Novo reset para tentar 57600 baud...");
    megaSerial.begin(57600);
    delay(10);
    while (megaSerial.available()) megaSerial.read();

    digitalWrite(MEGA_RESET_PIN, LOW);
    pinMode(MEGA_RESET_PIN, OUTPUT);
    delay(100);
    pinMode(MEGA_RESET_PIN, INPUT);
    delay(80);
    while (megaSerial.available()) megaSerial.read();

    for (int attempt = 0; attempt < 12 && !syncOk; attempt++) {
      ESP.wdtFeed();
      yield();
      if (sendStk500v2(megaSerial, signOnCmd, sizeof(signOnCmd), resp, respLen, seq, 150)) {
        if (respLen >= 2 && resp[0] == 0x01 && resp[1] == 0x00) {
          syncOk = true;
          Serial.println("[OTA-MEGA] Handshake STK500v2 OK @ 57600!");
          break;
        }
      }
      delay(20);
    }
  }

  if (!syncOk) {
    Serial.println("[OTA-MEGA] Erro: Arduino Mega nao respondeu ao STK500v2.");
    f.close();
    LittleFS.remove("/mega_temp.bin");
    megaSerial.begin(GAME_BAUD_RATE);
    return false;
  }

  // 2. Enter Prog Mode (12 bytes padrão STK500v2)
  uint8_t enterProg[] = { 0x10, 0xC8, 0x64, 0x19, 0x20, 0x00, 0x53, 0x03, 0xAC, 0x53, 0x00, 0x00 };
  sendStk500v2(megaSerial, enterProg, sizeof(enterProg), resp, respLen, seq, 1000);

  // 3. Grava páginas de 256 bytes lendo do arquivo local
  const uint16_t PAGE_SIZE = 256;
  uint8_t pageBuffer[PAGE_SIZE];
  uint32_t currentAddr = 0;
  uint8_t progPayload[10 + PAGE_SIZE];

  while (currentAddr < (uint32_t)written) {
    ESP.wdtFeed();
    yield();

    uint16_t toRead = min((uint32_t)PAGE_SIZE, (uint32_t)(written - currentAddr));
    size_t bytesRead = f.read(pageBuffer, toRead);

    // Aborta se a leitura do LittleFS retornar menos bytes que o esperado (falha de I/O)
    if (bytesRead != toRead) {
      Serial.printf("[OTA-MEGA] Erro: leitura LittleFS incompleta (%d/%d bytes), abortando.\n", bytesRead, toRead);
      uint8_t leaveProgErr[] = { 0x11, 0x01, 0x01 };
      sendStk500v2(megaSerial, leaveProgErr, sizeof(leaveProgErr), resp, respLen, seq, 1000);
      f.close();
      LittleFS.remove("/mega_temp.bin");
      megaSerial.begin(GAME_BAUD_RATE);
      return false;
    }

    while (bytesRead < PAGE_SIZE) {
      pageBuffer[bytesRead++] = 0xFF;
    }

    bool pageOk = false;
    for (int retry = 0; retry < 5; retry++) {
      ESP.wdtFeed();
      // CMD_LOAD_ADDRESS (endereço de flash em words de 16-bit para ATmega2560)
      // Bit 31 é setado SOMENTE se o endereço ultrapassar 64K words (128KB)
      uint32_t wordAddr = currentAddr >> 1;
      if (wordAddr >= 0x10000) {
        wordAddr |= 0x80000000;
      }
      uint8_t loadAddrCmd[] = {
        0x06,
        (uint8_t)((wordAddr >> 24) & 0xFF),
        (uint8_t)((wordAddr >> 16) & 0xFF),
        (uint8_t)((wordAddr >> 8) & 0xFF),
        (uint8_t)(wordAddr & 0xFF)
      };

      if (!sendStk500v2(megaSerial, loadAddrCmd, sizeof(loadAddrCmd), resp, respLen, seq, 1000) || resp[1] != 0x00) {
        delay(15);
        continue;
      }

      // CMD_PROGRAM_FLASH_ISP
      progPayload[0] = 0x13;
      progPayload[1] = (PAGE_SIZE >> 8) & 0xFF;
      progPayload[2] = PAGE_SIZE & 0xFF;
      progPayload[3] = 0xC1;
      progPayload[4] = 0x0A;
      progPayload[5] = 0x40;
      progPayload[6] = 0x4C;
      progPayload[7] = 0x00;
      progPayload[8] = 0x00;
      progPayload[9] = 0x00;
      memcpy(&progPayload[10], pageBuffer, PAGE_SIZE);

      if (sendStk500v2(megaSerial, progPayload, sizeof(progPayload), resp, respLen, seq, 2000) && resp[1] == 0x00) {
        pageOk = true;
        break;
      }
      delay(20);
    }

    if (!pageOk) {
      Serial.printf("[OTA-MEGA] Erro gravando pagina em 0x%X (status 0x%02X)\n", currentAddr, resp[1]);
      uint8_t leaveProg[] = { 0x11, 0x01, 0x01 };
      sendStk500v2(megaSerial, leaveProg, sizeof(leaveProg), resp, respLen, seq, 1000);
      f.close();
      LittleFS.remove("/mega_temp.bin");
      megaSerial.begin(GAME_BAUD_RATE);
      return false;
    }

    currentAddr += toRead;
    if ((currentAddr % 1024) == 0 || currentAddr >= (uint32_t)written) {
      Serial.printf("[OTA-MEGA] Progresso: %d / %d bytes (%.0f%%)\n",
                    currentAddr, written, (float)currentAddr * 100.0 / written);
    }
    delay(5);
    yield();
  }

  // 4. Leave Prog Mode
  uint8_t leaveProg[] = { 0x11, 0x01, 0x01 };
  sendStk500v2(megaSerial, leaveProg, sizeof(leaveProg), resp, respLen, seq, 1000);

  f.close();
  LittleFS.remove("/mega_temp.bin");

  // Reinicia o Mega para rodar o novo código e aguarda boot completo
  pinMode(MEGA_RESET_PIN, OUTPUT);
  digitalWrite(MEGA_RESET_PIN, LOW);
  delay(100);
  pinMode(MEGA_RESET_PIN, INPUT);
  delay(2000);

  megaSerial.begin(GAME_BAUD_RATE);

  Serial.println("[OTA-MEGA] Arduino Mega regravado com sucesso!");
  return true;
}

// ===== ESTADO DO JOGO =====
double makitas = 0.0;
const int MAX_OWNED = 100;

const UpgradeConfig UPGRADE_CONFIGS[NUM_UPGRADES] = {
  { "upgrade1",          10.0,         1.10, 0.1 },        // +0.1 MPS
  { "upgrade_1mps",      100.0,        1.12, 1.0 },        // +1.0 MPS
  { "upgrade_2mps",      250.0,        1.12, 2.0 },        // +2.0 MPS
  { "upgrade_5mps",      750.0,        1.13, 5.0 },        // +5.0 MPS
  { "upgrade_10mps",     1800.0,       1.13, 10.0 },       // +10.0 MPS
  { "upgrade_15mps",     3500.0,       1.14, 15.0 },       // +15.0 MPS
  { "upgrade_20mps",     6000.0,       1.14, 20.0 },       // +20.0 MPS
  { "upgrade_25mps",     10000.0,      1.14, 25.0 },       // +25.0 MPS
  { "upgrade_30mps",     16000.0,      1.15, 30.0 },       // +30.0 MPS
  { "upgrade_50mps",     35000.0,      1.15, 50.0 },       // +50.0 MPS
  { "upgrade_100mps",    100000.0,     1.15, 100.0 },      // +100.0 MPS
  { "upgrade_200mps",    300000.0,     1.16, 200.0 },      // +200.0 MPS
  { "upgrade_500mps",    1000000.0,    1.16, 500.0 },      // +500.0 MPS
  { "upgrade_1200mps",   3500000.0,    1.16, 1200.0 },     // +1.2k MPS
  { "upgrade_3000mps",   12000000.0,   1.16, 3000.0 },     // +3.0k MPS
  { "upgrade_8000mps",   40000000.0,   1.17, 8000.0 },     // +8.0k MPS
  { "upgrade_20kmps",    150000000.0,  1.17, 20000.0 },    // +20k MPS
  { "upgrade_60kmps",    500000000.0,  1.17, 60000.0 },    // +60k MPS
  { "upgrade_180kmps",   1800000000.0, 1.17, 180000.0 },   // +180k MPS
  { "upgrade_500kmps",   6000000000.0, 1.18, 500000.0 },   // +500k MPS
  { "upgrade_1500kmps",  20000000000.0,1.18, 1500000.0 },  // +1.5M MPS
  { "upgrade_5000kmps",  60000000000.0,1.18, 5000000.0 },  // +5.0M MPS
  { "upgrade_15000kmps", 200000000000.0,1.19,15000000.0 }, // +15.0M MPS
  { "upgrade_50000kmps", 800000000000.0,1.19,50000000.0 }  // +50.0M MPS
};

int ownedUpgrades[NUM_UPGRADES] = {0};

// 20 Melhorias permanentes ativas (Skill Tree)
bool permLubrificante = false;      // (bit 0)  +10% MPS global
bool permDiscoDiamante = false;     // (bit 1)  +1.0 poder de clique
bool permMotorBrushless = false;    // (bit 2)  2x ganho base das oficinas
bool permEmpunhadura = false;       // (bit 3)  clique gera +5% do MPS atual
bool permBateriaLitio = false;      // (bit 4)  +25% MPS global
bool permIaMaker = false;           // (bit 5)  +50% MPS global
bool permRefrigeracao = false;      // (bit 6)  +20% MPS global
bool permTitanio = false;           // (bit 7)  +3.0 poder de clique
bool permOverclock = false;         // (bit 8)  sinergia de clique passa a +10% do MPS
bool permNanobots = false;          // (bit 9)  +75% MPS global
bool permSingularidade = false;     // (bit 10) +150% MPS global e triplica clique base
bool permPlasmaCutter = false;      // (bit 11) +25.0 poder de clique
bool permFusaoFria = false;         // (bit 12) +100% MPS global
bool permHiperconducao = false;     // (bit 13) 3x produção base das oficinas
bool permSinergiaQuantica = false;  // (bit 14) sinergia de clique passa a +20% do MPS
bool permLaserGama = false;         // (bit 15) +200.0 poder de clique
bool permTaquions = false;          // (bit 16) +200% MPS global
bool permMateriaEscura = false;     // (bit 17) +300% MPS global
bool permHiperClique = false;       // (bit 18) 10x multiplicador de poder de clique
bool permOnipotenciaMaker = false;  // (bit 19) +500% MPS global, +30% MPS/clique, 4x oficinas

unsigned long lastTick = 0;
unsigned long lastBroadcast = 0;

int getUpgradeIndex(const String &id) {
  for (int i = 0; i < NUM_UPGRADES; i++) {
    if (id == UPGRADE_CONFIGS[i].id) return i;
  }
  return -1;
}

double unitCost(int index, int count) {
  if (index < 0 || index >= NUM_UPGRADES) return 1e18;
  return ceil(UPGRADE_CONFIGS[index].baseCost * pow((double)UPGRADE_CONFIGS[index].growth, count));
}

double getClickPower() {
  double power = 1.0;
  if (permDiscoDiamante) power += 1.0;
  if (permTitanio) power += 3.0;
  if (permPlasmaCutter) power += 25.0;
  if (permLaserGama) power += 200.0;
  if (permSingularidade) power *= 3.0;
  if (permHiperClique) power *= 10.0;
  return power;
}

double getTotalMps() {
  double baseMps = 0.0;
  for (int i = 0; i < NUM_UPGRADES; i++) {
    baseMps += ((double)ownedUpgrades[i] * UPGRADE_CONFIGS[i].mps);
  }
  
  // Multiplicadores base de oficinas
  double workshopMultiplier = 1.0;
  if (permMotorBrushless) workshopMultiplier *= 2.0;
  if (permHiperconducao) workshopMultiplier *= 3.0;
  if (permOnipotenciaMaker) workshopMultiplier *= 4.0;
  baseMps *= workshopMultiplier;

  // Multiplicadores globais percentuais aditivos
  double multiplier = 1.0;
  if (permLubrificante) multiplier += 0.10;
  if (permRefrigeracao) multiplier += 0.20;
  if (permBateriaLitio) multiplier += 0.25;
  if (permIaMaker) multiplier += 0.50;
  if (permNanobots) multiplier += 0.75;
  if (permFusaoFria) multiplier += 1.00;
  if (permSingularidade) multiplier += 1.50;
  if (permTaquions) multiplier += 2.00;
  if (permMateriaEscura) multiplier += 3.00;
  if (permOnipotenciaMaker) multiplier += 5.00;

  return baseMps * multiplier;
}

// JSON compatível com o index.html
String getGameStateJSON() {
  String json = "{";
  json += "\"makitas\":" + String(makitas, 1) + ",";
  json += "\"mps\":" + String(getTotalMps(), 1) + ",";
  json += "\"clickPower\":" + String(getClickPower(), 1) + ",";
  json += "\"owned\":{";
  for (int i = 0; i < NUM_UPGRADES; i++) {
    json += "\"" + String(UPGRADE_CONFIGS[i].id) + "\":" + String(ownedUpgrades[i]);
    if (i < NUM_UPGRADES - 1) json += ",";
  }
  json += "},";
  json += "\"perms\":{";
  json += "\"perm_lubrificante\":" + String(permLubrificante ? "true" : "false") + ",";
  json += "\"perm_disco_diamante\":" + String(permDiscoDiamante ? "true" : "false") + ",";
  json += "\"perm_motor_brushless\":" + String(permMotorBrushless ? "true" : "false") + ",";
  json += "\"perm_empunhadura\":" + String(permEmpunhadura ? "true" : "false") + ",";
  json += "\"perm_bateria_litio\":" + String(permBateriaLitio ? "true" : "false") + ",";
  json += "\"perm_ia_maker\":" + String(permIaMaker ? "true" : "false") + ",";
  json += "\"perm_refrigeracao\":" + String(permRefrigeracao ? "true" : "false") + ",";
  json += "\"perm_titanio\":" + String(permTitanio ? "true" : "false") + ",";
  json += "\"perm_overclock\":" + String(permOverclock ? "true" : "false") + ",";
  json += "\"perm_nanobots\":" + String(permNanobots ? "true" : "false") + ",";
  json += "\"perm_singularidade\":" + String(permSingularidade ? "true" : "false") + ",";
  json += "\"perm_plasma_cutter\":" + String(permPlasmaCutter ? "true" : "false") + ",";
  json += "\"perm_fusao_fria\":" + String(permFusaoFria ? "true" : "false") + ",";
  json += "\"perm_hiperconducao\":" + String(permHiperconducao ? "true" : "false") + ",";
  json += "\"perm_sinergia_quantica\":" + String(permSinergiaQuantica ? "true" : "false") + ",";
  json += "\"perm_laser_gama\":" + String(permLaserGama ? "true" : "false") + ",";
  json += "\"perm_taquions\":" + String(permTaquions ? "true" : "false") + ",";
  json += "\"perm_materia_escura\":" + String(permMateriaEscura ? "true" : "false") + ",";
  json += "\"perm_hiper_clique\":" + String(permHiperClique ? "true" : "false") + ",";
  json += "\"perm_onipotencia_maker\":" + String(permOnipotenciaMaker ? "true" : "false");
  json += "}}";
  return json;
}

void notifyMega() {
  int totalOwned = 0;
  for (int i = 0; i < NUM_UPGRADES; i++) {
    totalOwned += ownedUpgrades[i];
  }
  String payload = "MAKITA:" + String(makitas, 1) + "," + String(getTotalMps(), 1) + "," + String(getClickPower(), 1) + "," + String(totalOwned);
  megaSerial.println(payload);
  megaSerial.flush();
}

void broadcastState() {
  String state = getGameStateJSON();
  webSocket.broadcastTXT(state);
  notifyMega();
}

void handleClick() {
  double gain = getClickPower();
  double currentMps = getTotalMps();
  if (permOnipotenciaMaker) {
    gain += (currentMps * 0.30);
  } else if (permSinergiaQuantica) {
    gain += (currentMps * 0.20);
  } else if (permOverclock) {
    gain += (currentMps * 0.10);
  } else if (permEmpunhadura) {
    gain += (currentMps * 0.05);
  }
  makitas += gain;
  notifyMega();
}

// Processa a compra considerando id, 1, 10 ou MAX
void processBuy(int index, String qtyStr) {
  if (index < 0 || index >= NUM_UPGRADES) return;

  int remaining = MAX_OWNED - ownedUpgrades[index];
  if (remaining <= 0) return;

  if (qtyStr == "max") {
    while (ownedUpgrades[index] < MAX_OWNED) {
      double cost = unitCost(index, ownedUpgrades[index]);
      if (makitas < cost) break;
      makitas -= cost;
      ownedUpgrades[index]++;
    }
  } else {
    int requested = qtyStr.toInt();
    int toBuy = min(requested, remaining);
    
    // Calcula custo total do lote
    double totalCost = 0;
    for (int i = 0; i < toBuy; i++) {
      totalCost += unitCost(index, ownedUpgrades[index] + i);
    }

    if (makitas >= totalCost && toBuy > 0) {
      makitas -= totalCost;
      ownedUpgrades[index] += toBuy;
    }
  }
  saveGameState();
  broadcastState();
}

uint8_t calcChecksum(const EEPROMState &s) {
  const uint8_t *p = (const uint8_t*)&s;
  uint8_t cs = 0;
  for (size_t i = 0; i < sizeof(EEPROMState) - 1; i++) {
    cs ^= p[i];
  }
  return cs;
}

void saveEEPROM() {
  // TODO: reimplementar persistência
}

bool loadEEPROM() {
  // TODO: reimplementar persistência
  return false;
}

void loadGameState() {
  // TODO: reimplementar persistência
}

void saveGameState() {
  // TODO: reimplementar persistência
}

void resetGameState() {
  makitas = 0.0;
  for (int i = 0; i < NUM_UPGRADES; i++) {
    ownedUpgrades[i] = 0;
  }
  permLubrificante = false;
  permDiscoDiamante = false;
  permMotorBrushless = false;
  permEmpunhadura = false;
  permBateriaLitio = false;
  permIaMaker = false;
  permRefrigeracao = false;
  permTitanio = false;
  permOverclock = false;
  permNanobots = false;
  permSingularidade = false;
  permPlasmaCutter = false;
  permFusaoFria = false;
  permHiperconducao = false;
  permSinergiaQuantica = false;
  permLaserGama = false;
  permTaquions = false;
  permMateriaEscura = false;
  permHiperClique = false;
  permOnipotenciaMaker = false;
  
  if (LittleFS.exists("/gamestate.json")) {
    LittleFS.remove("/gamestate.json");
  }
  
  // Limpa EEPROM
  EEPROMState s;
  memset(&s, 0, sizeof(EEPROMState));
  s.magic = EEPROM_MAGIC;
  s.makitas = 0.0;
  s.checksum = calcChecksum(s);
  EEPROM.put(0, s);
  EEPROM.commit();

  saveGameState();
  
  // Transmite JSON especial informando o reset explicito
  String resetJson = "{\"isReset\":true,\"makitas\":0.0,\"mps\":0.0,\"clickPower\":1.0,\"owned\":{},\"perms\":{}}";
  webSocket.broadcastTXT(resetJson);
  notifyMega();
  Serial.println("[STATE] Progresso resetado com sucesso.");
}

void processPermBuy(String permId, double cost) {
  if (makitas < cost) return;

  bool bought = false;
  if (permId == "perm_lubrificante" && !permLubrificante) {
    permLubrificante = true; bought = true;
  } else if (permId == "perm_disco_diamante" && !permDiscoDiamante && permLubrificante) {
    permDiscoDiamante = true; bought = true;
  } else if (permId == "perm_motor_brushless" && !permMotorBrushless && permLubrificante) {
    permMotorBrushless = true; bought = true;
  } else if (permId == "perm_empunhadura" && !permEmpunhadura && permDiscoDiamante) {
    permEmpunhadura = true; bought = true;
  } else if (permId == "perm_bateria_litio" && !permBateriaLitio && permMotorBrushless) {
    permBateriaLitio = true; bought = true;
  } else if (permId == "perm_ia_maker" && !permIaMaker && permBateriaLitio) {
    permIaMaker = true; bought = true;
  } else if (permId == "perm_refrigeracao" && !permRefrigeracao && permMotorBrushless) {
    permRefrigeracao = true; bought = true;
  } else if (permId == "perm_titanio" && !permTitanio && permDiscoDiamante) {
    permTitanio = true; bought = true;
  } else if (permId == "perm_overclock" && !permOverclock && permEmpunhadura) {
    permOverclock = true; bought = true;
  } else if (permId == "perm_nanobots" && !permNanobots && permIaMaker) {
    permNanobots = true; bought = true;
  } else if (permId == "perm_singularidade" && !permSingularidade && permNanobots) {
    permSingularidade = true; bought = true;
  } else if (permId == "perm_plasma_cutter" && !permPlasmaCutter && permTitanio) {
    permPlasmaCutter = true; bought = true;
  } else if (permId == "perm_fusao_fria" && !permFusaoFria && permSingularidade) {
    permFusaoFria = true; bought = true;
  } else if (permId == "perm_hiperconducao" && !permHiperconducao && permFusaoFria) {
    permHiperconducao = true; bought = true;
  } else if (permId == "perm_sinergia_quantica" && !permSinergiaQuantica && permOverclock) {
    permSinergiaQuantica = true; bought = true;
  } else if (permId == "perm_laser_gama" && !permLaserGama && permPlasmaCutter) {
    permLaserGama = true; bought = true;
  } else if (permId == "perm_taquions" && !permTaquions && permHiperconducao) {
    permTaquions = true; bought = true;
  } else if (permId == "perm_materia_escura" && !permMateriaEscura && permTaquions) {
    permMateriaEscura = true; bought = true;
  } else if (permId == "perm_hiper_clique" && !permHiperClique && permLaserGama) {
    permHiperClique = true; bought = true;
  } else if (permId == "perm_onipotencia_maker" && !permOnipotenciaMaker && permMateriaEscura) {
    permOnipotenciaMaker = true; bought = true;
  }

  if (bought) {
    makitas -= cost;
    saveGameState();
    broadcastState();
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    String state = getGameStateJSON();
    webSocket.sendTXT(num, state);
  } else if (type == WStype_TEXT) {
    String msg = String((char*)payload);
    
    if (msg == "CLICK") {
      handleClick();
    } else if (msg == "RESET") {
      resetGameState();
    } else if (msg.startsWith("BUY:")) {
      // Formato: BUY:<id>:<qty>
      int firstSep = msg.indexOf(':', 4);
      if (firstSep != -1) {
        String upId = msg.substring(4, firstSep);
        String qtyStr = msg.substring(firstSep + 1);
        int upIndex = getUpgradeIndex(upId);
        if (upIndex != -1) {
          processBuy(upIndex, qtyStr); // processBuy() já chama saveGameState() internamente
        }
      }
    } else if (msg.startsWith("PERM_BUY:")) {
      // Formato: PERM_BUY:<id>:<custo>
      int firstSep = msg.indexOf(':', 9);
      if (firstSep != -1) {
        String permId = msg.substring(9, firstSep);
        double cost = msg.substring(firstSep + 1).toDouble();
        processPermBuy(permId, cost);
      }
    }
  }
}

void checkOTA() {
  Serial.println("[OTA] Verificando atualizacoes...");

  WiFiClientSecure client;
  client.setInsecure(); // sem verificação de certificado (RAM limitada no ESP8266)

  HTTPClient http;
  String checkUrl = String(VERSION_URL) + "?t=" + String(millis());
  http.begin(client, checkUrl);
  http.addHeader("Cache-Control", "no-cache");
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[OTA] Falha ao buscar version.json: %d\n", httpCode);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();
  client.stop(); // Encerra conexão HTTP e libera memória SSL

  StaticJsonDocument<384> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[OTA] JSON invalido: %s\n", err.c_str());
    return;
  }

  String fwUrl   = doc["firmware_url"] | "";
  String fsUrl   = doc["fs_url"]       | "";
  String megaUrl = doc["mega_url"]     | "";
  int remoteFwVer   = doc["firmware_version"] | 0;
  int remoteFsVer   = doc["fs_version"]       | 0;
  int remoteMegaVer = doc["mega_version"]     | 0;

  Serial.printf("[OTA] Local FW=%d FS=%d MEGA=%d | Remoto FW=%d FS=%d MEGA=%d\n",
                CURRENT_FIRMWARE_VER, CURRENT_FS_VER, CURRENT_MEGA_VER,
                remoteFwVer, remoteFsVer, remoteMegaVer);

  // 1. Atualiza o Arduino Mega se houver nova versao
  if (remoteMegaVer > CURRENT_MEGA_VER && megaUrl.length() > 0) {
    updateMega(client, megaUrl);
  }

  // 2. Atualiza FS do ESP (sem reboot)
  if (remoteFsVer > CURRENT_FS_VER && fsUrl.length() > 0) {
    client.stop();
    Serial.println("[OTA] Atualizando LittleFS...");
    ESPhttpUpdate.rebootOnUpdate(false);
    t_httpUpdate_return ret = ESPhttpUpdate.updateFS(client, fsUrl);
    if (ret == HTTP_UPDATE_OK) {
      Serial.println("[OTA] LittleFS atualizado com sucesso.");
    } else {
      Serial.printf("[OTA] Falha no FS update: %s\n", ESPhttpUpdate.getLastErrorString().c_str());
    }
  }

  // 3. Atualiza Firmware do ESP (com reboot automatico)
  if (remoteFwVer > CURRENT_FIRMWARE_VER && fwUrl.length() > 0) {
    client.stop();
    Serial.println("[OTA] Atualizando Firmware...");
    ESPhttpUpdate.rebootOnUpdate(true);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, fwUrl);
    // Se chegou aqui, houve falha (sucesso causa reboot automatico)
    Serial.printf("[OTA] Falha no FW update: %s\n", ESPhttpUpdate.getLastErrorString().c_str());
  }
}

// Buffer Serial Não-Bloqueante para recepção do Arduino Mega
char megaRxBuf[32];
uint8_t megaRxIdx = 0;

void processarSerialMega() {
  while (megaSerial.available() > 0) {
    char c = (char)megaSerial.read();
    if (c == '\n' || c == '\r') {
      if (megaRxIdx > 0) {
        megaRxBuf[megaRxIdx] = '\0';
        if (strcmp(megaRxBuf, "CLICK") == 0) {
          handleClick();
        }
        megaRxIdx = 0;
      }
    } else if (megaRxIdx < sizeof(megaRxBuf) - 1) {
      if (c >= 32 && c <= 126) {
        megaRxBuf[megaRxIdx++] = c;
      }
    } else {
      megaRxIdx = 0;
    }
  }
}

void setup() {
  system_update_cpu_freq(160); // 160MHz para máxima precisão de baud rate na SoftwareSerial
  Serial.begin(115200);
  megaSerial.begin(GAME_BAUD_RATE);

  // Inicializa o pino de reset do Mega em modo Open-Drain (alta impedancia)
  pinMode(MEGA_RESET_PIN, INPUT);

  if (!LittleFS.begin()) {
    Serial.println("[FS] Erro ao montar LittleFS");
  }

  WiFi.begin(ssid, password);
  Serial.print("[WiFi] Conectando");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiStart < 10000)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] Conectado! IP: ");
    Serial.println(WiFi.localIP());
    // Checa e aplica OTA antes de subir os serviços se conectado
    checkOTA();
  } else {
    Serial.println("[WiFi] Nao conectado (operando em modo offline)");
  }

  // Reinicia o Arduino Mega para sincronizar inicializacao e LCD
  resetMega();

  if (MDNS.begin(hostName)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);
  }

  server.on("/", HTTP_GET, []() {
    if (LittleFS.exists("/index.html")) {
      File f = LittleFS.open("/index.html", "r");
      server.streamFile(f, "text/html");
      f.close();
    } else {
      server.send(404, "text/plain", "index.html nao encontrado");
    }
  });

  // Serve arquivos de imagem se existirem na flash
  server.onNotFound([]() {
    if (LittleFS.exists(server.uri())) {
      File f = LittleFS.open(server.uri(), "r");
      server.streamFile(f, "image/png");
      f.close();
    } else {
      server.send(404, "text/plain", "Arquivo nao encontrado");
    }
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  notifyMega();
  // Envia IP da ESP para o display LCD do Arduino Mega
  if (WiFi.status() == WL_CONNECTED) {
    megaSerial.println("IP:" + WiFi.localIP().toString());
  } else {
    megaSerial.println("IP:Sem WiFi");
  }
  megaSerial.flush();
}

unsigned long lastMegaUpdate = 0;

void loop() {
  MDNS.update();
  server.handleClient();
  webSocket.loop();

  // Recebe cliques do Arduino Mega de forma 100% não-bloqueante
  processarSerialMega();

  // Produção passiva autoritativa no ESP
  unsigned long now = millis();
  if (now - lastTick >= 100) {
    float dt = (now - lastTick) / 1000.0;
    lastTick = now;
    if (getTotalMps() > 0) {
      makitas += (getTotalMps() * dt);
    }
  }

  // Atualização de telemetria para o LCD do Arduino Mega a cada 250ms
  if (now - lastMegaUpdate >= 250) {
    lastMegaUpdate = now;
    notifyMega();
  }

  // Sincronização periódica suave com a Web a cada 1.5 segundos
  if (now - lastBroadcast >= 1500) {
    lastBroadcast = now;
    broadcastState();
  }

}