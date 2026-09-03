# 🔌 Firmware — MakitaClicker

Firmware autônomo para o microcontrolador **ESP8266 NodeMCU**, controlando diretamente o display LCD 20×4 I2C, botão mecânico e sincronização com a nuvem (Cloudflare Pages & KV).

---

## 📂 Estrutura

```
firmware/
├── codigo_esp/                 # ESP8266 NodeMCU (ESP-12E / ESP-12F)
│   └── codigo_esp.ino          # Firmware único: LCD I2C, Botão, LittleFS, Sync HTTPS 5s, OTA
│
├── projeto/                    # Esquemático KiCad da PCB
│   ├── projeto.kicad_pcb
│   ├── projeto.kicad_pro
│   └── projeto.kicad_sch
│
└── GAME_DESIGN.md              # Mecânicas, upgrades, skill tree e protocolo
```

---

## 🧠 Arquitetura de Software Unificada

1. **CPU a 160MHz:** Alta velocidade e fluidez gráfica tanto para I2C quanto para criptografia TLS/HTTPS.
2. **Display LCD 20×4 I2C Nativo:**
   - Auto-detecção de endereço I2C (`0x27`, `0x3F`, etc.).
   - Double-buffering inteligente: apenas linhas alteradas são retransmitidas, eliminando flicker.
   - Caracteres customizados na CGRAM (disco de serra giratório, moedas, raios, troféu).
3. **Botão Físico com Resposta Instantânea (0ms):**
   - Pino `D5` (`INPUT_PULLUP`, fecha no GND).
   - Filtro de debounce de 25ms imune a repiques mecânicos.
   - Incremento imediato de makitas e animação sem qualquer intermediário serial.
4. **Persistência na Flash (LittleFS):**
   - Carrega save local no boot e realiza autosave a cada 15 segundos.
5. **Sincronização Master/Slave com Cloudflare KV:**
   - Sincroniza a cada 5 segundos via HTTPS com `https://makitaclicker.pages.dev/api/state`.
   - Adota upgrades e saldo da web automaticamente.
   - Envia saldo da ESP apenas se for superior (jogo offline).
6. **OTA Automático:**
   - Checa `version.json` no boot e a cada 5 minutos.
   - Atualiza tanto a lógica do jogo quanto o layout do display pelo ar!

---

## 🔌 Conexões de Hardware (ESP8266 NodeMCU)

| Componente | Pino no Componente | Pino na NodeMCU | Função |
|---|---|---|---|
| **Display LCD 20×4 I2C** | **GND** | **GND** | Terra (Negativo) |
| **Display LCD 20×4 I2C** | **VCC** | **VV (ou VU)** | Alimentação 5V direto da USB |
| **Display LCD 20×4 I2C** | **SDA** | **D2 (GPIO 4)** | Dados I2C |
| **Display LCD 20×4 I2C** | **SCL** | **D1 (GPIO 5)** | Clock I2C |
| **Botão Físico** | **Terminal 1** | **D5 (GPIO 14)** | Entrada digital (INPUT_PULLUP) |
| **Botão Físico** | **Terminal 2** | **GND** | Terra |

---

## 🛠️ Primeiro Flash Manual (se necessário)

1. Conecte a NodeMCU via cabo micro-USB.
2. Abra `codigo_esp/codigo_esp.ino` na Arduino IDE.
3. Instale as bibliotecas **ArduinoJson** e **LiquidCrystal I2C**.
4. Selecione a placa **NodeMCU 1.0 (ESP-12E Module)**.
5. Clique em **Upload**.

> 💡 Após o primeiro upload, todos os futuros firmwares são compilados automaticamente pelo CI/CD do Cloudflare Pages e instalados via OTA!
