# 🔌 Firmware — MakitaClicker

Código embarcado para os microcontroladores do MakitaClicker.

---

## 📂 Estrutura

```
firmware/
├── codigo_esp/                 # ESP8266 NodeMCU v2
│   └── codigo_esp.ino          # Cliente cloud, LittleFS, cache MPS, sync HTTPS 30s, ponte OTA
│
├── codigo_arduino/             # Arduino Mega 2560
│   └── codigo_arduino.ino      # Botão físico (25ms debounce), LCD 20×4 com double-buffering
│
├── projeto/                    # Esquemático KiCad da PCB
│   ├── projeto.kicad_pcb
│   ├── projeto.kicad_pro
│   └── projeto.kicad_sch
│
├── teste_serial/               # Sketches de teste da comunicação Serial
│   ├── teste_serial_arduino/
│   └── teste_serial_esp/
│
└── GAME_DESIGN.md              # Mecânicas, upgrades, skill tree e protocolo
```

---

## 🧠 Arquitetura de Software

### Arduino Mega (`codigo_arduino/codigo_arduino.ino`)

- **Debounce de 25ms:** Leitura confiável de 2 botões físicos (Pino 7 e Pino 6) imune a ruídos mecânicos de microswitches.
- **Resposta instantânea (0ms):** Incrementa saldo local imediatamente e dispara `CLICK\n` pela Serial (38400 baud).
- **Double-Buffering no LCD 20×4:** Auto-detecção I2C e reescrita seletiva apenas das linhas cujo texto ou animação foi alterado, eliminando completamente cintilações e sobrecarga no barramento I2C.

### ESP8266 (`codigo_esp/codigo_esp.ino`)

1. **CPU a 160MHz:** Temporização estável para comunicação serial e conexões de rede.
2. **Persistência na Flash (LittleFS):** Carrega `/gamestate.json` no boot e executa autosave a cada 15 segundos para reter o progresso mesmo sem Wi-Fi.
3. **Cálculos com Cache:** Taxa de produção passiva (MPS) e poder de clique mantidos em variáveis de cache (`cachedMps`, `cachedClickPower`), eliminando chamadas repetitivas e loops no ciclo principal.
4. **Sincronização Master/Slave com a Nuvem:** Sincroniza a cada 30 segundos via HTTPS com o Cloudflare KV:
   - Se o ESP tiver progresso offline superior, atualiza a nuvem com seu estado.
   - Se a nuvem estiver adiante, a nuvem é Master e atualiza o ESP.
5. **OTA Automático:** Verifica `version.json` e regrava o binário no ar se houver nova versão publicada.

---

## 🔌 Conexões entre Arduino Mega e ESP8266

| Arduino Mega 2560 | ESP8266 NodeMCU | Função |
|---|---|---|
| **TX0 (Pino 1)** | **D6 (GPIO12 / RX)** | Mega → ESP (telemetria) |
| **RX0 (Pino 0)** | **D7 (GPIO13 / TX)** | ESP → Mega (comandos) |
| **RESET** | **D5 (GPIO14)** | Pulso de reset Open-Drain |
| **GND** | **GND** | Terra comum |

---

## 🛠️ Primeiro Flash Manual

### 1. Arduino Mega 2560
1. Conecte via USB
2. Abra `codigo_arduino/codigo_arduino.ino` na Arduino IDE
3. Instale a biblioteca **LiquidCrystal I2C**
4. Selecione **Arduino Mega or Mega 2560** e a porta correspondente
5. Clique em **Upload**

### 2. ESP8266 NodeMCU v2
1. Conecte via USB
2. Abra `codigo_esp/codigo_esp.ino` na Arduino IDE
3. Instale **ArduinoJson**
4. Selecione **NodeMCU 1.0 (ESP-12E Module)**
5. Clique em **Upload**

### 3. Conexão dos Fios
Conecte os 4 jumpers:
- **Mega TX0 (Pino 1)** ➔ **ESP D6 (RX)**
- **Mega RX0 (Pino 0)** ➔ **ESP D7 (TX)**
- **Mega RESET** ➔ **ESP D5**
- **Mega GND** ➔ **ESP GND**

A partir desse momento, todas as atualizações do ESP acontecem via OTA pelo Cloudflare Pages.
