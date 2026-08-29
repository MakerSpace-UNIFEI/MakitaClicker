#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Endereço I2C, 20 colunas, 4 linhas
LiquidCrystal_I2C lcd(0x27, 20, 4);

const int BUTTON_PIN = 7;

bool ultimoEstadoBotao = HIGH;
unsigned long ultimoDebounce = 0;
const unsigned long tempoDebounce = 40;

long makitasGlobal = 0;
String mpsGlobal = "0.0";

void atualizarLCD() {
  // Linha 1: Saldo de Makitas
  lcd.setCursor(0, 1);
  lcd.print("Makitas: ");
  lcd.print(makitasGlobal);
  lcd.print("         "); // Limpa caracteres residuais

  // Linha 2: Taxa de Produção
  lcd.setCursor(0, 2);
  lcd.print("Taxa: ");
  lcd.print(mpsGlobal);
  lcd.print("/s    ");
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(115200);  // Monitor Serial USB (PC)
  Serial1.begin(9600);   // UART Hardware Serial1 com o ESP8266 (TX1: 18, RX1: 19)

  Wire.begin();
  lcd.init();
  lcd.backlight();

  // Layout inicial do display
  lcd.setCursor(0, 0);
  lcd.print("=== OFICINA GLOBAL =");
  
  atualizarLCD();

  lcd.setCursor(0, 3);
  lcd.print("Pressione o botao...");

  Serial.println("\n--- ARDUINO MEGA INICIADO ---");
  Serial.println("[INFO] Botao configurado no Pino 7.");
  Serial.println("[INFO] Serial1 conectada ao ESP8266 a 9600 baud.");
  Serial.println("----------------------------------------------");
}

void loop() {
  // 1. Leitura e Debounce do Botão Físico
  bool leitura = digitalRead(BUTTON_PIN);
  if (leitura != ultimoEstadoBotao) {
    if ((millis() - ultimoDebounce) > tempoDebounce) {
      if (leitura == LOW) { // Botão foi pressionado
        Serial1.println("CLICK");
        Serial.println("[LOCAL] Botao Pino 7 pressionado -> Comando 'CLICK' enviado pela Serial1.");
      }
      ultimoDebounce = millis();
      ultimoEstadoBotao = leitura;
    }
  }

  // 2. Leitura dos pacotes recebidos do ESP8266
  if (Serial1.available()) {
    String buffer = Serial1.readStringUntil('\n');
    buffer.trim();

    if (buffer.length() > 0) {
      Serial.println("[ESP1 -> MEGA RECEBIDO]: " + buffer);

      // Decodifica o padrão MAKITA:<valor>,<mps>
      if (buffer.startsWith("MAKITA:")) {
        String dados = buffer.substring(7);
        int separador = dados.indexOf(',');

        if (separador != -1) {
          makitasGlobal = dados.substring(0, separador).toInt();
          mpsGlobal = dados.substring(separador + 1);
          
          atualizarLCD();
          
          // Impressão compatível com AVR Serial
          Serial.print("[LCD ATUALIZADO] Saldo: ");
          Serial.print(makitasGlobal);
          Serial.print(" | Taxa: ");
          Serial.print(mpsGlobal);
          Serial.println("/s");
        }
      }
    }
  }
}