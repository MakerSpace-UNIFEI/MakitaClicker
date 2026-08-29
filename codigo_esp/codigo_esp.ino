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

const int NUM_UPGRADES = 13;

struct UpgradeConfig {
  const char* id;
  int baseCost;
  float growth;
  float mps;
};

#define EEPROM_MAGIC 0x4D4B5432 // "MKT2"

struct EEPROMState {
  uint32_t magic;
  double makitas;
  uint8_t owned[NUM_UPGRADES]; // 13 upgrades
  uint16_t perms; // bitmask para até 16 perms
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

// D6 = RX (Mega TX0 / Pino 1), D7 = TX (Mega RX0 / Pino 0)
SoftwareSerial megaSerial(D6, D7);

// Reinicia o Arduino Mega via pulso LOW em modo Open-Drain seguro
void resetMega() {
  Serial.println("[MEGA] Reiniciando Arduino Mega...");
  pinMode(MEGA_RESET_PIN, OUTPUT);
  digitalWrite(MEGA_RESET_PIN, LOW);
  delay(60);
  pinMode(MEGA_RESET_PIN, INPUT); // Retorna imediatamente para Hi-Z (alta impedancia)
  delay(150); // Aguarda boot do Mega
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

  // Aguarda resposta sincronizando no 0x1B inicial (descarta ruídos eventuais)
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (s.available() && s.peek() != 0x1B) s.read();
    if (s.available() >= 5 && s.peek() == 0x1B) break;
    delay(1);
    yield();
  }

  if (s.available() < 5 || s.read() != 0x1B) {
    Serial.printf("[STK-DBG] Timeout/No 0x1B (avail=%d)\n", s.available());
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

  HTTPClient http;
  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[OTA-MEGA] Falha HTTP: %d\n", httpCode);
    http.end();
    return false;
  }

  int totalBytes = http.getSize();
  Serial.printf("[OTA-MEGA] Tamanho: %d bytes\n", totalBytes);

  // Salva no LittleFS temporariamente para eliminar qualquer interferencia do Wi-Fi na Serial
  File f = LittleFS.open("/mega_temp.bin", "w");
  if (!f) {
    Serial.println("[OTA-MEGA] Erro ao abrir LittleFS para salvar temporario.");
    http.end();
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
      return false;
    }
    delay(1);
    yield();
  }
  f.close();
  http.end();
  Serial.printf("[OTA-MEGA] Download concluido: %d bytes salvos no LittleFS.\n", written);

  // Abre o arquivo local para gravacao estavel via Serial
  f = LittleFS.open("/mega_temp.bin", "r");
  if (!f) {
    Serial.println("[OTA-MEGA] Erro ao reabrir arquivo temporario.");
    return false;
  }

  // Pulsa o RESET do Mega para ativar o bootloader STK500v2
  pinMode(MEGA_RESET_PIN, OUTPUT);
  digitalWrite(MEGA_RESET_PIN, LOW);
  delay(100);
  digitalWrite(MEGA_RESET_PIN, HIGH);
  pinMode(MEGA_RESET_PIN, INPUT);

  megaSerial.begin(115200);
  while (megaSerial.available()) megaSerial.read();
  delay(150);

  uint8_t seq = 1;
  uint8_t resp[64];
  uint16_t respLen = 0;

  // 1. Handshake (CMD_SIGN_ON)
  bool syncOk = false;
  uint8_t signOnCmd[] = { 0x01 };
  for (int attempt = 0; attempt < 10; attempt++) {
    if (sendStk500v2(megaSerial, signOnCmd, sizeof(signOnCmd), resp, respLen, seq, 400)) {
      if (respLen >= 2 && resp[0] == 0x01 && resp[1] == 0x00) {
        syncOk = true;
        break;
      }
    }
    delay(50);
  }

  if (!syncOk) {
    Serial.println("[OTA-MEGA] Erro: Arduino Mega nao respondeu ao STK500v2.");
    f.close();
    LittleFS.remove("/mega_temp.bin");
    return false;
  }
  Serial.println("[OTA-MEGA] Handshake STK500v2 OK!");

  // 2. Enter Prog Mode
  uint8_t enterProg[] = { 0x10, 0xC8, 0x64, 0x19, 0x20, 0xAC, 0x53, 0x00, 0x00 };
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

    while (bytesRead < PAGE_SIZE) {
      pageBuffer[bytesRead++] = 0xFF;
    }

    bool pageOk = false;
    for (int retry = 0; retry < 5; retry++) {
      ESP.wdtFeed();
      // CMD_LOAD_ADDRESS (endereço em words de 16-bit)
      uint32_t wordAddr = currentAddr >> 1;
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
      f.close();
      LittleFS.remove("/mega_temp.bin");
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

  // Reinicia o Mega para rodar o novo código
  pinMode(MEGA_RESET_PIN, OUTPUT);
  digitalWrite(MEGA_RESET_PIN, LOW);
  delay(100);
  digitalWrite(MEGA_RESET_PIN, HIGH);
  pinMode(MEGA_RESET_PIN, INPUT);

  Serial.println("[OTA-MEGA] Arduino Mega regravado com sucesso!");
  return true;
}

// ===== ESTADO DO JOGO =====
double makitas = 0;
const int MAX_OWNED = 100;

const UpgradeConfig UPGRADE_CONFIGS[NUM_UPGRADES] = {
  { "upgrade1",    10,        1.10, 0.1 },  // +0.1 mps
  { "upgrade_1mps",   100,       1.12, 1.0 },  // +1 mps
  { "upgrade_2mps",   250,       1.12, 2.0 },  // +2 mps
  { "upgrade_5mps",   750,       1.13, 5.0 },  // +5 mps
  { "upgrade_10mps",  1800,      1.13, 10.0 }, // +10 mps
  { "upgrade_15mps",  3500,      1.14, 15.0 }, // +15 mps
  { "upgrade_20mps",  6000,      1.14, 20.0 }, // +20 mps
  { "upgrade_25mps",  10000,     1.14, 25.0 }, // +25 mps
  { "upgrade_30mps",  16000,     1.15, 30.0 }, // +30 mps
  { "upgrade_50mps",  35000,     1.15, 50.0 }, // +50 mps
  { "upgrade_100mps", 100000,    1.15, 100.0 },// +100 mps
  { "upgrade_200mps", 300000,    1.16, 200.0 },// +200 mps
  { "upgrade_500mps", 1000000,   1.16, 500.0 } // +500 mps
};

int ownedUpgrades[NUM_UPGRADES] = {0};

// Melhorias permanentes ativas
bool permLubrificante = false;   // +10% MPS global
bool permDiscoDiamante = false;  // +1.0 poder de clique
bool permMotorBrushless = false; // +100% (2x) ganho base das oficinas
bool permEmpunhadura = false;    // clique manual gera +5% do MPS atual
bool permBateriaLitio = false;   // +25% MPS global
bool permIaMaker = false;        // +50% MPS global
bool permRefrigeracao = false;   // +20% MPS global
bool permTitanio = false;        // +3.0 poder de clique
bool permOverclock = false;      // Dobra a sinergia de clique (+10% do MPS)
bool permNanobots = false;       // +75% MPS global
bool permSingularidade = false;  // +150% MPS global e triplica o clique base

unsigned long lastTick = 0;
unsigned long lastBroadcast = 0;

int getUpgradeIndex(const String &id) {
  for (int i = 0; i < NUM_UPGRADES; i++) {
    if (id == UPGRADE_CONFIGS[i].id) return i;
  }
  return -1;
}

int unitCost(int index, int count) {
  if (index < 0 || index >= NUM_UPGRADES) return 99999999;
  return ceil(UPGRADE_CONFIGS[index].baseCost * pow(UPGRADE_CONFIGS[index].growth, count));
}

float getClickPower() {
  float power = 1.0;
  if (permDiscoDiamante) power += 1.0;
  if (permTitanio) power += 3.0;
  if (permSingularidade) power *= 3.0;
  return power;
}

float getTotalMps() {
  float baseMps = 0.0;
  for (int i = 0; i < NUM_UPGRADES; i++) {
    baseMps += (ownedUpgrades[i] * UPGRADE_CONFIGS[i].mps);
  }
  if (permMotorBrushless) baseMps *= 2.0; // Dobra o ganho base das oficinas
  
  float multiplier = 1.0;
  if (permLubrificante) multiplier += 0.10;
  if (permRefrigeracao) multiplier += 0.20;
  if (permBateriaLitio) multiplier += 0.25;
  if (permIaMaker) multiplier += 0.50;
  if (permNanobots) multiplier += 0.75;
  if (permSingularidade) multiplier += 1.50;

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
  json += "\"perm_bateria_lítio\":" + String(permBateriaLitio ? "true" : "false") + ",";
  json += "\"perm_ia_maker\":" + String(permIaMaker ? "true" : "false") + ",";
  json += "\"perm_refrigeracao\":" + String(permRefrigeracao ? "true" : "false") + ",";
  json += "\"perm_titanio\":" + String(permTitanio ? "true" : "false") + ",";
  json += "\"perm_overclock\":" + String(permOverclock ? "true" : "false") + ",";
  json += "\"perm_nanobots\":" + String(permNanobots ? "true" : "false") + ",";
  json += "\"perm_singularidade\":" + String(permSingularidade ? "true" : "false");
  json += "}}";
  return json;
}

void notifyMega() {
  int totalOwned = 0;
  for (int i = 0; i < NUM_UPGRADES; i++) {
    totalOwned += ownedUpgrades[i];
  }
  String payload = "MAKITA:" + String((long)makitas) + "," + String(getTotalMps(), 1) + "," + String(getClickPower(), 1) + "," + String(totalOwned);
  megaSerial.println(payload);
}

void broadcastState() {
  String state = getGameStateJSON();
  webSocket.broadcastTXT(state);
  notifyMega();
}

void handleClick() {
  float gain = getClickPower();
  if (permOverclock) {
    gain += (getTotalMps() * 0.10);
  } else if (permEmpunhadura) {
    gain += (getTotalMps() * 0.05);
  }
  makitas += gain;
  broadcastState();
}

// Processa a compra considerando id, 1, 10 ou MAX
void processBuy(int index, String qtyStr) {
  if (index < 0 || index >= NUM_UPGRADES) return;

  int remaining = MAX_OWNED - ownedUpgrades[index];
  if (remaining <= 0) return;

  if (qtyStr == "max") {
    while (ownedUpgrades[index] < MAX_OWNED) {
      int cost = unitCost(index, ownedUpgrades[index]);
      if (makitas < cost) break;
      makitas -= cost;
      ownedUpgrades[index]++;
    }
  } else {
    int requested = qtyStr.toInt();
    int toBuy = min(requested, remaining);
    
    // Calcula custo total do lote
    int totalCost = 0;
    for (int i = 0; i < toBuy; i++) {
      totalCost += unitCost(index, ownedUpgrades[index] + i);
    }

    if (makitas >= totalCost && toBuy > 0) {
      makitas -= totalCost;
      ownedUpgrades[index] += toBuy;
    }
  }
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
  EEPROMState s;
  s.magic = EEPROM_MAGIC;
  s.makitas = makitas;
  for (int i = 0; i < NUM_UPGRADES; i++) {
    s.owned[i] = (uint8_t)ownedUpgrades[i];
  }
  s.perms = 0;
  if (permLubrificante)    s.perms |= (1 << 0);
  if (permDiscoDiamante)   s.perms |= (1 << 1);
  if (permMotorBrushless)  s.perms |= (1 << 2);
  if (permEmpunhadura)     s.perms |= (1 << 3);
  if (permBateriaLitio)    s.perms |= (1 << 4);
  if (permIaMaker)         s.perms |= (1 << 5);
  if (permRefrigeracao)    s.perms |= (1 << 6);
  if (permTitanio)         s.perms |= (1 << 7);
  if (permOverclock)       s.perms |= (1 << 8);
  if (permNanobots)        s.perms |= (1 << 9);
  if (permSingularidade)   s.perms |= (1 << 10);
  s.checksum = calcChecksum(s);

  EEPROM.put(0, s);
  EEPROM.commit();
}

bool loadEEPROM() {
  EEPROMState s;
  EEPROM.get(0, s);
  if (s.magic != EEPROM_MAGIC) return false;
  if (calcChecksum(s) != s.checksum) return false;

  makitas = s.makitas;
  for (int i = 0; i < NUM_UPGRADES; i++) {
    ownedUpgrades[i] = s.owned[i];
  }
  permLubrificante   = (s.perms & (1 << 0)) != 0;
  permDiscoDiamante  = (s.perms & (1 << 1)) != 0;
  permMotorBrushless = (s.perms & (1 << 2)) != 0;
  permEmpunhadura    = (s.perms & (1 << 3)) != 0;
  permBateriaLitio   = (s.perms & (1 << 4)) != 0;
  permIaMaker        = (s.perms & (1 << 5)) != 0;
  permRefrigeracao   = (s.perms & (1 << 6)) != 0;
  permTitanio        = (s.perms & (1 << 7)) != 0;
  permOverclock      = (s.perms & (1 << 8)) != 0;
  permNanobots       = (s.perms & (1 << 9)) != 0;
  permSingularidade  = (s.perms & (1 << 10)) != 0;
  return true;
}

void loadGameState() {
  bool loadedFromFS = false;
  if (LittleFS.exists("/gamestate.json")) {
    File f = LittleFS.open("/gamestate.json", "r");
    if (f) {
      StaticJsonDocument<1024> doc;
      DeserializationError err = deserializeJson(doc, f);
      f.close();

      if (!err) {
        makitas = doc["makitas"] | 0.0;
        
        JsonObject ownedObj = doc["owned"];
        if (!ownedObj.isNull()) {
          for (int i = 0; i < NUM_UPGRADES; i++) {
            ownedUpgrades[i] = ownedObj[UPGRADE_CONFIGS[i].id] | 0;
          }
        } else {
          // Compatibilidade retroativa com chave antiga
          ownedUpgrades[0] = doc["ownedUpgrade1"] | 0;
        }

        permLubrificante   = doc["permLubrificante"] | false;
        permDiscoDiamante  = doc["permDiscoDiamante"] | false;
        permMotorBrushless = doc["permMotorBrushless"] | false;
        permEmpunhadura    = doc["permEmpunhadura"] | false;
        permBateriaLitio   = doc["permBateriaLitio"] | false;
        permIaMaker        = doc["permIaMaker"] | false;
        permRefrigeracao   = doc["permRefrigeracao"] | false;
        permTitanio        = doc["permTitanio"] | false;
        permOverclock      = doc["permOverclock"] | false;
        permNanobots       = doc["permNanobots"] | false;
        permSingularidade  = doc["permSingularidade"] | false;
        loadedFromFS = true;
        Serial.printf("[STATE] Carregado do LittleFS: Makitas=%.1f\n", makitas);
      }
    }
  }

  if (!loadedFromFS) {
    if (loadEEPROM()) {
      Serial.printf("[STATE] Carregado do backup EEPROM: Makitas=%.1f\n", makitas);
      saveGameState(); // Restaura imediatamente para o LittleFS
    } else {
      Serial.println("[STATE] Nenhum estado anterior encontrado (inicio zerado).");
    }
  }
}

void saveGameState() {
  File f = LittleFS.open("/gamestate.json", "w");
  if (f) {
    StaticJsonDocument<1024> doc;
    doc["makitas"] = makitas;
    
    JsonObject ownedObj = doc.createNestedObject("owned");
    for (int i = 0; i < NUM_UPGRADES; i++) {
      ownedObj[UPGRADE_CONFIGS[i].id] = ownedUpgrades[i];
    }

    doc["permLubrificante"]   = permLubrificante;
    doc["permDiscoDiamante"]  = permDiscoDiamante;
    doc["permMotorBrushless"] = permMotorBrushless;
    doc["permEmpunhadura"]    = permEmpunhadura;
    doc["permBateriaLitio"]   = permBateriaLitio;
    doc["permIaMaker"]        = permIaMaker;
    doc["permRefrigeracao"]   = permRefrigeracao;
    doc["permTitanio"]        = permTitanio;
    doc["permOverclock"]      = permOverclock;
    doc["permNanobots"]       = permNanobots;
    doc["permSingularidade"]  = permSingularidade;

    serializeJson(doc, f);
    f.flush();
    f.close();
  }

  saveEEPROM();
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
  
  if (LittleFS.exists("/gamestate.json")) {
    LittleFS.remove("/gamestate.json");
  }
  saveGameState();
  broadcastState();
  Serial.println("[STATE] Progresso resetado com sucesso.");
}

void processPermBuy(String permId, int cost) {
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
  } else if ((permId == "perm_bateria_lítio" || permId == "perm_bateria_litio") && !permBateriaLitio && permMotorBrushless) {
    permBateriaLitio = true; bought = true;
  } else if (permId == "perm_ia_maker" && !permIaMaker && permBateriaLitio) {
    permIaMaker = true; bought = true;
  } else if (permId == "perm_refrigeracao" && !permRefrigeracao && permMotorBrushless) {
    permRefrigeracao = true; bought = true;
  } else if (permId == "perm_titanio" && !permTitanio && permDiscoDiamante) {
    permTitanio = true; bought = true;
  } else if (permId == "perm_overclock" && !permOverclock && permEmpunhadura && permRefrigeracao) {
    permOverclock = true; bought = true;
  } else if (permId == "perm_nanobots" && !permNanobots && permIaMaker) {
    permNanobots = true; bought = true;
  } else if (permId == "perm_singularidade" && !permSingularidade && permNanobots && permOverclock) {
    permSingularidade = true; bought = true;
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
          processBuy(upIndex, qtyStr);
          saveGameState();
        }
      }
    } else if (msg.startsWith("PERM_BUY:")) {
      // Formato: PERM_BUY:<id>:<custo>
      int firstSep = msg.indexOf(':', 9);
      if (firstSep != -1) {
        String permId = msg.substring(9, firstSep);
        int cost = msg.substring(firstSep + 1).toInt();
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
    Serial.println("[OTA] Atualizando Firmware...");
    ESPhttpUpdate.rebootOnUpdate(true);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, fwUrl);
    // Se chegou aqui, houve falha (sucesso causa reboot automatico)
    Serial.printf("[OTA] Falha no FW update: %s\n", ESPhttpUpdate.getLastErrorString().c_str());
  }
}

void setup() {
  system_update_cpu_freq(160); // 160MHz para máxima precisão de baud rate na SoftwareSerial
  Serial.begin(115200);
  megaSerial.begin(115200);

  // Inicializa o pino de reset do Mega em modo Open-Drain (alta impedancia)
  pinMode(MEGA_RESET_PIN, INPUT);

  EEPROM.begin(64);

  if (!LittleFS.begin()) {
    Serial.println("[FS] Erro ao montar LittleFS");
  }

  // Carrega estado imediatamente da flash/EEPROM antes de qualquer conexão ou OTA
  loadGameState();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());

  // Checa e aplica OTA antes de subir os serviços
  checkOTA();

  // Garante que o estado (mantido em RAM/EEPROM) seja regravado no LittleFS caso o FS tenha sido atualizado por OTA
  saveGameState();

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
}

unsigned long lastSave = 0;

void loop() {
  MDNS.update();
  server.handleClient();
  webSocket.loop();

  // Recebe cliques do Arduino Mega
  if (megaSerial.available()) {
    String cmd = megaSerial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "CLICK") {
      handleClick();
    }
  }

  // Produção passiva
  unsigned long now = millis();
  if (now - lastTick >= 100) {
    float dt = (now - lastTick) / 1000.0;
    lastTick = now;
    if (getTotalMps() > 0) {
      makitas += (getTotalMps() * dt);
    }
  }

  if (now - lastBroadcast >= 500) {
    lastBroadcast = now;
    if (getTotalMps() > 0) {
      broadcastState();
    }
  }

  // Autosave a cada 5 segundos se houver saldo ou produção
  if (now - lastSave >= 5000) {
    lastSave = now;
    if (getTotalMps() > 0 || makitas > 0) {
      saveGameState();
    }
  }
}