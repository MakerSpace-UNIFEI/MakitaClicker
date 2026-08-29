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

  // Serial principal (Pinos 0 RX e 1 TX) conectada ao ESP8266 a 115200 baud
  Serial.begin(115200);

  Wire.begin();
  lcd.init();
  lcd.backlight();

  // Layout inicial do display
  lcd.setCursor(0, 0);
  lcd.print("=== OFICINA GLOBAL =");
  
  atualizarLCD();

  lcd.setCursor(0, 3);
  lcd.print("Pressione o botao...");
}

void loop() {
  // 1. Leitura e Debounce do Botão Físico
  bool leitura = digitalRead(BUTTON_PIN);
  if (leitura != ultimoEstadoBotao) {
    if ((millis() - ultimoDebounce) > tempoDebounce) {
      if (leitura == LOW) { // Botão foi pressionado
        Serial.println("CLICK");
      }
      ultimoDebounce = millis();
      ultimoEstadoBotao = leitura;
    }
  }

  // 2. Leitura dos pacotes recebidos do ESP8266
  if (Serial.available()) {
    String buffer = Serial.readStringUntil('\n');
    buffer.trim();

    if (buffer.length() > 0) {
      // Decodifica o padrão MAKITA:<valor>,<mps>
      if (buffer.startsWith("MAKITA:")) {
        String dados = buffer.substring(7);
        int separador = dados.indexOf(',');

        if (separador != -1) {
          makitasGlobal = dados.substring(0, separador).toInt();
          mpsGlobal = dados.substring(separador + 1);
          
          atualizarLCD();
        }
      }
    }
  }
}