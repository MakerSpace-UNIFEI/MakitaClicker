#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Endereço I2C padrão 0x27, 20 colunas, 4 linhas
LiquidCrystal_I2C lcd(0x27, 20, 4);

const int BUTTON_PIN = 7;

// Debounce do Botão Físico
bool ultimoEstadoBotao = HIGH;
unsigned long ultimoDebounce = 0;
const unsigned long tempoDebounce = 35;

// Estado do Jogo recebido do ESP8266
long makitasGlobal = 0;
float mpsGlobal = 0.0;
float clickPowerGlobal = 1.0;
int totalOwnedGlobal = 0;

// Animação e feedback visual
unsigned long ultimoClickVisual = 0;
const unsigned long duracaoFeedbackClick = 600;
unsigned long ultimoTickAnimacao = 0;
int frameAnimacao = 0;
unsigned long ultimoTickInfo = 0;
int modoInfoLinha3 = 0;

// Caracteres Customizados (5x8 pixels)
byte iconBlade0[8] = {
  B00100,
  B01110,
  B11011,
  B00100,
  B00100,
  B11011,
  B01110,
  B00100
};

byte iconBlade1[8] = {
  B10001,
  B01110,
  B01010,
  B11111,
  B01010,
  B01110,
  B10001,
  B00000
};

byte iconCoin[8] = {
  B01110,
  B10001,
  B10101,
  B10101,
  B10101,
  B10001,
  B01110,
  B00000
};

byte iconBolt[8] = {
  B00010,
  B00100,
  B01000,
  B11111,
  B00010,
  B00100,
  B01000,
  B00000
};

byte iconFactory[8] = {
  B10010,
  B11011,
  B11011,
  B11011,
  B11111,
  B11111,
  B11111,
  B00000
};

// Formatação inteligente de números para caber no display 20x4
String formatarNumero(long num) {
  if (num < 1000) {
    return String(num);
  } else if (num < 1000000) {
    long milhar = num / 1000;
    long resto = (num % 1000) / 100;
    if (num < 10000) {
      return String(milhar) + "." + String(resto) + "k";
    } else {
      return String(milhar) + "k";
    }
  } else if (num < 1000000000) {
    float milhao = num / 1000000.0;
    return String(milhao, (num < 10000000 ? 2 : 1)) + "M";
  } else {
    float bilhao = num / 1000000000.0;
    return String(bilhao, 2) + "B";
  }
}

// Imprime uma linha completa preenchendo com espaços (elimina flicker do lcd.clear)
void printLinhaFormatada(int linha, String texto) {
  while (texto.length() < 20) {
    texto += " ";
  }
  if (texto.length() > 20) {
    texto = texto.substring(0, 20);
  }
  lcd.setCursor(0, linha);
  lcd.print(texto);
}

void atualizarLCD() {
  unsigned long now = millis();

  // Linha 0: Cabeçalho com Ícones de Disco Giratório Animado
  char bladeChar = (frameAnimacao % 2 == 0) ? 0 : 1;
  lcd.setCursor(0, 0);
  lcd.write(bladeChar);
  lcd.print(" MAKITA CLICKER ");
  lcd.write(bladeChar);
  lcd.print("  ");

  // Linha 1: Saldo de Makitas com Ícone de Moeda/Makita
  lcd.setCursor(0, 1);
  lcd.write(2); // iconCoin
  String saldoStr = " Saldo: " + formatarNumero(makitasGlobal) + " MKT";
  while (saldoStr.length() < 19) saldoStr += " ";
  lcd.print(saldoStr.substring(0, 19));

  // Linha 2: Poder de Clique e Taxa de Produção (MPS)
  lcd.setCursor(0, 2);
  lcd.write(3); // iconBolt
  String taxaStr = "+" + String(clickPowerGlobal, (clickPowerGlobal == (int)clickPowerGlobal ? 0 : 1)) + " | ";
  lcd.print(taxaStr);
  lcd.write(4); // iconFactory
  String mpsStr = " " + String(mpsGlobal, 1) + "/s";
  while (mpsStr.length() < (20 - 1 - taxaStr.length() - 1)) mpsStr += " ";
  lcd.print(mpsStr);

  // Linha 3: Feedback de Clique ou Status Rotativo
  if (now - ultimoClickVisual < duracaoFeedbackClick) {
    printLinhaFormatada(3, ">> CORTE EFETUADO! <<");
  } else {
    if (modoInfoLinha3 == 0) {
      printLinhaFormatada(3, "Maquinas: " + String(totalOwnedGlobal) + " un.");
    } else if (modoInfoLinha3 == 1) {
      printLinhaFormatada(3, "MakerSpace UNIFEI");
    } else {
      printLinhaFormatada(3, "Web: esp-painel.local");
    }
  }
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Comunicação Serial0 com ESP8266 a 115200 baud
  Serial.begin(115200);

  Wire.begin();
  lcd.init();
  lcd.backlight();

  // Registra caracteres customizados na memória do display LCD
  lcd.createChar(0, iconBlade0);
  lcd.createChar(1, iconBlade1);
  lcd.createChar(2, iconCoin);
  lcd.createChar(3, iconBolt);
  lcd.createChar(4, iconFactory);

  // Tela de Inicialização com Animação
  printLinhaFormatada(0, "====================");
  printLinhaFormatada(1, "   MAKITA CLICKER   ");
  printLinhaFormatada(2, "  MakerSpace UNIFEI ");
  printLinhaFormatada(3, "   Iniciando v2.0   ");
  delay(1200);

  atualizarLCD();
}

void loop() {
  unsigned long now = millis();

  // 1. Leitura e Debounce do Botão Físico com Resposta Instantânea
  bool leitura = digitalRead(BUTTON_PIN);
  if (leitura != ultimoEstadoBotao) {
    if ((now - ultimoDebounce) > tempoDebounce) {
      if (leitura == LOW) { // Botão físico pressionado
        Serial.println("CLICK");
        makitasGlobal += (long)clickPowerGlobal;
        ultimoClickVisual = now;
        frameAnimacao = (frameAnimacao + 1) % 4;
        atualizarLCD();
      }
      ultimoDebounce = now;
      ultimoEstadoBotao = leitura;
    }
  }

  // 2. Leitura dos pacotes de telemetria recebidos do ESP8266
  if (Serial.available()) {
    String buffer = Serial.readStringUntil('\n');
    buffer.trim();

    if (buffer.startsWith("MAKITA:")) {
      String dados = buffer.substring(7);
      
      // Decodifica formato: MAKITA:<saldo>,<mps>,<clickPower>,<totalOwned>
      int sep1 = dados.indexOf(',');
      if (sep1 != -1) {
        makitasGlobal = dados.substring(0, sep1).toInt();
        
        int sep2 = dados.indexOf(',', sep1 + 1);
        if (sep2 != -1) {
          mpsGlobal = dados.substring(sep1 + 1, sep2).toFloat();
          
          int sep3 = dados.indexOf(',', sep2 + 1);
          if (sep3 != -1) {
            clickPowerGlobal = dados.substring(sep2 + 1, sep3).toFloat();
            totalOwnedGlobal = dados.substring(sep3 + 1).toInt();
          } else {
            clickPowerGlobal = dados.substring(sep2 + 1).toFloat();
          }
        } else {
          mpsGlobal = dados.substring(sep1 + 1).toFloat();
        }
        
        atualizarLCD();
      }
    }
  }

  // 3. Animação de rotação do disco e rotação de informações na linha 3
  if (now - ultimoTickAnimacao >= 400) {
    ultimoTickAnimacao = now;
    if (mpsGlobal > 0 || (now - ultimoClickVisual < duracaoFeedbackClick)) {
      frameAnimacao = (frameAnimacao + 1) % 4;
    }
    atualizarLCD();
  }

  if (now - ultimoTickInfo >= 3500) {
    ultimoTickInfo = now;
    modoInfoLinha3 = (modoInfoLinha3 + 1) % 3;
    atualizarLCD();
  }
}