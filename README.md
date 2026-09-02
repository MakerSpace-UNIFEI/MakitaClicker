# 🔧 MakitaClicker

> Projeto desenvolvido pelo **MakerSpace UNIFEI**
>
> **Autores:** Nicolae Maximus T. N. Lopes · Victor Augusto de A. Silvério · Oliver Daniel Schiinke

MakitaClicker é um jogo estilo *cookie clicker* físico-digital, onde o jogador acumula "Makitas" clicando em um botão físico ou através de uma interface web. O sistema roda num **Arduino Mega 2560** + **ESP8266 NodeMCU**, com painel LCD 20×4, WebServer/WebSocket em tempo real e **atualização automática e unificada de firmware via nuvem (OTA total)** para ambos os microcontroladores.

---

## 📋 Índice

- [Como Funciona](#-como-funciona)
- [Arquitetura de Hardware e Ligações](#-arquitetura-de-hardware-e-ligações)
- [Arquitetura de Software](#-arquitetura-de-software)
- [Pipeline CI/CD — Cloudflare Pages](#-pipeline-cicd--cloudflare-pages)
- [Sistema OTA Unificado (ESP8266 + LittleFS + Arduino Mega)](#-sistema-ota-unificado)
- [Estrutura de Diretórios](#-estrutura-de-diretórios)
- [Como Fazer um Release](#-como-fazer-um-release)
- [Primeiro Flash Manual (Configuração Inicial)](#-primeiro-flash-manual)

---

## ⚙️ Como Funciona

O jogo tem dois modos de interação simultâneos e integrados:

**1. Botão Físico**
O jogador pressiona o botão conectado ao Arduino Mega (pinos 6 ou 7). O Mega envia o comando `CLICK` ao ESP8266 via Serial0 (38400 baud estável). O ESP soma as Makitas instantaneamente no display LCD e acumula os cliques para sincronização em lote com a nuvem (Cloudflare KV).

**2. Interface Web (Cloudflare Pages)**
Qualquer dispositivo acessa globalmente `https://makitaclicker.pages.dev`. O site é servido diretamente pela CDN da Cloudflare e sincroniza com a API Serverless (`/api/state`) conectada ao Cloudflare KV.

**Produção passiva:** upgrades e melhorias permanentes geram Makitas automaticamente de forma contínua no Cloudflare KV e são sincronizados com o ESP8266 a cada 30 segundos.

**LCD:** O Mega exibe o saldo e a taxa de produção em tempo real num display LCD I2C 20×4 (com auto-detecção de endereço 0x27 / 0x3F), recebendo atualizações do ESP via Serial.

```
[ Botão Físico ] ──Serial──▶ [ Arduino Mega 2560 ] ◄──Serial0 (38400)──► [ ESP8266 ]
                              [ LCD 20x4 I2C ]                              │
                              [ Display ] ◀─────────────────────────────────┘
                                                                            │ HTTPS Outbound (Sync 30s)
                                                                            ▼
                                                                [ Cloudflare Pages & KV ]
                                                                [ makitaclicker.pages.dev ]
                                                                            ▲
                                                                            │ HTTPS REST (Sync 3s)
                                                                [ Clientes Web / Celular ]
```

---

## 🔌 Arquitetura de Hardware e Ligações

| Componente | Detalhe |
|---|---|
| **Arduino Mega 2560** | Controle do botão físico, display LCD e recepção de comandos |
| **ESP8266 NodeMCU v2** | Wi-Fi, WebServer, WebSocket, ponte OTA e gravador STK500v2 do Mega |
| **Display LCD I2C 20×4** | Exibe saldo e taxa (auto-detecção `0x27` ou `0x3F`) |
| **Botão Físico** | Pino digital 7 do Mega (com resistor interno `INPUT_PULLUP`) |

### Conexões entre Arduino Mega e ESP8266 (OTA Total + Jogo)

| Arduino Mega 2560 | ESP8266 NodeMCU | Função |
|---|---|---|
| **TX0 (Pino 1)** | **D6 (GPIO12 / RX)** | Mega → ESP (telemetria e respostas STK500v2) |
| **RX0 (Pino 0)** | **D7 (GPIO13 / TX)** | ESP → Mega (comandos do jogo e dados de gravação) |
| **RESET** | **D5 (GPIO14)** | Pulso de reset em modo Open-Drain seguro |
| **GND** | **GND** | Terra de referência comum |

> ℹ️ **Por que os pinos 0 e 1?** O bootloader padrão de fábrica do ATmega2560 (STK500v2) escuta na porta UART0 (pinos 0 e 1) a **115200 baud**. Conectando esses pinos, a mesma conexão serve para a jogabilidade normal (a **38400 baud** estáveis) e para a gravação remota de firmware (a **115200 baud**).

---

## 🧠 Arquitetura de Software

### Arduino Mega (`codigo_arduino/codigo_arduino.ino`)

- Lê o botão físico no Pino 7 com debounce rápido de 35ms
- Ao pressionar: incrementa saldo local instantaneamente e envia `CLICK\n` pela `Serial` (38400 baud) para o ESP
- Escuta a `Serial` aguardando pacotes `MAKITA:<saldo>,<mps>,<clickPower>,<totalOwned>` enviados pelo ESP
- Atualiza o LCD 20×4 com auto-detecção I2C, animações em tempo real, caracteres customizados (lâmina giratória, moedas, faíscas, troféu 99B) e carrossel de telemetria com porcentagem da Meta de 99 Bilhões

### ESP8266 (`codigo_esp/codigo_esp.ino`)

Ao ligar, o ESP executa em sequência:

```
1. Inicia CPU em 160MHz, Serial USB (115200) e megaSerial (38400)
2. Conecta à rede Wi-Fi "MakerSpace UNIFEI"
3. Executa checkOTA(): verifica firmware_version e regrava firmware via nuvem se houver novidade
4. Executa resetMega() -> sincroniza o boot e LCD do Arduino Mega
5. Envia "IP:pages.dev" para exibição no display LCD
6. Executa syncWithCloud() -> puxa o estado atual do Cloudflare KV via HTTPS (/api/state)
7. Entra no loop principal:
   - Recebe cliques do botão físico via Serial do Mega (resposta instantânea em 0ms)
   - Atualiza o LCD a cada 250ms com telemetria (saldo, MPS, status)
   - Sincroniza periodicamente com o Cloudflare KV a cada 30 segundos enviando cliques em lote
```

---

## ☁️ Pipeline CI/CD — Cloudflare Pages

O repositório está integrado ao **Cloudflare Pages**. A cada `git push`, o Cloudflare executa o script [`build.sh`](file:///home/vaugusto/Desktop/MakitaClicker/build.sh) automaticamente num container Linux:

### O que o `build.sh` executa:

```
[1/3] Instala arduino-cli (binário independente)
[2/3] Configura e instala o core esp8266:esp8266
[3/3] Instala biblioteca: ArduinoJson
[*]   Copia os assets da interface web (web/) para a pasta de publicação (online/)
[*]   Compila o sketch do ESP8266 -> gera online/firmware.bin
[*]   Gera o manifesto online/version.json sincronizado com a contagem de commits git
```

Todos os artefatos são publicados diretamente na CDN global da Cloudflare:
- `https://makitaclicker.pages.dev/` (Interface Web)
- `https://makitaclicker.pages.dev/api/state` (API Serverless conectada ao Cloudflare KV)
- `https://makitaclicker.pages.dev/version.json` (Manifesto OTA)
- `https://makitaclicker.pages.dev/firmware.bin` (Binário do ESP8266)

---

## 📁 Estrutura de Diretórios

```
MakitaClicker/
├── build.sh                    # Script de CI/CD executado pelo Cloudflare Pages
│
├── web/                        # Interface Web do Jogo (hospedada no Cloudflare Pages)
│   ├── index.html              # Painel web do jogo (motor gráfico 60 FPS + REST Sync)
│   └── images/                 # Imagens e ícones
│
├── functions/                  # Cloudflare Pages Functions (API Serverless)
│   └── api/
│       └── state.js            # Endpoints GET e POST /api/state conectados ao Cloudflare KV
│
├── codigo_esp/                 # Código do ESP8266 (NodeMCU v2)
│   └── codigo_esp.ino          # Cliente cloud leve, sync HTTPS 30s e ponte com o Mega
│
├── codigo_arduino/             # Código do Arduino Mega 2560
│   └── codigo_arduino.ino      # Controle de botões físicos, LCD 20x4 e telemetria
│
└── online/                     # Diretório de publicação servido pela Cloudflare CDN
    ├── index.html              # Frontend servido pela CDN
    ├── images/                 # Assets gráficos
    ├── version.json            # Manifesto de versão OTA
    └── firmware.bin            # Binário do ESP8266
```

---

## 🚀 Como Fazer um Release

Basta alterar qualquer arquivo (código do Mega, código do ESP ou HTML) e enviar para o GitHub:

```bash
git add .
git commit -m "feat: nova melhoria no jogo"
git push origin main
```

1. O Cloudflare Pages compilará os códigos do ESP8266 e do Arduino Mega automaticamente.
2. Na próxima vez que o sistema reiniciar, o ESP8266 baixa e regrava os componentes atualizados.

---

## 🛠️ Primeiro Flash Manual (Configuração Inicial)

Para preparar as placas antes de usar as atualizações automáticas:

### 1. Arduino Mega 2560
1. Conecte o Arduino Mega ao computador via USB.
2. Abra [`codigo_arduino/codigo_arduino.ino`](file:///home/vaugusto/Desktop/MakitaClicker/codigo_arduino/codigo_arduino.ino) na Arduino IDE.
3. Instale a biblioteca **LiquidCrystal I2C** pelo Library Manager se ainda não tiver.
4. Selecione a placa **Arduino Mega or Mega 2560** e a porta COM/TTY correspondente.
5. Clique em **Carregar (Upload)**.

### 2. ESP8266 NodeMCU v2
1. Conecte o ESP8266 ao computador via USB.
2. Abra [`codigo_esp/codigo_esp.ino`](file:///home/vaugusto/Desktop/MakitaClicker/codigo_esp/codigo_esp.ino) na Arduino IDE.
3. Instale as bibliotecas **WebSockets** e **ArduinoJson** pelo Library Manager.
4. Selecione a placa **NodeMCU 1.0 (ESP-12E Module)**.
5. Clique em **Carregar (Upload)**.

### 3. Conexão dos Fios
Conecte os 4 jumpers entre as placas:
- **Mega TX0 (Pino 1)** ➔ **ESP D6 (RX)**
- **Mega RX0 (Pino 0)** ➔ **ESP D7 (TX)**
- **Mega RESET** ➔ **ESP D5**
- **Mega GND** ➔ **ESP GND**

Pronto! A partir desse momento, **todas as próximas atualizações de ambos os microcontroladores acontecerão 100% no ar via Cloudflare Pages!**
