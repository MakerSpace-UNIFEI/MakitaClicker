// =============================================================
//  TESTE DE COMUNICAÇÃO SERIAL — Arduino Mega (lado Mega)
//  Pinos: TX0 (pino 1) → D6 do ESP | RX0 (pino 0) ← D7 do ESP
//  Baud rate: 38400 (igual ao projeto principal)
//
//  O que este sketch faz:
//   1. A cada 2 segundos envia "PING:<contador>" para o ESP.
//   2. Qualquer coisa recebida do ESP é exibida no Serial Monitor.
//   3. Se receber "PONG" (resposta do ESP), acende o LED onboard.
//   4. Também repassa o que você digitar no Serial Monitor para o ESP.
// =============================================================

#define GAME_BAUD_RATE 38400

// LED onboard do Arduino Mega (pino 13)
#define LED_PIN 13

unsigned long ultimoPing = 0;
const unsigned long intervaloPing = 2000; // ms
uint32_t contadorPing = 0;

bool ledAtivo = false;
unsigned long ledInicio = 0;
const unsigned long ledDuracao = 300; // ms

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Serial0: comunicação com o ESP via D6/D7 (pinos físicos 0 e 1)
  Serial.begin(GAME_BAUD_RATE);

  // Serial1: opcional — use um segundo adaptador USB-Serial no pino 18(TX1)/19(RX1)
  // para ver os logs sem interferir no canal com o ESP.
  // Se não tiver, comente Serial1 e use apenas Serial Monitor no baud 38400.
  Serial1.begin(115200);
  Serial1.println(F("=== ARDUINO MEGA: Teste Serial ESP ==="));
  Serial1.println(F("Aguardando comunicacao em Serial0 @ 38400..."));
}

void piscarLED() {
  ledAtivo = true;
  ledInicio = millis();
  digitalWrite(LED_PIN, HIGH);
}

void loop() {
  unsigned long now = millis();

  // ── Desliga LED após duração ──────────────────────────────────
  if (ledAtivo && (now - ledInicio >= ledDuracao)) {
    ledAtivo = false;
    digitalWrite(LED_PIN, LOW);
  }

  // ── Envia PING periódico para o ESP ──────────────────────────
  if (now - ultimoPing >= intervaloPing) {
    ultimoPing = now;
    contadorPing++;

    String msg = "PING:" + String(contadorPing);
    Serial.println(msg);  // Envia para o ESP

    Serial1.print(F("[Mega→ESP] "));
    Serial1.println(msg);
  }

  // ── Recebe dados do ESP e exibe no Serial1 (monitor) ─────────
  while (Serial.available() > 0) {
    String recebido = Serial.readStringUntil('\n');
    recebido.trim();
    if (recebido.length() == 0) continue;

    Serial1.print(F("[ESP→Mega] "));
    Serial1.println(recebido);

    // Se receber "PONG", acende LED como confirmação visual
    if (recebido.startsWith("PONG")) {
      piscarLED();
      Serial1.println(F("  ✓ PONG recebido! Canal OK."));
    }

    // Se receber o pacote real do jogo, valida o formato
    if (recebido.startsWith("MAKITA:")) {
      Serial1.println(F("  ✓ Pacote MAKITA detectado! Formato OK."));
    }
  }

  // ── Repassa digitação do Serial Monitor para o ESP ───────────
  // (Útil para testar comandos manuais como "CLICK")
  while (Serial1.available() > 0) {
    char c = Serial1.read();
    Serial.write(c);
  }
}
