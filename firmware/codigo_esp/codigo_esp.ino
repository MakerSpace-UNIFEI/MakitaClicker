#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <math.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

const int NUM_UPGRADES = 24;

struct UpgradeConfig {
  const char* id;
  double baseCost;
  float growth;
  double mps;
};

// ===== VERSÃO LOCAL — gerenciado automaticamente pelo build.sh =====
// NÃO edite manualmente. O Cloudflare Pages injeta o valor correto antes de compilar.
#define CURRENT_FIRMWARE_VER 0

#define MEGA_RESET_PIN D5

const char* VERSION_URL = "https://makitaclicker.pages.dev/version.json";
const char* STATE_URL   = "https://makitaclicker.pages.dev/api/state";
const char* GAMESTATE_FILE = "/gamestate.json";

const char* ssid = "MakerSpace UNIFEI";
const char* password = "makerspace@23";

#define GAME_BAUD_RATE 38400

// D6 = RX (Mega TX0 / Pino 1), D7 = TX (Mega RX0 / Pino 0)
SoftwareSerial megaSerial(D6, D7);

// Reinicia o Arduino Mega via pulso LOW em modo Open-Drain seguro
void resetMega() {
  Serial.println("[MEGA] Reiniciando Arduino Mega...");
  pinMode(MEGA_RESET_PIN, OUTPUT);
  digitalWrite(MEGA_RESET_PIN, LOW);
  delay(60);
  pinMode(MEGA_RESET_PIN, INPUT); // Retorna imediatamente para Hi-Z (alta impedancia)
  delay(1200); // Aguarda inicializacao do Mega
  Serial.println("[MEGA] Reset concluido.");
}

// ===== ESTADO DO JOGO =====
double makitas = 0.0;
const int MAX_OWNED = 100;
int pendingPhysicalClicks = 0;

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

// Cache de MPS e Poder de Clique para evitar chamadas pesadas no loop
double cachedMps = 0.0;
double cachedClickPower = 1.0;

void recalculateStats() {
  // Poder de clique
  double power = 1.0;
  if (permDiscoDiamante) power += 1.0;
  if (permTitanio) power += 3.0;
  if (permPlasmaCutter) power += 25.0;
  if (permLaserGama) power += 200.0;
  if (permSingularidade) power *= 3.0;
  if (permHiperClique) power *= 10.0;
  cachedClickPower = power;

  // MPS base das oficinas
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

  cachedMps = baseMps * multiplier;
}

double getClickPower() {
  return cachedClickPower;
}

double getTotalMps() {
  return cachedMps;
}

void notifyMega() {
  int totalOwned = 0;
  for (int i = 0; i < NUM_UPGRADES; i++) {
    totalOwned += ownedUpgrades[i];
  }
  String payload = "MAKITA:" + String(makitas, 1) + "," + String(cachedMps, 1) + "," + String(cachedClickPower, 1) + "," + String(totalOwned);
  megaSerial.println(payload);
  megaSerial.flush();
}

void handleClick() {
  double gain = cachedClickPower;
  if (permOnipotenciaMaker) {
    gain += (cachedMps * 0.30);
  } else if (permSinergiaQuantica) {
    gain += (cachedMps * 0.20);
  } else if (permOverclock) {
    gain += (cachedMps * 0.10);
  } else if (permEmpunhadura) {
    gain += (cachedMps * 0.05);
  }

  makitas += gain;
  pendingPhysicalClicks++;
  notifyMega(); // Atualização instantânea no display LCD (0ms)
}

// ===== PERSISTÊNCIA LOCAL (LITTLEFS) =====
void loadLocalGameState() {
  if (!LittleFS.begin()) {
    Serial.println(F("[FS] Falha ao montar LittleFS"));
    recalculateStats();
    return;
  }

  if (!LittleFS.exists(GAMESTATE_FILE)) {
    Serial.println(F("[FS] Nenhum save local encontrado. Iniciando estado padrao."));
    recalculateStats();
    return;
  }

  File f = LittleFS.open(GAMESTATE_FILE, "r");
  if (!f) {
    Serial.println(F("[FS] Erro ao abrir gamestate.json"));
    recalculateStats();
    return;
  }

#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  DynamicJsonDocument doc(2048);
#endif
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.printf("[FS] Erro parse save local: %s\n", err.c_str());
    recalculateStats();
    return;
  }

  if (doc.containsKey("makitas")) makitas = doc["makitas"].as<double>();
  if (doc.containsKey("owned")) {
    JsonObject ownedObj = doc["owned"].as<JsonObject>();
    for (int i = 0; i < NUM_UPGRADES; i++) {
      if (ownedObj.containsKey(UPGRADE_CONFIGS[i].id)) {
        ownedUpgrades[i] = ownedObj[UPGRADE_CONFIGS[i].id].as<int>();
      }
    }
  }

  if (doc.containsKey("perms")) {
    JsonObject permsObj = doc["perms"].as<JsonObject>();
    permLubrificante = permsObj["perm_lubrificante"] | false;
    permDiscoDiamante = permsObj["perm_disco_diamante"] | false;
    permMotorBrushless = permsObj["perm_motor_brushless"] | false;
    permEmpunhadura = permsObj["perm_empunhadura"] | false;
    permBateriaLitio = permsObj["perm_bateria_litio"] | false;
    permIaMaker = permsObj["perm_ia_maker"] | false;
    permRefrigeracao = permsObj["perm_refrigeracao"] | false;
    permTitanio = permsObj["perm_titanio"] | false;
    permOverclock = permsObj["perm_overclock"] | false;
    permNanobots = permsObj["perm_nanobots"] | false;
    permSingularidade = permsObj["perm_singularidade"] | false;
    permPlasmaCutter = permsObj["perm_plasma_cutter"] | false;
    permFusaoFria = permsObj["perm_fusao_fria"] | false;
    permHiperconducao = permsObj["perm_hiperconducao"] | false;
    permSinergiaQuantica = permsObj["perm_sinergia_quantica"] | false;
    permLaserGama = permsObj["perm_laser_gama"] | false;
    permTaquions = permsObj["perm_taquions"] | false;
    permMateriaEscura = permsObj["perm_materia_escura"] | false;
    permHiperClique = permsObj["perm_hiper_clique"] | false;
    permOnipotenciaMaker = permsObj["perm_onipotencia_maker"] | false;
  }

  recalculateStats();
  Serial.printf("[FS] Save local carregado! Saldo: %.1f | MPS: %.1f\n", makitas, cachedMps);
}

void saveLocalGameState() {
  File f = LittleFS.open(GAMESTATE_FILE, "w");
  if (!f) return;

#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  DynamicJsonDocument doc(2048);
#endif
  doc["makitas"] = makitas;

  JsonObject ownedObj = doc["owned"].to<JsonObject>();
  for (int i = 0; i < NUM_UPGRADES; i++) {
    ownedObj[UPGRADE_CONFIGS[i].id] = ownedUpgrades[i];
  }

  JsonObject permsObj = doc["perms"].to<JsonObject>();
  permsObj["perm_lubrificante"] = permLubrificante;
  permsObj["perm_disco_diamante"] = permDiscoDiamante;
  permsObj["perm_motor_brushless"] = permMotorBrushless;
  permsObj["perm_empunhadura"] = permEmpunhadura;
  permsObj["perm_bateria_litio"] = permBateriaLitio;
  permsObj["perm_ia_maker"] = permIaMaker;
  permsObj["perm_refrigeracao"] = permRefrigeracao;
  permsObj["perm_titanio"] = permTitanio;
  permsObj["perm_overclock"] = permOverclock;
  permsObj["perm_nanobots"] = permNanobots;
  permsObj["perm_singularidade"] = permSingularidade;
  permsObj["perm_plasma_cutter"] = permPlasmaCutter;
  permsObj["perm_fusao_fria"] = permFusaoFria;
  permsObj["perm_hiperconducao"] = permHiperconducao;
  permsObj["perm_sinergia_quantica"] = permSinergiaQuantica;
  permsObj["perm_laser_gama"] = permLaserGama;
  permsObj["perm_taquions"] = permTaquions;
  permsObj["perm_materia_escura"] = permMateriaEscura;
  permsObj["perm_hiper_clique"] = permHiperClique;
  permsObj["perm_onipotencia_maker"] = permOnipotenciaMaker;

  serializeJson(doc, f);
  f.close();
}

// ===== SINCRONIZAÇÃO COM A NUVEM (ESP = RECEIVER, SERVIDOR = MASTER) =====
// Regra: servidor/web é sempre master para upgrades (owned/perms).
// A ESP só envia makitas se for maior que o servidor, e os cliques pendentes.
// Tudo mais (owned, perms, MPS) é sempre recebido e aplicado a partir do servidor.
void syncWithCloud() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // Economia de RAM no ESP8266

  HTTPClient http;
  http.begin(client, STATE_URL);
  http.addHeader("Content-Type", "application/json");

  int clicksToSend = pendingPhysicalClicks;

  // ESP envia apenas: source, makitas (para comparação), clicks pendentes.
  // owned e perms NÃO são enviados — o servidor é master absoluto para upgrades.
#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument reqDoc;
#else
  DynamicJsonDocument reqDoc(256);
#endif
  reqDoc["action"] = "sync";
  reqDoc["source"] = "esp";          // Identifica origem como ESP
  reqDoc["clicks"] = clicksToSend;
  reqDoc["makitas"] = makitas;       // Servidor aceita SOMENTE se for maior

  String reqBody;
  serializeJson(reqDoc, reqBody);

  int httpCode = http.POST(reqBody);

  if (httpCode == HTTP_CODE_OK) {
    String responsePayload = http.getString();
#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    DynamicJsonDocument doc(2048);
#endif
    DeserializationError err = deserializeJson(doc, responsePayload);

    if (!err) {
      // Servidor é MASTER: sempre aplica o estado recebido
      if (doc.containsKey("makitas")) {
        makitas = doc["makitas"].as<double>();
      }

      // Desconta cliques confirmados
      pendingPhysicalClicks -= clicksToSend;
      if (pendingPhysicalClicks < 0) pendingPhysicalClicks = 0;

      // Sempre aplica owned do servidor (master absoluto)
      if (doc.containsKey("owned")) {
        JsonObject ownedObj = doc["owned"].as<JsonObject>();
        for (int i = 0; i < NUM_UPGRADES; i++) {
          if (ownedObj.containsKey(UPGRADE_CONFIGS[i].id)) {
            ownedUpgrades[i] = ownedObj[UPGRADE_CONFIGS[i].id].as<int>();
          }
        }
      }

      // Sempre aplica perms do servidor (master absoluto)
      if (doc.containsKey("perms")) {
        JsonObject permsObj = doc["perms"].as<JsonObject>();
        permLubrificante = permsObj["perm_lubrificante"] | false;
        permDiscoDiamante = permsObj["perm_disco_diamante"] | false;
        permMotorBrushless = permsObj["perm_motor_brushless"] | false;
        permEmpunhadura = permsObj["perm_empunhadura"] | false;
        permBateriaLitio = permsObj["perm_bateria_litio"] | false;
        permIaMaker = permsObj["perm_ia_maker"] | false;
        permRefrigeracao = permsObj["perm_refrigeracao"] | false;
        permTitanio = permsObj["perm_titanio"] | false;
        permOverclock = permsObj["perm_overclock"] | false;
        permNanobots = permsObj["perm_nanobots"] | false;
        permSingularidade = permsObj["perm_singularidade"] | false;
        permPlasmaCutter = permsObj["perm_plasma_cutter"] | false;
        permFusaoFria = permsObj["perm_fusao_fria"] | false;
        permHiperconducao = permsObj["perm_hiperconducao"] | false;
        permSinergiaQuantica = permsObj["perm_sinergia_quantica"] | false;
        permLaserGama = permsObj["perm_laser_gama"] | false;
        permTaquions = permsObj["perm_taquions"] | false;
        permMateriaEscura = permsObj["perm_materia_escura"] | false;
        permHiperClique = permsObj["perm_hiper_clique"] | false;
        permOnipotenciaMaker = permsObj["perm_onipotencia_maker"] | false;
      }

      recalculateStats();
      saveLocalGameState();
      notifyMega();
      Serial.printf("[CLOUD] Sync OK! Saldo: %.1f | MPS: %.1f\n", makitas, cachedMps);
    } else {
      Serial.printf("[CLOUD] Erro parse JSON: %s\n", err.c_str());
    }
  } else {
    Serial.printf("[CLOUD] Falha HTTP: %d\n", httpCode);
  }

  http.end();
  client.stop();
}

void checkOTA() {
  Serial.println("[OTA] Verificando atualizacoes...");

  WiFiClientSecure client;
  client.setInsecure();

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
  client.stop();

#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  StaticJsonDocument<384> doc;
#endif
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[OTA] JSON invalido: %s\n", err.c_str());
    return;
  }

  String fwUrl   = doc["firmware_url"] | "";
  int remoteFwVer = doc["firmware_version"] | 0;

  Serial.printf("[OTA] Local FW=%d | Remoto FW=%d\n", CURRENT_FIRMWARE_VER, remoteFwVer);

  // Atualiza Firmware do ESP se houver versão mais recente
  if (remoteFwVer > CURRENT_FIRMWARE_VER && fwUrl.length() > 0) {
    client.stop();
    Serial.println("[OTA] Atualizando Firmware...");
    ESPhttpUpdate.rebootOnUpdate(true);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, fwUrl);
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
  system_update_cpu_freq(160); // 160MHz para estabilidade de temporização
  Serial.begin(115200);
  megaSerial.begin(GAME_BAUD_RATE);

  // Inicializa o pino de reset do Mega em modo Open-Drain (alta impedancia)
  pinMode(MEGA_RESET_PIN, INPUT);

  // Carrega save da flash LittleFS imediatamente
  loadLocalGameState();

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
    checkOTA();
  } else {
    Serial.println("[WiFi] Nao conectado (modo offline)");
  }

  // Reinicia o Arduino Mega para sincronizar inicializacao e LCD
  resetMega();

  // Envia endereço Web para a Linha 3 do LCD do Arduino Mega
  megaSerial.println("WEB:pages.dev");
  megaSerial.flush();

  // Sincronização inicial com o Cloudflare KV
  syncWithCloud();
}

unsigned long lastTick = 0;
unsigned long lastMegaUpdate = 0;
unsigned long lastCloudSync = 0;
unsigned long lastLocalSave = 0;
const unsigned long CLOUD_SYNC_INTERVAL_MS = 10000; // Sincronização a cada 10 segundos
const unsigned long LOCAL_SAVE_INTERVAL_MS = 10000; // Autosave na flash a cada 10 segundos

void loop() {
  unsigned long now = millis();

  // 1. Recebe cliques do Arduino Mega de forma 100% não-bloqueante
  processarSerialMega();

  // 2. Produção passiva local contínua entre ciclos de sync (usando cache)
  if (now - lastTick >= 100) {
    float dt = (now - lastTick) / 1000.0;
    lastTick = now;
    if (cachedMps > 0) {
      makitas += (cachedMps * dt);
    }
  }

  // 3. Atualização contínua do LCD do Arduino Mega a cada 250ms
  if (now - lastMegaUpdate >= 250) {
    lastMegaUpdate = now;
    notifyMega();
  }

  // 4. Envia status de Web para o LCD apenas na mudança de status ou a cada 60 segundos
  static unsigned long lastUrlDisplay = 0;
  static bool lastWifiConnected = false;
  bool isConnected = (WiFi.status() == WL_CONNECTED);
  if (isConnected != lastWifiConnected || (now - lastUrlDisplay >= 60000)) {
    lastUrlDisplay = now;
    lastWifiConnected = isConnected;
    if (isConnected) {
      megaSerial.println("WEB:pages.dev");
    } else {
      megaSerial.println("WEB:Sem WiFi");
    }
  }

  // 5. Autosave na flash LittleFS a cada 15 segundos (proteção contra perda de energia)
  if (now - lastLocalSave >= LOCAL_SAVE_INTERVAL_MS) {
    lastLocalSave = now;
    saveLocalGameState();
  }

  // 6. Sincronização periódica com a Nuvem (Cloudflare Pages & KV) a cada 30 segundos
  if (now - lastCloudSync >= CLOUD_SYNC_INTERVAL_MS) {
    lastCloudSync = now;
    syncWithCloud();
  }
}