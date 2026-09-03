# 🔧 MakitaClicker

> Projeto desenvolvido pelo **MakerSpace UNIFEI**
>
> **Autores:** Nicolae Maximus T. N. Lopes · Victor Augusto de A. Silvério · Oliver Daniel Schiinke

MakitaClicker é um jogo estilo *cookie clicker* físico-digital, onde o jogador acumula "Makitas" clicando em um botão físico ou através de uma interface web. O sistema roda de forma autônoma num **ESP8266 NodeMCU**, com painel LCD 20×4 I2C, sincronização cloud em tempo real e **atualização automática de firmware via nuvem (OTA)**.

---

## 📂 Estrutura do Repositório

```
MakitaClicker/
│
├── web/                        # 🌐 Interface Web (Cloudflare Pages)
│   ├── index.html              # Marcação DOM semântica e limpa
│   ├── style.css               # Folha de estilo completa e responsiva
│   ├── game.js                 # Motor de jogo 60 FPS, reconciliação e LocalStorage
│   ├── images/                 # Imagens e ícones
│   └── makitaCoracao.png       # Logo
│
├── functions/                  # ☁️ API Serverless (Cloudflare Pages Functions)
│   └── api/
│       └── state.js            # Endpoints GET e POST /api/state (Cloudflare KV Master)
│
├── firmware/                   # 🔌 Firmware do Microcontrolador
│   ├── codigo_esp/             # ESP8266 NodeMCU (Display LCD 20x4, Botão D5, LittleFS, Wi-Fi, OTA)
│   │   └── codigo_esp.ino
│   ├── projeto/                # Esquemático KiCad da PCB
│   └── GAME_DESIGN.md          # Documento de design do jogo
│
├── build.sh                    # Script CI/CD (Cloudflare Pages)
└── online/                     # Diretório de publicação (gerado pelo build)
```

### 🌐 Parte Web (`web/` + `functions/`)

A interface web e API serverless hospedadas no Cloudflare Pages. O frontend é desacoplado em HTML, CSS e JS modular, com renderização a 60 FPS e persistência automática em `localStorage`. Veja detalhes em [web/README.md](web/README.md).

- **Frontend:** `https://makitaclicker.pages.dev/`
- **API:** `https://makitaclicker.pages.dev/api/state`

### 🔌 Parte Firmware (`firmware/`)

Firmware único e autônomo para ESP8266 NodeMCU. Controla diretamente o display LCD 20×4 I2C e o botão físico no pino D5, com persistência na memória flash via **LittleFS**, cache de cálculos (MPS) e double-buffering seletivo no LCD para eliminar cintilações. Veja detalhes em [firmware/README.md](firmware/README.md).

---

## ⚙️ Como Funciona e Sincronização Inteligente (Cloud Master / Client Slave)

O sistema opera com sincronização bidirecional onde a **nuvem (Cloudflare KV) e a Web atuam como Master**:

1. **Botão Físico:** O jogador pressiona o botão conectado ao pino D5 da NodeMCU. O ESP processa o clique instantaneamente (0ms), soma as Makitas no LCD e acumula os cliques para envio à nuvem.
2. **Interface Web:** Dispositivos acessam `https://makitaclicker.pages.dev`. O motor local roda a 60 FPS e sincroniza via REST a cada 5 segundos com o Cloudflare KV.
3. **Resolução de Conflitos (Master / Slave):**
   - O Cloudflare KV dita o estado para todos os clientes conectados.
   - A ESP adota sempre o saldo e oficinas da Web/KV.
   - A única exceção é se a ESP acumular saldo superior no jogo offline (botão físico), caso em que a nuvem aceita o saldo maior da ESP.

```
[ Botão Físico (D5) ] ──▶ [ ESP8266 NodeMCU ] ──I2C──▶ [ LCD 20x4 ]
                                 │
                                 │ HTTPS (Sync 5s)
                                 ▼
                     [ Cloudflare Pages & KV ]
                     [ makitaclicker.pages.dev ]
                                 ▲
                                 │ HTTPS REST (Sync 5s)
                     [ Clientes Web / Celular ]
```

---

## ☁️ Pipeline CI/CD — Cloudflare Pages

A cada `git push`, o Cloudflare Pages executa [`build.sh`](build.sh):

1. Copia os assets web modulares (`web/index.html`, `style.css`, `game.js`, imagens) para `online/`
2. Instala dependências do Arduino (`ArduinoJson`, `LiquidCrystal I2C`) e compila o firmware do ESP8266 via `arduino-cli` → gera `online/firmware.bin`
3. Gera o manifesto `online/version.json` com versionamento automático por commits

Artefatos publicados na CDN global:
- `https://makitaclicker.pages.dev/` — Interface Web
- `https://makitaclicker.pages.dev/api/state` — API Serverless (Cloudflare KV)
- `https://makitaclicker.pages.dev/version.json` — Manifesto OTA
- `https://makitaclicker.pages.dev/firmware.bin` — Binário do ESP8266

---

## 🚀 Como Fazer um Release

```bash
git add .
git commit -m "feat: atualizacao do sistema"
git push origin main
```

O Cloudflare Pages compilará tudo automaticamente. Na próxima inicialização (ou no ciclo automático de 5 minutos), o ESP8266 baixa e aplica o firmware atualizado via OTA!
