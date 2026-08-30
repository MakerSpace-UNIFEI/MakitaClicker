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
double makitasGlobal = 0.0;
double mpsGlobal = 0.0;
double clickPowerGlobal = 1.0;
int totalOwnedGlobal = 0;

// Animação e feedback visual
unsigned long ultimoClickVisual = 0;
const unsigned long duracaoFeedbackClick = 600;
unsigned long ultimoTickAnimacao = 0;
int frameAnimacao = 0;
unsigned long ultimoTickInfo = 0;
int modoInfoLinha3 = 0;

// Caracteres Customizados (5x8 pixels, até 8 caracteres na CGRAM)
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

byte iconSpark[8] = {
  B00100,
  B10101,
  B01110,
  B11111,
  B01110,
  B10101,
  B00100,
  B00000
};

byte iconTrophy[8] = {
  B11111,
  B10101,
  B01110,
  B00100,
  B00100,
  B01110,
  B11111,
  B00000
};

// Formatação inteligente e ultra compacta de números para o LCD 20x4
// Suporta perfeitamente até Bilhões (99B+), Trilhões (T) e Quatrilhões (Qa)
String formatarNumero(double num) {
  if (num < 0) return "0";
  if (num < 999.5) {
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

// Imprime uma linha completa preenchendo exatamente com espaços (elimina flicker do lcd.clear)
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
  bool clickAtivo = (now - ultimoClickVisual < duracaoFeedbackClick);

  // Linha 0: Cabeçalho com Ícones de Disco Giratório / Faíscas de Corte
  char bladeChar = (frameAnimacao % 2 == 0) ? 0 : 1;
  lcd.setCursor(0, 0);
  if (clickAtivo) {
    lcd.write(5); // iconSpark
    lcd.print(" MAKITA CLICKER ");
    lcd.write(5); // iconSpark
    lcd.print("  ");
  } else {
    lcd.write(bladeChar);
    lcd.print(" MAKITA CLICKER ");
    lcd.write(bladeChar);
    lcd.print("  ");
  }

  // Linha 1: Saldo de Makitas (com Ícone de Moeda ou Troféu para 99B+)
  lcd.setCursor(0, 1);
  if (makitasGlobal >= 99000000000.0) {
    lcd.write(6); // iconTrophy
    String saldoStr = " Saldo:" + formatarNumero(makitasGlobal) + " MKT!";
    while (saldoStr.length() < 19) saldoStr += " ";
    lcd.print(saldoStr.substring(0, 19));
  } else {
    lcd.write(2); // iconCoin
    String saldoStr = " Saldo:" + formatarNumero(makitasGlobal) + " MKT";
    while (saldoStr.length() < 19) saldoStr += " ";
    lcd.print(saldoStr.substring(0, 19));
  }

  // Linha 2: Poder de Clique e Taxa de Produção (MPS)
  lcd.setCursor(0, 2);
  lcd.write(3); // iconBolt
  String taxaStr = "+" + formatarNumero(clickPowerGlobal) + " | ";
  lcd.print(taxaStr);
  lcd.write(4); // iconFactory
  String mpsStr = " " + formatarNumero(mpsGlobal) + "/s";
  int espacoRestante = 20 - 1 - taxaStr.length() - 1;
  if (espacoRestante > 0) {
    while (mpsStr.length() < espacoRestante) mpsStr += " ";
    lcd.print(mpsStr.substring(0, espacoRestante));
  }

  // Linha 3: Feedback de Clique ou Status Rotativo com Meta 99B
  if (clickAtivo) {
    printLinhaFormatada(3, ">> CORTE EFETUADO! <<");
  } else {
    if (modoInfoLinha3 == 0) {
      printLinhaFormatada(3, "Oficinas: " + String(totalOwnedGlobal) + " un.");
    } else if (modoInfoLinha3 == 1) {
      if (makitasGlobal >= 99000000000.0) {
        printLinhaFormatada(3, "** META 99B FEITA! **");
      } else {
        float pct = (float)(makitasGlobal / 99000000000.0) * 100.0;
        if (pct < 0.01 && makitasGlobal > 0) {
          printLinhaFormatada(3, "Meta 99B: >0.01%");
        } else {
          printLinhaFormatada(3, "Meta 99B: " + String(pct, (pct < 10.0 ? 2 : 1)) + "%");
        }
      }
    } else if (modoInfoLinha3 == 2) {
      printLinhaFormatada(3, "MakerSpace UNIFEI");
    } else {
      printLinhaFormatada(3, "Web: esp-painel.local");
    }
  }
}

// Buffer Serial Não-Bloqueante (Zero timeout, imune a travamentos)
char serialRxBuf[96];
uint8_t serialRxIdx = 0;

void processPacket(char *line) {
  if (strncmp(line, "MAKITA:", 7) != 0) return;
  char *p = line + 7;
  
  // Decodifica formato: MAKITA:<saldo>,<mps>,<clickPower>,<totalOwned>
  char *sep1 = strchr(p, ',');
  if (!sep1) return;
  *sep1 = '\0';
  makitasGlobal = atof(p);
  
  char *p2 = sep1 + 1;
  char *sep2 = strchr(p2, ',');
  if (!sep2) {
    mpsGlobal = atof(p2);
    atualizarLCD();
    return;
  }
  *sep2 = '\0';
  mpsGlobal = atof(p2);
  
  char *p3 = sep2 + 1;
  char *sep3 = strchr(p3, ',');
  if (!sep3) {
    clickPowerGlobal = atof(p3);
    atualizarLCD();
    return;
  }
  *sep3 = '\0';
  clickPowerGlobal = atof(p3);
  
  char *p4 = sep3 + 1;
  totalOwnedGlobal = atoi(p4);
  
  atualizarLCD();
}

void processarSerialRecebida() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialRxIdx > 0) {
        serialRxBuf[serialRxIdx] = '\0';
        processPacket(serialRxBuf);
        serialRxIdx = 0;
      }
    } else if (serialRxIdx < sizeof(serialRxBuf) - 1) {
      serialRxBuf[serialRxIdx++] = c;
    } else {
      // Buffer excedido: descarta para recuperar sincronia imediatamente
      serialRxIdx = 0;
    }
  }
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Comunicação Serial0 com ESP8266 a 115200 baud
  Serial.begin(115200);
  Serial.setTimeout(20);

  Wire.begin();
  lcd.init();
  lcd.backlight();

  // Registra caracteres customizados na memória CGRAM do display LCD
  lcd.createChar(0, iconBlade0);
  lcd.createChar(1, iconBlade1);
  lcd.createChar(2, iconCoin);
  lcd.createChar(3, iconBolt);
  lcd.createChar(4, iconFactory);
  lcd.createChar(5, iconSpark);
  lcd.createChar(6, iconTrophy);

  // Tela de Inicialização com Animação
  printLinhaFormatada(0, "====================");
  printLinhaFormatada(1, "   MAKITA CLICKER   ");
  printLinhaFormatada(2, "  MakerSpace UNIFEI ");
  printLinhaFormatada(3, "   Edicao 99B v3.0  ");
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
        makitasGlobal += clickPowerGlobal;
        ultimoClickVisual = now;
        frameAnimacao = (frameAnimacao + 1) % 4;
        atualizarLCD();
      }
      ultimoDebounce = now;
      ultimoEstadoBotao = leitura;
    }
  }

  // 2. Processa pacotes seriais do ESP8266 de forma 100% não-bloqueante
  processarSerialRecebida();

  // 3. Animação de rotação do disco e rotação de informações na linha 3
  if (now - ultimoTickAnimacao >= 400) {
    ultimoTickAnimacao = now;
    if (mpsGlobal > 0 || (now - ultimoClickVisual < duracaoFeedbackClick)) {
      frameAnimacao = (frameAnimacao + 1) % 4;
    }
    atualizarLCD();
  }

  if (now - ultimoTickInfo >= 3200) {
    ultimoTickInfo = now;
    modoInfoLinha3 = (modoInfoLinha3 + 1) % 4;
    atualizarLCD();
  }
}