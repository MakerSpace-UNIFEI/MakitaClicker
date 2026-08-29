# 🔧 MakitaClicker

> Projeto desenvolvido pelo **MakerSpace UNIFEI**
>
> **Autores:** Nicolae Maximus T. N. Lopes · Victor Augusto de A. Silvério · Oliver Daniel Schiinke

MakitaClicker é um jogo estilo *cookie clicker* físico-digital, onde o jogador acumula "Makitas" clicando em um botão físico ou através de uma interface web. O sistema roda num **Arduino Mega** + **ESP8266**, com um painel LCD, WebServer local e atualização automática de firmware via internet (OTA).

---

## 📋 Índice

- [Como Funciona](#-como-funciona)
- [Arquitetura de Hardware](#-arquitetura-de-hardware)
- [Arquitetura de Software](#-arquitetura-de-software)
- [Pipeline CI/CD — Cloudflare Pages](#-pipeline-cicd--cloudflare-pages)
- [Sistema OTA (Over-The-Air Update)](#-sistema-ota-over-the-air-update)
- [Estrutura de Diretórios](#-estrutura-de-diretórios)
- [Como Fazer um Release](#-como-fazer-um-release)
- [Configuração Inicial](#-configuração-inicial)

---

## ⚙️ Como Funciona

O jogo tem dois modos de interação simultâneos:

**1. Botão Físico**
O jogador pressiona um botão conectado ao Arduino Mega (pino 7). O Mega envia o comando `CLICK` ao ESP via Serial (pinos 18/19). O ESP soma +1 Makita e transmite o novo estado para todos os clientes web via WebSocket.

**2. Interface Web**
Qualquer dispositivo na mesma rede Wi-Fi acessa `http://esp-painel.local` (ou pelo IP). A página HTML — servida diretamente da memória flash do ESP (LittleFS) — se comunica com o ESP via WebSocket na porta 81 em tempo real.

**Produção passiva:** upgrades comprados geram Makitas automaticamente a cada 100ms.

**LCD:** O Mega exibe o saldo e a taxa de produção em tempo real num display LCD I2C 20×4, recebendo atualizações do ESP via Serial.

```
[ Botão Físico ] ──Serial──▶ [ Arduino Mega ] ──Serial──▶ [ ESP8266 ]
                              [ LCD 20x4 I2C ]              │       │
                              [ Display ]    ◀──Serial──────┘       │
                                                                     │ Wi-Fi
                                                             [ Clientes Web ]
                                                             [ WebSocket :81 ]
```

---

## 🔌 Arquitetura de Hardware

| Componente | Detalhe |
|---|---|
| **Arduino Mega 2560** | Controle do botão físico e LCD |
| **ESP8266 NodeMCU v2** | Wi-Fi, WebServer, WebSocket, OTA |
| **Display LCD I2C 20×4** | Exibe saldo e taxa (endereço `0x27`) |
| **Botão** | Pino digital 7 do Mega, com `INPUT_PULLUP` |

### Conexões entre Mega e ESP8266

| Mega | ESP8266 | Função |
|---|---|---|
| TX1 (pino 18) | D6 (RX) | Mega → ESP |
| RX1 (pino 19) | D7 (TX) | ESP → Mega |
| GND | GND | Terra comum |

> ⚠️ A comunicação é a **9600 baud** usando o hardware Serial1 do Mega e SoftwareSerial no ESP.

---

## 🧠 Arquitetura de Software

### Arduino Mega (`codigo_arduino/codigo_arduino.ino`)

- Lê o botão físico com debounce de 40ms
- Ao pressionar: envia `CLICK\n` pelo Serial1 para o ESP
- Escuta o Serial1 aguardando pacotes `MAKITA:<valor>,<mps>` enviados pelo ESP
- Atualiza o LCD com saldo e taxa de produção recebidos

### ESP8266 (`codigo_esp/codigo_esp.ino`)

Ao ligar, o ESP executa em ordem:

```
1. Inicia Serial + SoftwareSerial (Mega)
2. Monta o LittleFS (sistema de arquivos na flash)
3. Conecta ao Wi-Fi "MakerSpace UNIFEI"
4. Executa checkOTA() → checa e aplica atualizações
5. Registra rotas HTTP (/, arquivos)
6. Inicia WebServer na porta 80
7. Inicia WebSocketServer na porta 81
8. Entra no loop principal
```

**Loop principal:**
- `server.handleClient()` — serve requisições HTTP
- `webSocket.loop()` — processa mensagens WebSocket
- `MDNS.update()` — mantém o hostname `esp-painel.local`
- Lê comandos do Mega via SoftwareSerial
- Produção passiva a cada 100ms
- Broadcast do estado a cada 500ms (se houver produção ativa)

**Protocolo WebSocket (mensagens do cliente → ESP):**

| Mensagem | Ação |
|---|---|
| `CLICK` | +1 Makita |
| `BUY:upgrade1:1` | Compra 1 unidade do upgrade |
| `BUY:upgrade1:10` | Compra 10 unidades |
| `BUY:upgrade1:max` | Compra o máximo possível |

**Protocolo Serial ESP → Mega:**

```
MAKITA:<saldo_inteiro>,<mps_float>
```
Exemplo: `MAKITA:1500,3.2`

---

## ☁️ Pipeline CI/CD — Cloudflare Pages

O repositório está integrado ao **Cloudflare Pages**. A cada `git push` para a branch principal, o Cloudflare executa o `build.sh` automaticamente num container Debian e publica os artefatos gerados na CDN global.

### Configuração no painel Cloudflare Pages

| Campo | Valor |
|---|---|
| Framework preset | `None` |
| Build command | `bash build.sh` |
| Build output directory | `online` |
| Root directory | `/` |

### O que o `build.sh` faz (em ordem)

```
[1/5] Instala o arduino-cli (binário standalone)
[2/5] Adiciona o repositório ESP8266 e instala o core esp8266:esp8266
[3/5] Instala as bibliotecas: WebSockets e ArduinoJson
[4/5] Compila o sketch do ESP8266 → gera firmware.bin em ./online/
[5/5] Localiza o mklittlefs instalado pelo core e empacota
       codigo_esp/data/ → gera littlefs.bin em ./online/
```

Após o build, a pasta `online/` é publicada na CDN e fica acessível publicamente em:

```
https://makitaclicker.pages.dev/firmware.bin
https://makitaclicker.pages.dev/littlefs.bin
https://makitaclicker.pages.dev/version.json
```

---

## 📡 Sistema OTA (Over-The-Air Update)

O ESP verifica e aplica atualizações automaticamente **a cada boot**, sem necessidade de cabo USB.

### Como funciona

```
ESP liga
   │
   ▼
Conecta ao Wi-Fi
   │
   ▼
GET https://makitaclicker.pages.dev/version.json
   │
   ├── fs_version remoto > CURRENT_FS_VER local?
   │      └── SIM → baixa littlefs.bin e grava na flash (sem reboot)
   │
   └── firmware_version remoto > CURRENT_FIRMWARE_VER local?
          └── SIM → baixa firmware.bin e grava na flash → reboot automático
   │
   ▼ (nenhuma atualização necessária)
Sobe WebServer + WebSocket normalmente
```

### Manifesto de versão (`online/version.json`)

```json
{
  "firmware_version": 1,
  "fs_version": 2,
  "firmware_url": "https://makitaclicker.pages.dev/firmware.bin",
  "fs_url": "https://makitaclicker.pages.dev/littlefs.bin"
}
```

### Constantes de versão no firmware (`codigo_esp.ino`)

```cpp
#define CURRENT_FIRMWARE_VER 1   // versão do firmware gravado na placa
#define CURRENT_FS_VER       2   // versão do LittleFS gravado na placa
```

> ⚠️ **Regra de ouro:** Os `#define` no `.ino` devem sempre refletir o que está gravado na placa. Se `version.json` diz `fs_version: 3`, o `.ino` deve ter `CURRENT_FS_VER 3` — caso contrário o ESP entrará em loop de atualização infinita.

### Detalhes técnicos do OTA

- Usa `WiFiClientSecure` com `setInsecure()` para HTTPS sem certificado (economiza RAM)
- **FS update:** `ESPhttpUpdate.updateFS()` com `rebootOnUpdate(false)` — atualiza o sistema de arquivos sem reiniciar, permitindo aplicar firmware logo em seguida
- **FW update:** `ESPhttpUpdate.update()` com `rebootOnUpdate(true)` — ao concluir com sucesso, o ESP reinicia automaticamente já com o novo firmware
- Ordem importa: FS primeiro, firmware depois — garante que após o reboot o novo firmware já encontra o novo FS pronto

---

## 📁 Estrutura de Diretórios

```
MakitaClicker/
├── build.sh                    # Script de CI/CD executado pelo Cloudflare Pages
│
├── codigo_esp/                 # Sketch do ESP8266 (NodeMCU v2)
│   ├── codigo_esp.ino          # Código principal: Wi-Fi, WebServer, WebSocket, OTA
│   └── data/                   # Conteúdo do LittleFS (sistema de arquivos da flash)
│       ├── index.html          # Interface web do jogo
│       └── images/             # Assets (ex: makitaCoracao.png)
│
├── codigo_arduino/             # Sketch do Arduino Mega 2560
│   └── codigo_arduino.ino      # Botão físico + LCD + comunicação com ESP
│
└── online/                     # Pasta de saída pública (servida pelo Cloudflare Pages)
    ├── version.json            # Manifesto de versões — lido pelo ESP a cada boot
    ├── firmware.bin            # (gerado pelo build) Firmware do ESP compilado
    └── littlefs.bin            # (gerado pelo build) Imagem do sistema de arquivos
```

---

## 🚀 Como Fazer um Release

Para publicar uma atualização que o ESP vai baixar automaticamente:

### 1. Faça as alterações no código ou nos arquivos web

Edite `codigo_esp.ino` e/ou os arquivos em `codigo_esp/data/`.

### 2. Incremente as versões

**Se mudou o código `.ino`** → incremente `firmware_version` no `version.json`:
```json
"firmware_version": 2
```
E no `.ino`:
```cpp
#define CURRENT_FIRMWARE_VER 2
```

**Se mudou arquivos em `data/`** (HTML, imagens) → incremente `fs_version`:
```json
"fs_version": 3
```
E no `.ino`:
```cpp
#define CURRENT_FS_VER 3
```

> Pode incrementar os dois ao mesmo tempo se mudou ambos.

### 3. Faça o push

```bash
git add .
git commit -m "release: fw=2 fs=3 - descrição da mudança"
git push
```

### 4. Aguarde o build (~2 minutos)

O Cloudflare Pages compila tudo automaticamente. Acompanhe em [dash.cloudflare.com](https://dash.cloudflare.com).

### 5. Reinicie o ESP

Na próxima vez que o ESP ligar (ou ao apertar reset), ele detecta a nova versão e se atualiza. O Serial Monitor mostrará:

```
[OTA] Local FW=1 FS=2 | Remoto FW=2 FS=3
[OTA] Atualizando LittleFS...
[OTA] LittleFS atualizado com sucesso.
[OTA] Atualizando Firmware...
← reboot automático
```

---

## 🛠️ Configuração Inicial

Para gravar o firmware pela primeira vez (único flash manual necessário):

### Pré-requisitos

- Arduino IDE com suporte ao ESP8266 (`https://arduino.esp8266.com/stable/package_esp8266com_index.json`)
- Bibliotecas instaladas via Library Manager:
  - `WebSockets` by Markus Sattler
  - `ArduinoJson` by Benoit Blanchon
  - `LiquidCrystal I2C` by Frank de Brabander (para o Mega)

### Flash do ESP8266

1. Abra `codigo_esp/codigo_esp.ino` na Arduino IDE
2. Selecione a placa: **NodeMCU 1.0 (ESP-12E Module)**
3. Selecione a porta COM correta
4. Grave (`Ctrl+U`)
5. Abra o Serial Monitor a **115200 baud** para acompanhar o boot e o OTA

### Flash do Arduino Mega

1. Abra `codigo_arduino/codigo_arduino.ino` na Arduino IDE
2. Selecione a placa: **Arduino Mega or Mega 2560**
3. Grave normalmente via USB

Após o primeiro flash do ESP, todos os updates futuros acontecem automaticamente via OTA a cada boot.
