#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

// ===== CONFIGURAÇÃO DE HARDWARE (100% AUTÔNOMO NA ESP8266) =====
// I2C Display LCD 20x4: SDA = D2 (GPIO4), SCL = D1 (GPIO5), VCC = VV (5V), GND = GND
// Botão Físico: Pino D5 (GPIO14) com INPUT_PULLUP (fecha no GND ao pressionar)
const int PIN_BOTAO = D5;
const unsigned long DEBOUNCE_MS = 25; // Filtro de 25ms para microswitches mecânicos

// Ponteiro dinâmico para o LCD (permite auto-detecção de endereço I2C: 0x27, 0x3F, etc.)
LiquidCrystal_I2C* lcd = nullptr;

// Controle de atualização desacoplada do LCD (elimina latência I2C)
bool precisaAtualizarLCD = false;
unsigned long ultimoUpdateLCD = 0;
const unsigned long INTERVALO_UPDATE_LCD = 75; // ~13 FPS para não sobrecarregar I2C

// Controle do Botão Físico
bool estadoAnteriorBotao = HIGH;
unsigned long ultimoTempoBotao = 0;

// Feedback visual de clique
unsigned long ultimoClickVisual = 0;
const unsigned long duracaoFeedbackClick = 600;

// Status operacional da ESP (exibido na Linha 3 do LCD: Ativo, Offline, Conectando, Sincroniz., Apagando..., Reset OK!)
String statusAtual = "Iniciando";

// Double-buffering nas 4 linhas do LCD para eliminar flicker e latência I2C
String prevLcdLines[4] = {"", "", "", ""};

// Variáveis de controle de conexão Wi-Fi e reconexão infinita
unsigned long ultimoWifiRetry = 0;
bool wifiConectadoAnterior = false;
const unsigned long WIFI_RETRY_INTERVAL_MS = 20000; // Retry infinito a cada 20 segundos

const int NUM_UPGRADES = 24;

struct UpgradeConfig {
  const char* id;
  double baseCost;
  float growth;
  double mps;
};

// ===== VERSÃO LOCAL — gerenciado automaticamente pelo build.sh =====
#define CURRENT_FIRMWARE_VER 0

const char* VERSION_URL = "https://makitaclicker.pages.dev/version.json";
const char* STATE_URL   = "https://makitaclicker.pages.dev/api/state";
const char* GAMESTATE_FILE = "/gamestate.json";

const char* ssid = "MakerSpace UNIFEI";
const char* password = "makerspace@25";

// ===== ESTADO DO JOGO =====
double makitas = 0.0;
const int MAX_OWNED = 100;
int pendingPhysicalClicks = 0;

// Top Player do Ranking recebido da Nuvem
String topPlayerName = "";
double topPlayerMakitas = 0.0;

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

// Cache de MPS e Poder de Clique
double cachedMps = 0.0;
double cachedClickPower = 1.0;

// Auto-detecta endereço I2C do display LCD (0x27, 0x3F, etc.)
uint8_t detectarEnderecoI2C() {
  const uint8_t enderecosComuns[] = {0x27, 0x3F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E};
  for (uint8_t i = 0; i < sizeof(enderecosComuns); i++) {
    Wire.beginTransmission(enderecosComuns[i]);
    if (Wire.endTransmission() == 0) {
      return enderecosComuns[i];
    }
  }
  return 0x27; // Endereço padrão fallback
}

// Formatação inteligente e ultra compacta de números para o LCD 20x4
String formatarNumero(double num) {
  if (num < 0) return "0";
  if (num < 10.0) {
    if (fabs(num - (long)num) > 0.05) {
      return String(num, 1);
    }
    return String((long)(num + 0.5));
  } else if (num < 999.5) {
    return String((long)(num + 0.5));
  } else if (num < 999500.0) {
    double k = num / 1000.0;
    if (k < 9.995) return String(k, 2) + "k";
    if (k < 99.95) return String(k, 1) + "k";
    return String((long)(k + 0.5)) + "k";
  } else if (num < 999500000.0) {
    double m = num / 1000000.0;
    if (m < 9.995) return String(m, 2) + "M";
    if (m < 99.95) return String(m, 1) + "M";
    return String((long)(m + 0.5)) + "M";
  } else if (num < 999500000000.0) {
    double b = num / 1000000000.0;
    if (b < 9.995) return String(b, 2) + "B";
    if (b < 99.95) return String(b, 1) + "B";
    return String((long)(b + 0.5)) + "B";
  } else if (num < 999500000000000.0) {
    double t = num / 1000000000000.0;
    if (t < 9.995) return String(t, 2) + "T";
    if (t < 99.95) return String(t, 1) + "T";
    return String((long)(t + 0.5)) + "T";
  } else {
    double q = num / 1000000000000000.0;
    return String(q, (q < 9.995 ? 2 : 1)) + "Qa";
  }
}

void printLinhaFormatada(int linha, String texto) {
  if (!lcd || linha < 0 || linha >= 4) return;
  while (texto.length() < 20) {
    texto += " ";
  }
  if (texto.length() > 20) {
    texto = texto.substring(0, 20);
  }
  if (texto == prevLcdLines[linha]) {
    return;
  }
  prevLcdLines[linha] = texto;
  lcd->setCursor(0, linha);
  lcd->print(texto);
}

void atualizarLCD() {
  if (!lcd) return;
  unsigned long now = millis();
  bool clickAtivo = (now - ultimoClickVisual < duracaoFeedbackClick);

  // Linha 0: 1° Lugar / Nome do Líder
  String topStr;
  String nome = (topPlayerName.length() > 0) ? topPlayerName : "MakerSpace";
  if (topPlayerMakitas > 0) {
    String scoreStr = " (" + formatarNumero(topPlayerMakitas) + ")";
    int maxNomeLen = 20 - 4 - (int)scoreStr.length();
    if (maxNomeLen > 2 && (int)nome.length() > maxNomeLen) {
      nome = nome.substring(0, maxNomeLen);
    }
    topStr = "1o: " + nome + scoreStr;
  } else {
    topStr = "1o: " + nome;
  }
  printLinhaFormatada(0, topStr);

  // Linha 1: Quantidade Atual de Makitas
  String qtdStr;
  if (makitas >= 99000000000.0) {
    qtdStr = "Makitas: 99B (META!)";
  } else {
    qtdStr = "Makitas: " + formatarNumero(makitas) + " MKT";
  }
  printLinhaFormatada(1, qtdStr);

  // Linha 2: Produção Atual ou Feedback Visual de Corte
  if (clickAtivo) {
    printLinhaFormatada(2, ">> CORTE EFETUADO! <<");
  } else {
    String prodStr = "Prod: +" + formatarNumero(cachedMps) + "/s";
    String clkStr = "(+" + formatarNumero(cachedClickPower) + ")";
    int espaco = 20 - (int)prodStr.length();
    if (espaco >= (int)clkStr.length() + 1) {
      while ((int)prodStr.length() < 20 - (int)clkStr.length()) {
        prodStr += " ";
      }
      prodStr += clkStr;
    }
    printLinhaFormatada(2, prodStr);
  }

  // Linha 3: Status Operacional (Ativo, Offline, Conectando, Sincroniz., Apagando..., Reset OK!)
  printLinhaFormatada(3, "Status: " + statusAtual);
}

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

// Processamento Instantâneo de Clique Físico (0ms de latência)
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
  ultimoClickVisual = millis();
  precisaAtualizarLCD = true;
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

  File f = LittleFS.open(GAMESTATE_FILE, "w");
  if (!f) {
    Serial.println(F("[FS] Erro ao gravar gamestate.json"));
    return;
  }
  serializeJson(doc, f);
  f.close();
}

// Controle de Reset Remoto da ESP
long lastProcessedResetId = 0;
bool hasPendingResetAck = false;

// ===== SINCRONIZAÇÃO COM A NUVEM (ESP = RECEIVER, SERVIDOR = MASTER) =====
void syncWithCloud() {
  if (WiFi.status() != WL_CONNECTED) {
    statusAtual = "Offline";
    precisaAtualizarLCD = true;
    return;
  }

  statusAtual = "Sincroniz.";
  precisaAtualizarLCD = true;
  atualizarLCD();

  WiFiClientSecure client;
  client.setInsecure(); // Economia de RAM no ESP8266

  HTTPClient http;
  http.begin(client, STATE_URL);
  http.addHeader("Content-Type", "application/json");

  int clicksToSend = pendingPhysicalClicks;

#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument reqDoc;
#else
  DynamicJsonDocument reqDoc(512);
#endif
  reqDoc["action"] = "sync";
  reqDoc["source"] = "esp";          // Identifica origem como ESP
  reqDoc["clicks"] = clicksToSend;
  reqDoc["makitas"] = makitas;
  reqDoc["fwVersion"] = CURRENT_FIRMWARE_VER;
  reqDoc["ip"] = WiFi.localIP().toString();
  reqDoc["rssi"] = WiFi.RSSI();
  reqDoc["uptime"] = (unsigned long)(millis() / 1000);
  reqDoc["freeHeap"] = ESP.getFreeHeap();

  // Envia check de confirmação (ACK) se a limpeza foi executada
  if (hasPendingResetAck) {
    reqDoc["resetAck"] = true;
  }

  String reqBody;
  serializeJson(reqDoc, reqBody);

  int httpCode = http.POST(reqBody);

  if (httpCode == HTTP_CODE_OK) {
    String responsePayload = http.getString();
#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    DynamicJsonDocument doc(4096);
#endif
    DeserializationError err = deserializeJson(doc, responsePayload);

    if (!err) {
      // 0. TRATAMENTO DA ORDEM DE RESET LATENTE VINDA DA NUVEM:
      // A ordem permanece ativa no servidor até a ESP enviar o ACK
      bool resetOrder = doc["resetOrder"] | false;

      if (resetOrder) {
        Serial.println(F("[RESET] Ordem de reset latente recebida do servidor!"));
        statusAtual = "Apagando...";
        precisaAtualizarLCD = true;
        atualizarLCD();

        // Limpa todas as variáveis locais do jogo
        makitas = 0.0;
        pendingPhysicalClicks = 0;
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

        recalculateStats();
        saveLocalGameState();

        // Marca ACK pendente e notifica no LCD
        hasPendingResetAck = true;
        statusAtual = "Reset OK!";
        precisaAtualizarLCD = true;
        atualizarLCD();

        http.end();
        client.stop();

        // Envia o ACK imediatamente de volta ao servidor para desativar a ordem latente
        delay(150);
        syncWithCloud();
        return;
      }

      // Se a ordem de reset já foi desativada no servidor, o ACK foi recebido com sucesso!
      if (!resetOrder && hasPendingResetAck) {
        hasPendingResetAck = false;
        Serial.println(F("[RESET] Confirmacao de reset (ACK) validada pela nuvem!"));
      }

      // 1. Saldo Monotônico: adota se o servidor estiver com saldo maior
      if (doc.containsKey("makitas")) {
        double serverMakitas = doc["makitas"].as<double>();
        if (serverMakitas > makitas) {
          makitas = serverMakitas;
        }
      }

      // Desconta cliques confirmados
      pendingPhysicalClicks -= clicksToSend;
      if (pendingPhysicalClicks < 0) pendingPhysicalClicks = 0;

      // 2. Upgrades: NUNCA reduz. Mantém sempre o maior nível de cada oficina
      if (doc.containsKey("owned")) {
        JsonObject ownedObj = doc["owned"].as<JsonObject>();
        for (int i = 0; i < NUM_UPGRADES; i++) {
          if (ownedObj.containsKey(UPGRADE_CONFIGS[i].id)) {
            int serverVal = ownedObj[UPGRADE_CONFIGS[i].id].as<int>();
            if (serverVal > ownedUpgrades[i]) {
              ownedUpgrades[i] = serverVal;
            }
          }
        }
      }

      // 3. Tecnologias Permanentes: Ativa localmente qualquer tecnologia liberada na nuvem
      if (doc.containsKey("perms")) {
        JsonObject permsObj = doc["perms"].as<JsonObject>();
        if (permsObj["perm_lubrificante"] | false) permLubrificante = true;
        if (permsObj["perm_disco_diamante"] | false) permDiscoDiamante = true;
        if (permsObj["perm_motor_brushless"] | false) permMotorBrushless = true;
        if (permsObj["perm_empunhadura"] | false) permEmpunhadura = true;
        if (permsObj["perm_bateria_litio"] | false) permBateriaLitio = true;
        if (permsObj["perm_ia_maker"] | false) permIaMaker = true;
        if (permsObj["perm_refrigeracao"] | false) permRefrigeracao = true;
        if (permsObj["perm_titanio"] | false) permTitanio = true;
        if (permsObj["perm_overclock"] | false) permOverclock = true;
        if (permsObj["perm_nanobots"] | false) permNanobots = true;
        if (permsObj["perm_singularidade"] | false) permSingularidade = true;
        if (permsObj["perm_plasma_cutter"] | false) permPlasmaCutter = true;
        if (permsObj["perm_fusao_fria"] | false) permFusaoFria = true;
        if (permsObj["perm_hiperconducao"] | false) permHiperconducao = true;
        if (permsObj["perm_sinergia_quantica"] | false) permSinergiaQuantica = true;
        if (permsObj["perm_laser_gama"] | false) permLaserGama = true;
        if (permsObj["perm_taquions"] | false) permTaquions = true;
        if (permsObj["perm_materia_escura"] | false) permMateriaEscura = true;
        if (permsObj["perm_hiper_clique"] | false) permHiperClique = true;
        if (permsObj["perm_onipotencia_maker"] | false) permOnipotenciaMaker = true;
      }

      // 4. TELEMETRIA DO LÍDER DO RANKING (TOP PLAYER):
      if (doc["topPlayer"].is<JsonObject>()) {
        JsonObject topObj = doc["topPlayer"];
        const char* tName = topObj["name"] | "";
        if (strlen(tName) > 0) {
          topPlayerName = String(tName);
          topPlayerMakitas = topObj["makitas"] | 0.0;
        }
      }

      recalculateStats();
      saveLocalGameState();
      statusAtual = "Ativo";
      precisaAtualizarLCD = true;
      Serial.printf("[CLOUD] Sync OK! Saldo: %.1f | MPS: %.1f\n", makitas, cachedMps);
    } else {
      Serial.printf("[CLOUD] Erro parse JSON: %s\n", err.c_str());
      statusAtual = "Ativo";
      precisaAtualizarLCD = true;
    }
  } else {
    Serial.printf("[CLOUD] Falha HTTP: %d\n", httpCode);
    if (WiFi.status() == WL_CONNECTED) {
      statusAtual = "Ativo";
    } else {
      statusAtual = "Offline";
    }
    precisaAtualizarLCD = true;
  }

  http.end();
  client.stop();
}

void checkOTA() {
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.println(F("[OTA] Verificando atualizacoes..."));

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

#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  DynamicJsonDocument doc(256);
#endif
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.println(F("[OTA] Erro parse version.json"));
    return;
  }

  int remoteVersion = doc["firmware_version"] | 0;
  const char* fwUrl = doc["firmware_url"] | "";

  Serial.printf("[OTA] Local FW=%d | Remoto FW=%d\n", CURRENT_FIRMWARE_VER, remoteVersion);

  if (remoteVersion > CURRENT_FIRMWARE_VER && strlen(fwUrl) > 0) {
    if (lcd) {
      lcd->clear();
      for (int i = 0; i < 4; i++) prevLcdLines[i] = "";
      printLinhaFormatada(0, "====================");
      printLinhaFormatada(1, "  ATUALIZANDO OTA   ");
      printLinhaFormatada(2, " v" + String(CURRENT_FIRMWARE_VER) + " -> v" + String(remoteVersion));
      printLinhaFormatada(3, ">> BAIXANDO FW... <<");
    }
    client.stop();
    Serial.println(F("[OTA] Atualizando Firmware..."));
    ESPhttpUpdate.rebootOnUpdate(true);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, fwUrl);
    Serial.printf("[OTA] Falha no FW update: %s\n", ESPhttpUpdate.getLastErrorString().c_str());

    if (lcd) {
      printLinhaFormatada(1, "  FALHA NO OTA!     ");
      printLinhaFormatada(3, "Tentando depois...  ");
      delay(1500);
      for (int i = 0; i < 4; i++) prevLcdLines[i] = "";
    }
  }
}

void setup() {
  system_update_cpu_freq(160); // 160MHz para máxima velocidade e estabilidade
  Serial.begin(115200);

  // Configuração do Botão Físico
  pinMode(PIN_BOTAO, INPUT_PULLUP);

  // Inicialização do Barramento I2C nos pinos D2 (SDA) e D1 (SCL)
  Wire.begin(D2, D1);

  // Auto-detecta endereço e inicializa LCD 20x4
  uint8_t lcdAddr = detectarEnderecoI2C();
  lcd = new LiquidCrystal_I2C(lcdAddr, 20, 4);
  lcd->init();
  lcd->backlight();
  lcd->clear();

  // Tela de Inicialização com versão atual
  printLinhaFormatada(0, "====================");
  printLinhaFormatada(1, "   MAKITA CLICKER   ");
  printLinhaFormatada(2, "    Versao: v" + String(CURRENT_FIRMWARE_VER));
  printLinhaFormatada(3, "Iniciando sistema...");

  // Carrega save da flash LittleFS imediatamente
  loadLocalGameState();
  recalculateStats();

  delay(600);

  // Tentativa inicial de conexão Wi-Fi
  printLinhaFormatada(3, "Conectando WiFi...  ");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  Serial.print(F("[WiFi] Conectando"));
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiStart < 7000)) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConectadoAnterior = true;
    statusAtual = "Ativo";
    Serial.print(F("[WiFi] Conectado! IP: "));
    Serial.println(WiFi.localIP());
    printLinhaFormatada(3, "WiFi: Conectado!    ");
    delay(500);
    checkOTA();
    syncWithCloud();
  } else {
    wifiConectadoAnterior = false;
    statusAtual = "Offline";
    Serial.println(F("[WiFi] Falha inicial. Entrando em Modo Offline (retry 20s)"));
    printLinhaFormatada(3, "Modo Offline (20s)  ");
    delay(600);
  }

  // Limpa buffer de linhas para desenhar a Tela Principal
  for (int i = 0; i < 4; i++) prevLcdLines[i] = "";
  atualizarLCD();
}

unsigned long lastTick = 0;
unsigned long lastCloudSync = 0;
unsigned long lastLocalSave = 0;
const unsigned long CLOUD_SYNC_INTERVAL_MS = 5000;  // Sincronização a cada 5 segundos
const unsigned long LOCAL_SAVE_INTERVAL_MS = 15000; // Autosave na flash a cada 15 segundos

void gerenciarWiFi() {
  unsigned long now = millis();
  wl_status_t wifiStatus = WiFi.status();

  if (wifiStatus == WL_CONNECTED) {
    if (!wifiConectadoAnterior) {
      wifiConectadoAnterior = true;
      Serial.print(F("[WiFi] Reconectado com sucesso! IP: "));
      Serial.println(WiFi.localIP());
      statusAtual = "Ativo";
      precisaAtualizarLCD = true;
      checkOTA();
      syncWithCloud();
    }
  } else {
    if (wifiConectadoAnterior) {
      wifiConectadoAnterior = false;
      Serial.println(F("[WiFi] Conexao perdida. Modo Offline ativo."));
      statusAtual = "Offline";
      precisaAtualizarLCD = true;
    }

    // Tentativa periódica infinita a cada 20 segundos
    if (now - ultimoWifiRetry >= WIFI_RETRY_INTERVAL_MS) {
      ultimoWifiRetry = now;
      Serial.println(F("[WiFi] Tentando reconectar (retry 20s)..."));
      statusAtual = "Conectando";
      precisaAtualizarLCD = true;
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
  }
}

void loop() {
  unsigned long now = millis();

  // 1. Leitura do Botão Físico no Pino D5 (disparo instantâneo com INPUT_PULLUP)
  bool leitura = digitalRead(PIN_BOTAO);
  if (leitura == LOW && estadoAnteriorBotao == HIGH && (now - ultimoTempoBotao > DEBOUNCE_MS)) {
    ultimoTempoBotao = now;
    handleClick();
  }
  estadoAnteriorBotao = leitura;

  // 2. Produção passiva local contínua entre ciclos de sync
  if (now - lastTick >= 100) {
    float dt = (now - lastTick) / 1000.0;
    lastTick = now;
    if (cachedMps > 0) {
      makitas += (cachedMps * dt);
      precisaAtualizarLCD = true;
    }
  }

  // 3. Atualização não-bloqueante e cadenciada do Display LCD 20x4
  if (precisaAtualizarLCD && (now - ultimoUpdateLCD >= INTERVALO_UPDATE_LCD)) {
    precisaAtualizarLCD = false;
    ultimoUpdateLCD = now;
    atualizarLCD();
  }

  // 4. Gerenciamento de Wi-Fi Não-Bloqueante (Retry Infinito a cada 20s)
  gerenciarWiFi();

  // 5. Autosave na flash LittleFS a cada 15 segundos (proteção contra perda de energia)
  if (now - lastLocalSave >= LOCAL_SAVE_INTERVAL_MS) {
    lastLocalSave = now;
    saveLocalGameState();
  }

  // 6. Sincronização periódica com a Nuvem (Cloudflare Pages & KV) a cada 5 segundos
  if (now - lastCloudSync >= CLOUD_SYNC_INTERVAL_MS) {
    lastCloudSync = now;
    if (WiFi.status() == WL_CONNECTED) {
      syncWithCloud();
    }
  }

  // 7. Verificação periódica de OTA a cada 5 minutos
  static unsigned long lastOtaCheck = 0;
  if (now - lastOtaCheck >= 300000) {
    lastOtaCheck = now;
    if (WiFi.status() == WL_CONNECTED) {
      checkOTA();
    }
  }
}
