# 🔧 MakitaClicker

> Projeto desenvolvido pelo **MakerSpace UNIFEI**
>
> **Autores:** Nicolae Maximus T. N. Lopes · Victor Augusto de A. Silvério · Oliver Daniel Schiinke

MakitaClicker é um jogo estilo *cookie clicker* físico-digital, onde o jogador acumula "Makitas" clicando em um botão físico ou através de uma interface web. O sistema roda num **Arduino Mega 2560** + **ESP8266 NodeMCU**, com painel LCD 20×4, sincronização cloud em tempo real e **atualização automática de firmware via nuvem (OTA)**.

---

## 📂 Estrutura do Repositório

Este repositório está organizado de forma modular em duas frentes independentes:

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
├── firmware/                   # 🔌 Firmware dos Microcontroladores
│   ├── codigo_esp/             # ESP8266 NodeMCU v2 (LittleFS, Wi-Fi, sync cloud, ponte OTA)
│   │   └── codigo_esp.ino
│   ├── codigo_arduino/         # Arduino Mega 2560 (botão físico, LCD 20×4 com double-buffering)
│   │   └── codigo_arduino.ino
│   ├── projeto/                # Esquemático KiCad da PCB
│   ├── teste_serial/           # Sketches de teste da comunicação Serial
│   └── GAME_DESIGN.md          # Documento de design do jogo
│
├── build.sh                    # Script CI/CD (Cloudflare Pages)
└── online/                     # Diretório de publicação (gerado pelo build)
```

### 🌐 Parte Web (`web/` + `functions/`)

A interface web e API serverless hospedadas no Cloudflare Pages. O frontend foi desacoplado em HTML, CSS e JS modular, com renderização throttled a 6 FPS para manipulações DOM de listas e persistência automática em `localStorage`. Veja detalhes em [web/README.md](web/README.md).

- **Frontend:** `https://makitaclicker.pages.dev/`
- **API:** `https://makitaclicker.pages.dev/api/state`

### 🔌 Parte Firmware (`firmware/`)

Código embarcado para Arduino Mega 2560 e ESP8266 NodeMCU, contando com persistência na memória flash via **LittleFS**, cache de cálculos de taxa de produção (MPS) e double-buffering seletivo no LCD 20×4 para eliminar cintilações. Veja detalhes em [firmware/README.md](firmware/README.md).

---

## ⚙️ Como Funciona e Sincronização Inteligente (Cloud Master / Client Slave)

O sistema opera com sincronização bidirecional onde a **nuvem (Cloudflare KV) atua como Master**:

1. **Botão Físico:** O jogador pressiona o botão conectado ao Arduino Mega (pino 7). O Mega envia `CLICK` ao ESP8266 via Serial (38400 baud). O ESP soma as Makitas instantaneamente no display LCD e acumula os cliques para sincronização com a nuvem.
2. **Interface Web:** Dispositivos acessam `https://makitaclicker.pages.dev`. O motor local interpola a produção a 60 FPS e sincroniza via REST a cada 3 segundos com o Cloudflare KV.
3. **Resolução de Conflitos (Master / Slave):**
   - Se o Cloudflare KV possuir saldo igual ou superior, seu estado é transmitido aos clientes (ESP8266 e Web) como verdade absoluta (Master), creditando os cliques pendentes enviados.
   - Caso o microcontrolador ou navegador tenha progredido offline e apresente saldo superior, o cliente realiza um push completo das suas Makitas, oficinas e melhorias permanentes para a nuvem.

```
[ Botão Físico ] ──Serial──▶ [ Arduino Mega 2560 ] ◄──Serial0 (38400)──► [ ESP8266 (LittleFS) ]
                              [ LCD 20x4 I2C ]                                     │
                              [ Display ] ◀────────────────────────────────────────┘
                                                                                   │ HTTPS (Sync 30s)
                                                                                   ▼
                                                                       [ Cloudflare Pages & KV ]
                                                                       [ makitaclicker.pages.dev ]
                                                                                   ▲
                                                                                   │ HTTPS REST (Sync 3s)
                                                                       [ Clientes Web / Celular ]
```

---

## ☁️ Pipeline CI/CD — Cloudflare Pages

A cada `git push`, o Cloudflare Pages executa [`build.sh`](build.sh):

1. Copia os assets web modulares (`web/index.html`, `style.css`, `game.js`, imagens) para `online/`
2. Compila o firmware do ESP8266 (`firmware/codigo_esp/`) via `arduino-cli` → gera `online/firmware.bin`
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
git commit -m "feat: melhorias no motor web e firmware"
git push origin main
```

O Cloudflare Pages compilará tudo automaticamente. Na próxima inicialização, o ESP8266 baixa e aplica o firmware atualizado via OTA.
