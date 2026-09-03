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

// Animação e feedback visual
unsigned long ultimoClickVisual = 0;
const unsigned long duracaoFeedbackClick = 600;
unsigned long ultimoTickAnimacao = 0;
int frameAnimacao = 0;
unsigned long ultimoTickInfo = 0;
int modoInfoLinha3 = 0;
String webInfo = "makitaclicker.pages.dev";

// Caracteres Customizados na CGRAM do LCD (5x8 pixels)
byte iconBlade0[8] = { B00100, B01110, B11011, B00100, B00100, B11011, B01110, B00100 };
byte iconBlade1[8] = { B10001, B01110, B01010, B11111, B01010, B01110, B10001, B00000 };
byte iconCoin[8]   = { B01110, B10001, B10101, B10101, B10101, B10001, B01110, B00000 };
byte iconBolt[8]   = { B00010, B00100, B01000, B11111, B00010, B00100, B01000, B00000 };
byte iconFactory[8]= { B10010, B11011, B11011, B11011, B11111, B11111, B11111, B00000 };
byte iconSpark[8]  = { B00100, B10101, B01110, B11111, B01110, B10101, B00100, B00000 };
byte iconTrophy[8] = { B11111, B10101, B01110, B00100, B00100, B01110, B11111, B00000 };

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

// Double-buffering na linha 3 para economizar transmissões I2C
String prevLcdLine3 = "";

void printLinhaFormatada(int linha, String texto) {
  if (!lcd) return;
  while (texto.length() < 20) {
    texto += " ";
  }
  if (texto.length() > 20) {
    texto = texto.substring(0, 20);
  }
  if (linha == 3 && texto == prevLcdLine3) {
    return;
  }
  if (linha == 3) {
    prevLcdLine3 = texto;
  }
  lcd->setCursor(0, linha);
  lcd->print(texto);
}

void atualizarLCD() {
  if (!lcd) return;
  unsigned long now = millis();
  bool clickAtivo = (now - ultimoClickVisual < duracaoFeedbackClick);

  // Linha 0: Cabeçalho com Ícones de Disco Giratório / Faíscas
  uint8_t bladeChar = (frameAnimacao % 2 == 0) ? 0 : 1;
  static uint8_t prevBladeChar = 255;
  static bool prevClickAtivo0 = false;
  if (bladeChar != prevBladeChar || clickAtivo != prevClickAtivo0) {
    prevBladeChar = bladeChar;
    prevClickAtivo0 = clickAtivo;
    lcd->setCursor(0, 0);
    if (clickAtivo) {
      lcd->write((uint8_t)5); // iconSpark
      lcd->print(F(" MAKITA CLICKER "));
      lcd->write((uint8_t)5); // iconSpark
      lcd->print(F("  "));
    } else {
      lcd->write(bladeChar);
      lcd->print(F(" MAKITA CLICKER "));
      lcd->write(bladeChar);
      lcd->print(F("  "));
    }
  }

  // Linha 1: Saldo de Makitas
  bool is99B = (makitas >= 99000000000.0);
  String saldoStr = is99B 
      ? (" Saldo:" + formatarNumero(makitas) + " MKT!") 
      : (" Saldo:" + formatarNumero(makitas) + " MKT");
  while (saldoStr.length() < 19) saldoStr += " ";
  saldoStr = saldoStr.substring(0, 19);

  static String prevSaldoStr = "";
  static bool prevIs99B = false;
  if (saldoStr != prevSaldoStr || is99B != prevIs99B) {
    prevSaldoStr = saldoStr;
    prevIs99B = is99B;
    lcd->setCursor(0, 1);
    lcd->write(is99B ? (uint8_t)6 : (uint8_t)2);
    lcd->print(saldoStr);
  }

  // Linha 2: Poder de Clique e Taxa de Produção (MPS)
  String taxaStr = "+" + formatarNumero(cachedClickPower) + " | ";
  String mpsStr = " " + formatarNumero(cachedMps) + "/s";
  int espacoRestante = 20 - 1 - (int)taxaStr.length() - 1;
  if (espacoRestante > 0) {
    while ((int)mpsStr.length() < espacoRestante) mpsStr += " ";
    mpsStr = mpsStr.substring(0, espacoRestante);
  }

  static String prevTaxaStr = "";
  static String prevMpsStr = "";
  if (taxaStr != prevTaxaStr || mpsStr != prevMpsStr) {
    prevTaxaStr = taxaStr;
    prevMpsStr = mpsStr;
    lcd->setCursor(0, 2);
    lcd->write((uint8_t)3); // iconBolt
    lcd->print(taxaStr);
    lcd->write((uint8_t)4); // iconFactory
    lcd->print(mpsStr);
  }

  // Linha 3: Feedback de Clique ou Status Rotativo
  if (clickAtivo) {
    printLinhaFormatada(3, ">> CORTE EFETUADO! <<");
  } else {
    if (modoInfoLinha3 == 0) {
      printLinhaFormatada(3, "Web: " + webInfo);
    } else if (modoInfoLinha3 == 1) {
      if (makitas >= 99000000000.0) {
        printLinhaFormatada(3, "** META 99B FEITA! **");
      } else {
        float pct = (float)(makitas / 99000000000.0) * 100.0;
        if (pct < 0.01 && makitas > 0) {
          printLinhaFormatada(3, "Meta 99B: >0.01%");
        } else {
          printLinhaFormatada(3, "Meta 99B: " + String(pct, (pct < 10.0 ? 2 : 1)) + "%");
        }
      }
    } else if (modoInfoLinha3 == 2) {
      int totalOwned = 0;
      for (int i = 0; i < NUM_UPGRADES; i++) totalOwned += ownedUpgrades[i];
      printLinhaFormatada(3, "Oficinas: " + String(totalOwned) + " un.");
    } else {
      printLinhaFormatada(3, " MakerSpace UNIFEI  ");
    }
  }
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
  frameAnimacao = (frameAnimacao + 1) % 4;
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

// ===== SINCRONIZAÇÃO COM A NUVEM (ESP = RECEIVER, SERVIDOR = MASTER) =====
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

#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument reqDoc;
#else
  DynamicJsonDocument reqDoc(256);
#endif
  reqDoc["action"] = "sync";
  reqDoc["source"] = "esp";          // Identifica origem como ESP
  reqDoc["clicks"] = clicksToSend;
  reqDoc["makitas"] = makitas;       // Servidor aceita SOMENTE se for maior que o KV

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
      // Servidor é MASTER: a ESP sempre adota e exibe o saldo oficial do servidor
      if (doc.containsKey("makitas")) {
        makitas = doc["makitas"].as<double>();
      }

      // Desconta cliques confirmados
      pendingPhysicalClicks -= clicksToSend;
      if (pendingPhysicalClicks < 0) pendingPhysicalClicks = 0;

      // Guard contra KV stale: só aplica owned/perms do servidor se o totalOwned
      // recebido for >= ao totalOwned local (evita estado zerado sobrescrever upgrades reais)
      int serverTotalOwned = doc["totalOwned"] | -1;
      int localTotalOwned = 0;
      for (int i = 0; i < NUM_UPGRADES; i++) localTotalOwned += ownedUpgrades[i];

      if (serverTotalOwned < 0 || serverTotalOwned >= localTotalOwned) {
        // Aplica owned do servidor
        if (doc.containsKey("owned")) {
          JsonObject ownedObj = doc["owned"].as<JsonObject>();
          for (int i = 0; i < NUM_UPGRADES; i++) {
            if (ownedObj.containsKey(UPGRADE_CONFIGS[i].id)) {
              ownedUpgrades[i] = ownedObj[UPGRADE_CONFIGS[i].id].as<int>();
            }
          }
        }

        // Aplica perms do servidor
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
      } else {
        Serial.printf("[CLOUD] Guard: servidor totalOwned=%d < local=%d. owned/perms preservados.\n",
                      serverTotalOwned, localTotalOwned);
      }

      // Preserva cachedMps se recalculate retornar 0 mas havia upgrades locais
      double prevMps = cachedMps;
      recalculateStats();
      if (cachedMps == 0.0 && localTotalOwned > 0 && prevMps > 0.0) {
        cachedMps = prevMps; // KV stale guard: não zera MPS local
        Serial.printf("[CLOUD] Guard: MPS recalc=0, restaurando prevMps=%.1f\n", prevMps);
      }

      saveLocalGameState();
      precisaAtualizarLCD = true;
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

#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  DynamicJsonDocument doc(256);
#endif
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.println("[OTA] Erro parse version.json");
    return;
  }

  int remoteVersion = doc["firmware_version"] | 0;
  const char* fwUrl = doc["firmware_url"] | "";

  Serial.printf("[OTA] Local FW=%d | Remoto FW=%d\n", CURRENT_FIRMWARE_VER, remoteVersion);

  if (remoteVersion > CURRENT_FIRMWARE_VER && strlen(fwUrl) > 0) {
    if (lcd) {
      printLinhaFormatada(0, "====================");
      printLinhaFormatada(1, " ATUALIZANDO FIRMWARE");
      printLinhaFormatada(2, "      VIA OTA...    ");
      printLinhaFormatada(3, "  Por favor, aguarde ");
    }
    client.stop();
    Serial.println("[OTA] Atualizando Firmware...");
    ESPhttpUpdate.rebootOnUpdate(true);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, fwUrl);
    Serial.printf("[OTA] Falha no FW update: %s\n", ESPhttpUpdate.getLastErrorString().c_str());
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

  // Registra caracteres customizados na memória CGRAM do display LCD
  lcd->createChar(0, iconBlade0);
  lcd->createChar(1, iconBlade1);
  lcd->createChar(2, iconCoin);
  lcd->createChar(3, iconBolt);
  lcd->createChar(4, iconFactory);
  lcd->createChar(5, iconSpark);
  lcd->createChar(6, iconTrophy);

  // Tela de Inicialização
  printLinhaFormatada(0, "====================");
  printLinhaFormatada(1, "   MAKITA CLICKER   ");
  printLinhaFormatada(2, "  MakerSpace UNIFEI ");
  printLinhaFormatada(3, "   Edicao 99B v4.0  ");

  // Carrega save da flash LittleFS imediatamente
  loadLocalGameState();
  recalculateStats();

  delay(1000);

  printLinhaFormatada(3, "Conectando WiFi...");
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
    printLinhaFormatada(3, "WiFi: Conectado!");
    checkOTA();
  } else {
    Serial.println("[WiFi] Nao conectado (modo offline)");
    printLinhaFormatada(3, "Modo Offline");
  }

  // Sincronização inicial com o Cloudflare KV
  syncWithCloud();
  atualizarLCD();
}

unsigned long lastTick = 0;
unsigned long lastCloudSync = 0;
unsigned long lastLocalSave = 0;
const unsigned long CLOUD_SYNC_INTERVAL_MS = 5000;  // Sincronização a cada 5 segundos
const unsigned long LOCAL_SAVE_INTERVAL_MS = 15000; // Autosave na flash a cada 15 segundos

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

  // 3. Animação de rotação do disco e feedback visual de faíscas
  if (now - ultimoTickAnimacao >= 400) {
    ultimoTickAnimacao = now;
    if (cachedMps > 0 || (now - ultimoClickVisual < duracaoFeedbackClick)) {
      frameAnimacao = (frameAnimacao + 1) % 4;
      precisaAtualizarLCD = true;
    }
  }

  // 4. Rotação de informações da Linha 3 a cada 3.2s
  if (now - ultimoTickInfo >= 3200) {
    ultimoTickInfo = now;
    modoInfoLinha3 = (modoInfoLinha3 + 1) % 4;
    precisaAtualizarLCD = true;
  }

  // 5. Atualização não-bloqueante e cadenciada do Display LCD 20x4
  if (precisaAtualizarLCD && (now - ultimoUpdateLCD >= INTERVALO_UPDATE_LCD)) {
    precisaAtualizarLCD = false;
    ultimoUpdateLCD = now;
    atualizarLCD();
  }

  // 6. Autosave na flash LittleFS a cada 15 segundos (proteção contra perda de energia)
  if (now - lastLocalSave >= LOCAL_SAVE_INTERVAL_MS) {
    lastLocalSave = now;
    saveLocalGameState();
  }

  // 7. Sincronização periódica com a Nuvem (Cloudflare Pages & KV) a cada 5 segundos
  if (now - lastCloudSync >= CLOUD_SYNC_INTERVAL_MS) {
    lastCloudSync = now;
    syncWithCloud();
  }

  // 8. Verificação periódica de OTA a cada 5 minutos (auto-update sem necessidade de reset)
  static unsigned long lastOtaCheck = 0;
  if (now - lastOtaCheck >= 300000) {
    lastOtaCheck = now;
    if (WiFi.status() == WL_CONNECTED) {
      checkOTA();
    }
  }
}
