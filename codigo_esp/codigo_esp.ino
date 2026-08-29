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

// ===== VERSÃO LOCAL (atualizar a cada release) =====
#define CURRENT_FIRMWARE_VER 2
#define CURRENT_FS_VER       3

const char* VERSION_URL = "https://makitaclicker.pages.dev/version.json";

const char* ssid = "MakerSpace UNIFEI";
const char* password = "makerspace@23";
const char* hostName = "esp-painel";

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// D6 = RX (Mega 18), D7 = TX (Mega 19)
SoftwareSerial megaSerial(D6, D7);

// ===== ESTADO DO JOGO =====
double makitas = 0;
int ownedUpgrade1 = 0;
const int MAX_OWNED = 100;
const int baseCostUpgrade1 = 10;
const float growthUpgrade1 = 1.10;
const float mpsUpgrade1 = 0.1;

unsigned long lastTick = 0;
unsigned long lastBroadcast = 0;

int unitCost(int count) {
  return ceil(baseCostUpgrade1 * pow(growthUpgrade1, count));
}

float getTotalMps() {
  return ownedUpgrade1 * mpsUpgrade1;
}

// JSON compatível com o novo index.html
String getGameStateJSON() {
  String json = "{";
  json += "\"makitas\":" + String(makitas, 1) + ",";
  json += "\"mps\":" + String(getTotalMps(), 1) + ",";
  json += "\"owned\":{\"upgrade1\":" + String(ownedUpgrade1) + "}";
  json += "}";
  return json;
}

void notifyMega() {
  String payload = "MAKITA:" + String((long)makitas) + "," + String(getTotalMps(), 1);
  megaSerial.println(payload);
}

void broadcastState() {
  String state = getGameStateJSON();
  webSocket.broadcastTXT(state);
  notifyMega();
}

// Processa a compra considerando 1, 10 ou MAX
void processBuy(String qtyStr) {
  int remaining = MAX_OWNED - ownedUpgrade1;
  if (remaining <= 0) return;

  if (qtyStr == "max") {
    while (ownedUpgrade1 < MAX_OWNED) {
      int cost = unitCost(ownedUpgrade1);
      if (makitas < cost) break;
      makitas -= cost;
      ownedUpgrade1++;
    }
  } else {
    int requested = qtyStr.toInt();
    int toBuy = min(requested, remaining);
    
    // Calcula custo total do lote
    int totalCost = 0;
    for (int i = 0; i < toBuy; i++) {
      totalCost += unitCost(ownedUpgrade1 + i);
    }

    if (makitas >= totalCost && toBuy > 0) {
      makitas -= totalCost;
      ownedUpgrade1 += toBuy;
    }
  }
  broadcastState();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    String state = getGameStateJSON();
    webSocket.sendTXT(num, state);
  } else if (type == WStype_TEXT) {
    String msg = String((char*)payload);
    
    if (msg == "CLICK") {
      makitas += 1.0;
      broadcastState();
    } else if (msg.startsWith("BUY:upgrade1:")) {
      String qtyStr = msg.substring(13);
      processBuy(qtyStr);
    } else if (msg.startsWith("PERM_BUY:")) {
      // Formato: PERM_BUY:<id>:<custo>
      int firstSep = msg.indexOf(':', 9);
      if (firstSep != -1) {
        int cost = msg.substring(firstSep + 1).toInt();
        if (cost > 0 && makitas >= cost) {
          makitas -= cost;
          broadcastState();
        }
      }
    }
  }
}

void checkOTA() {
  Serial.println("[OTA] Verificando atualizacoes...");

  WiFiClientSecure client;
  client.setInsecure(); // sem verificação de certificado (RAM limitada no ESP8266)

  HTTPClient http;
  http.begin(client, VERSION_URL);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[OTA] Falha ao buscar version.json: %d\n", httpCode);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[OTA] JSON invalido: %s\n", err.c_str());
    return;
  }

  String fwUrl = doc["firmware_url"] | "";
  String fsUrl = doc["fs_url"]       | "";
  int remoteFwVer = doc["firmware_version"] | 0;
  int remoteFsVer = doc["fs_version"]       | 0;

  Serial.printf("[OTA] Local FW=%d FS=%d | Remoto FW=%d FS=%d\n",
                CURRENT_FIRMWARE_VER, CURRENT_FS_VER, remoteFwVer, remoteFsVer);

  // Atualiza FS primeiro (sem reboot), depois firmware (com reboot)
  if (remoteFsVer > CURRENT_FS_VER) {
    Serial.println("[OTA] Atualizando LittleFS...");
    ESPhttpUpdate.rebootOnUpdate(false);
    t_httpUpdate_return ret = ESPhttpUpdate.updateFS(client, fsUrl);
    if (ret == HTTP_UPDATE_OK) {
      Serial.println("[OTA] LittleFS atualizado com sucesso.");
    } else {
      Serial.printf("[OTA] Falha no FS update: %s\n", ESPhttpUpdate.getLastErrorString().c_str());
    }
  }

  if (remoteFwVer > CURRENT_FIRMWARE_VER) {
    Serial.println("[OTA] Atualizando Firmware...");
    ESPhttpUpdate.rebootOnUpdate(true);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, fwUrl);
    // Se chegou aqui, houve falha (sucesso causa reboot automatico)
    Serial.printf("[OTA] Falha no FW update: %s\n", ESPhttpUpdate.getLastErrorString().c_str());
  }
}

void setup() {
  Serial.begin(115200);
  megaSerial.begin(9600);

  if (!LittleFS.begin()) {
    Serial.println("Erro LittleFS");
    return;
  }

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

void loop() {
  MDNS.update();
  server.handleClient();
  webSocket.loop();

  // Recebe cliques do Arduino Mega
  if (megaSerial.available()) {
    String cmd = megaSerial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "CLICK") {
      makitas += 1.0;
      broadcastState();
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
}