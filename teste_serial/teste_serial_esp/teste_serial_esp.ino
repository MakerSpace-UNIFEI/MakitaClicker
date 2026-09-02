// =============================================================
//  TESTE DIAGNÓSTICO COMPLETO — ESP8266
//  Pinos: D6 = RX (← TX0 Mega) | D7 = TX (→ RX0 Mega)
//         D5 = RESET do Mega
//  Velocidade com Mega: 115200 baud
//  Velocidade USB Monitor: 115200 baud
// =============================================================

#include <SoftwareSerial.h>
extern "C" {
  #include "user_interface.h"
}

#define MEGA_RESET_PIN D5
#define BAUD_BOOT 115200
#define BAUD_ALT 57600

SoftwareSerial megaSerial(D6, D7);

unsigned long ultimoEnvio = 0;
uint32_t contador = 0;

void resetarMega() {
  Serial.println(F("\n[TESTE-RESET] Puxando D5 para LOW por 100ms..."));
  digitalWrite(MEGA_RESET_PIN, LOW);
  pinMode(MEGA_RESET_PIN, OUTPUT);
  delay(100);
  pinMode(MEGA_RESET_PIN, INPUT); // Solta em Hi-Z
  Serial.println(F("[TESTE-RESET] D5 solto (Hi-Z). O Mega deve ter reiniciado!"));
}

void testarBootloader(uint32_t baud) {
  Serial.printf("\n=== TESTE BOOTLOADER STK500v2 @ %d BAUD ===\n", baud);
  megaSerial.begin(baud);
  delay(10);
  while (megaSerial.available()) megaSerial.read();

  // Reseta o Mega
  digitalWrite(MEGA_RESET_PIN, LOW);
  pinMode(MEGA_RESET_PIN, OUTPUT);
  delay(100);
  pinMode(MEGA_RESET_PIN, INPUT);
  delay(80); // Aguarda bootloader acordar UART0

  while (megaSerial.available()) megaSerial.read();

  // CMD_SIGN_ON: 0x1B 0x01 0x00 0x01 0x0E 0x01 0x14
  uint8_t signOn[] = { 0x1B, 0x01, 0x00, 0x01, 0x0E, 0x01, 0x14 };
  bool respondeu = false;

  for (int tentativa = 1; tentativa <= 8; tentativa++) {
    while (megaSerial.available()) megaSerial.read();
    megaSerial.write(signOn, sizeof(signOn));
    megaSerial.flush();

    unsigned long start = millis();
    Serial.printf("  Tentativa %d: aguardando resposta... ", tentativa);

    int bytesRecebidos = 0;
    uint8_t buf[32];
    while (millis() - start < 150) {
      while (megaSerial.available() > 0) {
        uint8_t c = megaSerial.read();
        if (bytesRecebidos < sizeof(buf)) buf[bytesRecebidos] = c;
        bytesRecebidos++;
      }
      delay(1);
    }

    if (bytesRecebidos > 0) {
      respondeu = true;
      Serial.printf("SUCESSO! Recebeu %d bytes: [ ", bytesRecebidos);
      for (int i = 0; i < min(bytesRecebidos, (int)sizeof(buf)); i++) {
        Serial.printf("0x%02X ", buf[i]);
      }
      Serial.println("]");
      break;
    } else {
      Serial.println("0 bytes.");
    }
    delay(20);
  }

  if (respondeu) {
    Serial.printf(">>> BOOTLOADER RESPONDEU EM %d BAUD! <<<\n", baud);
  } else {
    Serial.printf(">>> Nenhum sinal do bootloader em %d baud. <<<\n", baud);
  }
}

void setup() {
  system_update_cpu_freq(160); // 160MHz
  Serial.begin(115200);
  delay(500);

  Serial.println(F("\n============================================="));
  Serial.println(F("   DIAGNÓSTICO SERIAL & BOOTLOADER ESP-MEGA   "));
  Serial.println(F("============================================="));
  Serial.println(F("Comandos no Serial Monitor:"));
  Serial.println(F("  '1' -> Testar Bootloader a 115200 baud"));
  Serial.println(F("  '2' -> Testar Bootloader a 57600 baud"));
  Serial.println(F("  'r' -> Resetar o Mega manualmente via D5"));
  Serial.println(F("  'p' -> Enviar PING de texto a 115200"));
  Serial.println(F("  't' -> Testar nível físico do D7 (LOW/HIGH por 2s)"));
  Serial.println(F("=============================================\n"));

  pinMode(MEGA_RESET_PIN, INPUT); // Comeca em Hi-Z
  megaSerial.begin(BAUD_BOOT);
  Serial.println(F("SoftwareSerial D6(RX)/D7(TX) iniciado a 115200 baud."));
  Serial.println(F("Modo escuta ativo. Qualquer dado recebido do Mega sera exibido abaixo:\n"));
}

void loop() {
  // 1. Le tudo que o Mega envia e exibe no Serial Monitor com detalhes
  if (megaSerial.available() > 0) {
    Serial.print(F("[MEGA->ESP]: "));
    while (megaSerial.available() > 0) {
      char c = (char)megaSerial.read();
      if (c >= 32 && c <= 126) {
        Serial.print(c);
      } else if (c == '\n') {
        Serial.print(F("\\n\n"));
      } else if (c == '\r') {
        Serial.print(F("\\r"));
      } else {
        Serial.printf("[0x%02X]", (uint8_t)c);
      }
      delay(1);
    }
  }

  // 2. Comandos do usuario digitados no Serial Monitor
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '1') {
      testarBootloader(115200);
      megaSerial.begin(BAUD_BOOT);
    } else if (cmd == '2') {
      testarBootloader(57600);
      megaSerial.begin(BAUD_BOOT);
    } else if (cmd == 'r' || cmd == 'R') {
      resetarMega();
    } else if (cmd == 'p' || cmd == 'P') {
      contador++;
      String p = "PING_ESP:" + String(contador);
      megaSerial.println(p);
      Serial.println("[ESP->MEGA]: " + p);
    } else if (cmd == 't' || cmd == 'T') {
      Serial.println(F("\n[TESTE] Testando nível elétrico em D7, D1 e D2 simultaneamente..."));
      megaSerial.end();
      pinMode(D7, OUTPUT);
      pinMode(D1, OUTPUT);
      pinMode(D2, OUTPUT);
      for (int i = 1; i <= 4; i++) {
        Serial.printf("  Ciclo %d: D7, D1 e D2 em LOW (0V) por 2 segundos... (Olhe o LCD do Mega!)\n", i);
        digitalWrite(D7, LOW);
        digitalWrite(D1, LOW);
        digitalWrite(D2, LOW);
        delay(2000);
        Serial.printf("  Ciclo %d: D7, D1 e D2 em HIGH (3.3V) por 2 segundos... (Olhe o LCD do Mega!)\n", i);
        digitalWrite(D7, HIGH);
        digitalWrite(D1, HIGH);
        digitalWrite(D2, HIGH);
        delay(2000);
      }
      megaSerial.begin(BAUD_BOOT);
      Serial.println(F("[TESTE] Teste concluido! megaSerial restaurada.\n"));
    }
  }
}
