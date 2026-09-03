#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

#define GAME_BAUD_RATE 38400

// Configuração do Botão Físico
const int PIN_BOTAO = 7; // Botão simples no Pino 7 (INPUT_PULLUP / fecha no GND)

// Ponteiro dinâmico para o LCD (permite auto-detecção de endereço I2C: 0x27, 0x3F, etc.)
LiquidCrystal_I2C* lcd = nullptr;

// Detecção de borda com debounce (resposta instantânea)
bool estadoAnterior = HIGH;
unsigned long ultimoTempoBotao = 0;

const unsigned long DEBOUNCE_MS = 25; // Filtro de 25ms para botões mecânicos e microswitches

// Controle de atualização desacoplada do LCD (elimina latência I2C)
bool precisaAtualizarLCD = false;
unsigned long ultimoUpdateLCD = 0;
const unsigned long INTERVALO_UPDATE_LCD = 75; // Atualiza LCD em no máximo ~13 FPS

// Estado do Jogo recebido do ESP8266
double makitasGlobal = 0.0;
double mpsGlobal = 0.0;
double clickPowerGlobal = 1.0;
int totalOwnedGlobal = 0;
String webInfo = "pages.dev";

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

// Auto-detecta o endereço I2C do display LCD (suporta PCF8574 com 0x27, PCF8574A com 0x3F e clones)
uint8_t detectarEnderecoI2C() {
  const uint8_t enderecosComuns[] = {0x27, 0x3F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E};
  for (uint8_t i = 0; i < sizeof(enderecosComuns); i++) {
    Wire.beginTransmission(enderecosComuns[i]);
    if (Wire.endTransmission() == 0) {
      return enderecosComuns[i];
    }
  }
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      return addr;
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

// Cache de linha para double-buffering (elimina transmissões redundantes via I2C)
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
    return; // Linha inalterada, dispensa comunicação I2C
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

  // Linha 0: Cabeçalho com Ícones de Disco Giratório / Faíscas de Corte (só reescreve se mudar)
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

  // Linha 1: Saldo de Makitas (com Ícone de Moeda ou Troféu para 99B+)
  bool is99B = (makitasGlobal >= 99000000000.0);
  String saldoStr = is99B 
      ? (" Saldo:" + formatarNumero(makitasGlobal) + " MKT!") 
      : (" Saldo:" + formatarNumero(makitasGlobal) + " MKT");
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
  String taxaStr = "+" + formatarNumero(clickPowerGlobal) + " | ";
  String mpsStr = " " + formatarNumero(mpsGlobal) + "/s";
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

  // Linha 3: Feedback de Clique ou Status Rotativo com IP e Meta 99B
  if (clickAtivo) {
    printLinhaFormatada(3, ">> CORTE EFETUADO! <<");
  } else {
    if (modoInfoLinha3 == 0) {
      printLinhaFormatada(3, "Web: " + webInfo);
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
      printLinhaFormatada(3, "Oficinas: " + String(totalOwnedGlobal) + " un.");
    } else {
      printLinhaFormatada(3, " MakerSpace UNIFEI  ");
    }
  }
}

// Buffer Serial Não-Bloqueante (Zero timeout, imune a ruídos e travamentos)
char serialRxBuf[96];
uint8_t serialRxIdx = 0;

void processPacket(char *line) {
  // Trata pacote de URL ou IP da ESP
  char *webStart = strstr(line, "WEB:");
  if (webStart) {
    webInfo = String(webStart + 4);
    webInfo.trim();
    precisaAtualizarLCD = true;
    return;
  }
  char *ipStart = strstr(line, "IP:");
  if (ipStart) {
    webInfo = String(ipStart + 3);
    webInfo.trim();
    precisaAtualizarLCD = true;
    return;
  }

  // Procura por "MAKITA:" em qualquer ponto da linha recebida
  char *start = strstr(line, "MAKITA:");
  if (!start) return;
  char *p = start + 7;
  
  // Decodifica formato: MAKITA:<saldo>,<mps>,<clickPower>,<totalOwned>
  char *sep1 = strchr(p, ',');
  if (!sep1) return;
  *sep1 = '\0';
  double mkt = atof(p);
  
  char *p2 = sep1 + 1;
  char *sep2 = strchr(p2, ',');
  if (!sep2) {
    makitasGlobal = mkt;
    mpsGlobal = atof(p2);
    precisaAtualizarLCD = true;
    return;
  }
  *sep2 = '\0';
  double mps = atof(p2);
  
  char *p3 = sep2 + 1;
  char *sep3 = strchr(p3, ',');
  if (!sep3) {
    makitasGlobal = mkt;
    mpsGlobal = mps;
    clickPowerGlobal = atof(p3);
    precisaAtualizarLCD = true;
    return;
  }
  *sep3 = '\0';
  double cp = atof(p3);
  
  char *p4 = sep3 + 1;
  int ow = atoi(p4);
  
  makitasGlobal = mkt;
  mpsGlobal = mps;
  clickPowerGlobal = cp;
  totalOwnedGlobal = ow;
  
  precisaAtualizarLCD = true;
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
      if (c >= 32 && c <= 126) {
        serialRxBuf[serialRxIdx++] = c;
      }
    } else {
      serialRxIdx = 0;
    }
  }
}

void setup() {
  pinMode(PIN_BOTAO, INPUT_PULLUP);

  // Comunicação Serial0 com ESP8266 a 38400 baud (estável e sem perda de pacotes)
  Serial.begin(GAME_BAUD_RATE);
  Serial.setTimeout(20);

  Wire.begin();
  #if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(25000, true);
  #endif

  // Auto-detecta endereço I2C do display LCD
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

  // 1. Leitura do Botão Físico no Pino 7 (disparo instantâneo no fechamento para GND)
  bool leitura = digitalRead(PIN_BOTAO);
  if (leitura == LOW && estadoAnterior == HIGH && (now - ultimoTempoBotao > DEBOUNCE_MS)) {
    ultimoTempoBotao = now;
    Serial.println("CLICK");
    makitasGlobal += clickPowerGlobal;
    ultimoClickVisual = now;
    frameAnimacao = (frameAnimacao + 1) % 4;
    precisaAtualizarLCD = true;
  }
  estadoAnterior = leitura;

  // 2. Processa pacotes seriais do ESP8266 de forma 100% não-bloqueante
  processarSerialRecebida();

  // 3. Animação de rotação do disco e rotação de informações na linha 3
  if (now - ultimoTickAnimacao >= 400) {
    ultimoTickAnimacao = now;
    if (mpsGlobal > 0 || (now - ultimoClickVisual < duracaoFeedbackClick)) {
      frameAnimacao = (frameAnimacao + 1) % 4;
      precisaAtualizarLCD = true;
    }
  }

  if (now - ultimoTickInfo >= 3200) {
    ultimoTickInfo = now;
    modoInfoLinha3 = (modoInfoLinha3 + 1) % 4;
    precisaAtualizarLCD = true;
  }

  // 4. Atualização não-bloqueante e cadenciada do Display LCD
  if (precisaAtualizarLCD && (now - ultimoUpdateLCD >= INTERVALO_UPDATE_LCD)) {
    precisaAtualizarLCD = false;
    ultimoUpdateLCD = now;
    atualizarLCD();
  }
}