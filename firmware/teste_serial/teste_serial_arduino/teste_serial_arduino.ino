// =============================================================
//  TESTE DE COMUNICAÇÃO SERIAL E BOOTLOADER — Arduino Mega
//  Pinos: TX0 (pino 1) → D6 do ESP | RX0 (pino 0) ← D7 do ESP
//  Pino RESET do Mega ← D5 do ESP
//  Baud rate: 115200 (a mesma velocidade usada pelo Bootloader)
// =============================================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define BAUD_RATE 115200
#define LED_PIN 13

LiquidCrystal_I2C *lcd = nullptr;

unsigned long ultimoPing = 0;
const unsigned long intervaloPing = 2000;
uint32_t contadorEnvios = 0;
uint32_t contadorRecebidos = 0;

bool ledAtivo = false;
unsigned long ledInicio = 0;

uint8_t detectarEnderecoI2C() {
  Wire.begin();
  byte addrs[] = { 0x27, 0x3F, 0x20, 0x26, 0x38 };
  for (byte i = 0; i < sizeof(addrs); i++) {
    Wire.beginTransmission(addrs[i]);
    if (Wire.endTransmission() == 0) return addrs[i];
  }
  return 0x27; // fallback padrao
}

void printLcd(uint8_t row, String txt) {
  if (!lcd) return;
  while (txt.length() < 20) txt += " ";
  if (txt.length() > 20) txt = txt.substring(0, 20);
  lcd->setCursor(0, row);
  lcd->print(txt);
}

void piscarLED() {
  ledAtivo = true;
  ledInicio = millis();
  digitalWrite(LED_PIN, HIGH);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Inicializa comunicação com o ESP a 115200
  Serial.begin(BAUD_RATE);

  // Inicializa LCD I2C se presente
  uint8_t addr = detectarEnderecoI2C();
  lcd = new LiquidCrystal_I2C(addr, 20, 4);
  lcd->init();
  lcd->backlight();
  lcd->clear();

  printLcd(0, "== TESTE SERIAL ==");
  printLcd(1, "Baud: 115200");
  printLcd(2, "Mega INICIALIZADO!");
  printLcd(3, "Aguardando ESP...");

  piscarLED();
}

void loop() {
  unsigned long now = millis();

  // Apaga LED apos pulso de 150ms
  if (ledAtivo && (now - ledInicio >= 150)) {
    ledAtivo = false;
    digitalWrite(LED_PIN, LOW);
  }

  // 1. Monitora nivel fisico do pino 0 (RX0) no LCD
  bool nivelRx0 = digitalRead(0);
  static bool ultimoNivelRx0 = !nivelRx0;
  if (nivelRx0 != ultimoNivelRx0) {
    ultimoNivelRx0 = nivelRx0;
    printLcd(1, "Baud:115200 RX0:" + String(nivelRx0 ? "HIGH(5V)" : "LOW(0V) "));
  }

  // 2. Envia PING periodico para o ESP a cada 2s
  if (now - ultimoPing >= intervaloPing) {
    ultimoPing = now;
    contadorEnvios++;

    String ping = "PING_MEGA:" + String(contadorEnvios);
    Serial.println(ping);

    printLcd(3, "TX #" + String(contadorEnvios) + " " + ping);
  }

  // 3. Le respostas do ESP
  if (Serial.available() > 0) {
    String recebido = Serial.readStringUntil('\n');
    recebido.trim();
    if (recebido.length() > 0) {
      contadorRecebidos++;
      piscarLED();

      // Eco de confirmacao de volta para o ESP
      Serial.println("ECHO_MEGA:" + recebido);

      printLcd(2, "RX #" + String(contadorRecebidos) + ": " + recebido.substring(0, 13));
    }
  }
}
