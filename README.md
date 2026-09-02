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
O jogador pressiona um botão conectado ao Arduino Mega (pino 7). O Mega envia o comando `CLICK` ao ESP8266 via Serial0 (38400 baud estável). O ESP soma as Makitas calculando os multiplicadores da árvore de habilidades e transmite o novo estado para todos os clientes web via WebSocket.

**2. Interface Web**
Qualquer dispositivo na mesma rede Wi-Fi acessa `http://esp-painel.local` (ou pelo IP). A página HTML — servida diretamente da memória flash do ESP (LittleFS) — se comunica com o ESP via WebSocket na porta 81 em tempo real.

**Produção passiva:** upgrades e melhorias permanentes geram Makitas automaticamente a cada 100ms e salvam o progresso em `/gamestate.json`.

**LCD:** O Mega exibe o saldo e a taxa de produção em tempo real num display LCD I2C 20×4 (com auto-detecção de endereço 0x27 / 0x3F), recebendo atualizações do ESP via Serial.

```
[ Botão Físico ] ──Serial──▶ [ Arduino Mega 2560 ] ──Serial0 (38400 / 115200 OTA)──▶ [ ESP8266 ]
                              [ LCD 20x4 I2C ]                                      │       │
                              [ Display ]    ◀───────────Serial─────────────────────┘       │
                                                                                            │ Wi-Fi
                                                                                    [ Clientes Web ]
                                                                                    [ WebSocket :81 ]
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
2. Inicializa EEPROM (128 bytes) e monta a partição LittleFS
3. Conecta à rede Wi-Fi "MakerSpace UNIFEI"
4. Executa checkOTA():
   a. Verifica versão do Mega -> grava se houver novidade (STK500v2)
   b. Verifica versão do LittleFS -> grava se houver novidade
   c. Verifica versão do ESP -> grava e reinicia se houver novidade
5. Executa resetMega() -> sincroniza o boot e LCD do Arduino Mega
6. Registra rotas HTTP (/, imagens)
7. Inicia WebServer (porta 80) e WebSocketServer (porta 81)
8. Carrega o estado salvo (/gamestate.json com espelhamento EEPROM)
9. Entra no loop principal (24 oficinas, 20 tecnologias, produção passiva a cada 100ms)
```

---

## ☁️ Pipeline CI/CD — Cloudflare Pages

O repositório está integrado ao **Cloudflare Pages**. A cada `git push`, o Cloudflare executa o script [`build.sh`](file:///home/vaugusto/Desktop/MakitaClicker/build.sh) automaticamente num container Linux:

### O que o `build.sh` executa (em 6 etapas)

```
[1/6] Instala arduino-cli (binário independente)
[2/6] Configura e instala os cores: esp8266:esp8266 e arduino:avr
[3/6] Instala bibliotecas: WebSockets, ArduinoJson, LiquidCrystal I2C
[4/6] Compila o sketch do ESP8266 -> gera online/firmware.bin
[5/6] Compila o sketch do Arduino Mega e converte para binário -> gera online/mega.bin
[6/6] Empacota a pasta data/ com mklittlefs -> gera online/littlefs.bin
[*]   Gera o manifesto online/version.json sincronizado com a contagem de commits git
```

Todos os artefatos são publicados diretamente na CDN global da Cloudflare:
- `https://makitaclicker.pages.dev/version.json`
- `https://makitaclicker.pages.dev/firmware.bin`
- `https://makitaclicker.pages.dev/littlefs.bin`
- `https://makitaclicker.pages.dev/mega.bin`

---

## 📁 Estrutura de Diretórios

```
MakitaClicker/
├── build.sh                    # Script de CI/CD executado pelo Cloudflare Pages
│
├── codigo_esp/                 # Código do ESP8266 (NodeMCU v2)
│   ├── codigo_esp.ino          # Firmware principal, lógica do jogo e gravador STK500v2
│   └── data/                   # Arquivos da interface Web (LittleFS)
│       ├── index.html          # Painel web do jogo
│       └── images/             # Imagens e ícones
│
├── codigo_arduino/             # Código do Arduino Mega 2560
│   └── codigo_arduino.ino      # Controle de botão físico, LCD e comunicação Serial0
│
└── online/                     # Diretório de publicação servido pela Cloudflare CDN
    ├── version.json            # Manifesto de versão
    ├── firmware.bin            # Binário do ESP8266
    ├── littlefs.bin            # Imagem do sistema de arquivos
    └── mega.bin                # Binário bruto do Arduino Mega
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
