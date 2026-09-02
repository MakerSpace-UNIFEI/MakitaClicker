// =============================================================
//  TESTE DE COMUNICAÇÃO SERIAL — ESP8266 (lado ESP)
//  Pinos: D6 = RX (← TX0 do Mega) | D7 = TX (→ RX0 do Mega)
//  Baud rate: 38400
//
//  O que este sketch faz:
//   1. Quando receber "PING:<n>" do Mega, responde com "PONG:<n>".
//   2. A cada 3 segundos envia um pacote MAKITA: fake para testar
//      se o Arduino consegue processar o formato real.
//   3. Loga tudo no Serial Monitor via porta USB (115200).
// =============================================================

#include <SoftwareSerial.h>

#define GAME_BAUD_RATE 38400

// D6 = RX (recebe do Mega TX0)  |  D7 = TX (envia para Mega RX0)
SoftwareSerial megaSerial(D6, D7);

unsigned long ultimoPacoteFake = 0;
const unsigned long intervaloPacote = 3000; // ms
uint32_t contadorPong = 0;

// Valores fake incrementais para simular o jogo
double fakeMakitas    = 1000.0;
double fakeMps        = 12.5;
double fakeClickPower = 5.0;
int    fakeTotalOwned = 3;

void setup() {
  // Serial USB: logs para o Serial Monitor do computador
  Serial.begin(115200);
  Serial.println(F("=== ESP8266: Teste Serial Mega ==="));
  Serial.println(F("SoftwareSerial em D6(RX)/D7(TX) @ 38400 baud"));
  Serial.println(F("Aguardando PING do Arduino Mega..."));

  // SoftwareSerial: canal com o Arduino Mega
  megaSerial.begin(GAME_BAUD_RATE);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // LED onboard é ativo em LOW no ESP
}

// Buffer para recepção não-bloqueante
char rxBuf[64];
uint8_t rxIdx = 0;

void processarMensagem(const char* msg) {
  Serial.print(F("[Mega→ESP] "));
  Serial.println(msg);

  // Responde PING com PONG
  if (strncmp(msg, "PING:", 5) == 0) {
    contadorPong++;
    const char* num = msg + 5; // parte numérica após "PING:"
    String resposta = "PONG:" + String(num);
    megaSerial.println(resposta);

    Serial.print(F("[ESP→Mega] "));
    Serial.println(resposta);

    // Pisca LED onboard como feedback visual
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH);
  }

  // Trata CLICK vindo do Mega (útil para testar o canal reverso)
  if (strcmp(msg, "CLICK") == 0) {
    fakeMakitas += fakeClickPower;
    Serial.println(F("  → CLICK recebido! Makitas incrementadas."));
  }
}

void loop() {
  unsigned long now = millis();

  // ── Leitura não-bloqueante da SoftwareSerial ─────────────────
  while (megaSerial.available() > 0) {
    char c = (char)megaSerial.read();
    if (c == '\n' || c == '\r') {
      if (rxIdx > 0) {
        rxBuf[rxIdx] = '\0';
        processarMensagem(rxBuf);
        rxIdx = 0;
      }
    } else if (rxIdx < sizeof(rxBuf) - 1 && c >= 32 && c <= 126) {
      rxBuf[rxIdx++] = c;
    } else {
      rxIdx = 0; // descarta buffer corrompido
    }
  }

  // ── Envia pacote MAKITA: fake periodicamente ─────────────────
  if (now - ultimoPacoteFake >= intervaloPacote) {
    ultimoPacoteFake = now;

    // Simula crescimento do jogo
    fakeMakitas    += fakeMps * (intervaloPacote / 1000.0);
    fakeTotalOwned  = (fakeTotalOwned < 20) ? fakeTotalOwned + 1 : fakeTotalOwned;

    // Formato exato igual ao código real: MAKITA:<saldo>,<mps>,<clickPower>,<totalOwned>
    String pacote = "MAKITA:" +
                    String(fakeMakitas, 2) + "," +
                    String(fakeMps, 2)     + "," +
                    String(fakeClickPower, 2) + "," +
                    String(fakeTotalOwned);

    megaSerial.println(pacote);

    Serial.print(F("[ESP→Mega] "));
    Serial.println(pacote);
  }

  // ── Repassa digitação do Serial Monitor para o Mega ──────────
  // (Útil para enviar comandos manuais durante o teste)
  while (Serial.available() > 0) {
    char c = Serial.read();
    megaSerial.write(c);
    Serial.write(c); // echo local
  }
}
