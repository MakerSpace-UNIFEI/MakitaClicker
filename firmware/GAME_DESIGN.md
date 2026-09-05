# 🎮 Arquitetura e Organização do Jogo — MakitaClicker (Edição 99B)

> Documento de referência técnica para o funcionamento, regras de negócio, mecânicas de gameplay, árvore de habilidades, persistência de dados e comunicação hardware/software.

---

## 1. ⚙️ Visão Geral do Sistema

O **MakitaClicker** é um jogo híbrido físico/digital. O hardware do **ESP8266** atua como o servidor autoritativo da partida (saldo de Makitas até 99B+, produção passiva, compras de upgrades, árvore de habilidades e persistência em flash), enquanto a **Interface Web** e o **Arduino Mega** são clientes de interação em tempo real.

```
┌─────────────────┐   Serial0 38400 (115200 OTA)  ┌─────────────────────────┐
│  Arduino Mega   │ ◄───────────────────────────► │         ESP8266         │
│ (Botão 7 + LCD) │    CLICK / MAKITA:S,M,C,O     │ (Servidor Autoritativo) │
└─────────────────┘    (STK500v2 OTA no Boot)     └────────────┬────────────┘
                                                               │ WebSocket :81
                                                               │ (HTTP :80 LittleFS)
                                                               ▼
                                                  ┌─────────────────────────┐
                                                  │   Interface Web (JS)    │
                                                  │   - Clicker & Efeitos   │
                                                  │   - 24 Oficinas (Loja)  │
                                                  │   - 20 Tecnologias      │
                                                  │   - Meta 99B & Stats    │
                                                  │   - Persistência Flash  │
                                                  │   - Botão Reset Total   │
                                                  └─────────────────────────┘
```

---

## 2. 🌳 Árvore de Habilidades & Melhorias Permanentes (20 Tecnologias até 99B)

As melhorias permanentes fornecem **multiplicadores e bônus diretos** tanto na produção manual (clique) quanto na produção passiva (MPS).

### Regras de Desbloqueio e Visibilidade (Névoa de Guerra):
- **Oculto (🔒):** O jogador ainda não atingiu o saldo acumulado de Makitas necessário nem comprou a melhoria anterior.
- **Revelado / Requisito Pendente (⏳):** Visível quando o jogador atinge o volume histórico de Makitas, mas exige a compra do nó pai na árvore.
- **Disponível (🔶):** Pré-requisito atendido e saldo suficiente para compra.
- **Adquirido (🟢):** Bônus ativo de forma permanente.

### Tabela Completa de Habilidades (20 Tecnologias):

| ID | Nome | Ícone | Custo | Req. Makitas | Pré-requisito | Efeito Real no Jogo |
|---|---|:---:|---|---|---|---|
| `perm_lubrificante` | Óleo Sintético Premium | 🛢️ | 25 | 10 | *Nenhum* | **+10% MPS Global** em todas as fontes |
| `perm_disco_diamante` | Disco Diamantado Reforçado | 💠 | 100 | 50 | `perm_lubrificante` | **+1.0 Poder de Clique** (Clique base passa de 1 para 2) |
| `perm_motor_brushless` | Motor Brushless Industrial | ⚡ | 300 | 150 | `perm_lubrificante` | **2x Produção Base** de todas as oficinas |
| `perm_empunhadura` | Empunhadura Ergonômica Pro | 🧤 | 600 | 250 | `perm_disco_diamante` | **Clique Sinergético**: Cada clique gera +5% do MPS atual |
| `perm_bateria_lítio` | Bateria Makita 40V Max XGT | 🔋 | 1.500 | 600 | `perm_motor_brushless` | **+25% MPS Global** permanente |
| `perm_ia_maker` | MakerBot Autônomo com IA | 🤖 | 5.000 | 2.000 | `perm_bateria_lítio` | **+50% MPS Global** permanente |
| `perm_refrigeracao` | Sistema Criogênico de Nitrogênio | ❄️ | 15.000 | 6.000 | `perm_motor_brushless` | **+20% MPS Global** permanente |
| `perm_titanio` | Lâmina de Titânio a Plasma | 🗡️ | 35.000 | 12.000 | `perm_disco_diamante` | **+3.0 Poder de Clique** manual |
| `perm_overclock` | Circuito de Overclock Extremo | ⚡ | 100.000 | 30.000 | `perm_empunhadura` | **Sinergia Dobrada**: Cada clique gera **+10% do MPS** |
| `perm_nanobots` | Enxame de Nanobots Montadores | 🔬 | 250.000 | 80.000 | `perm_ia_maker` | **+75% MPS Global** permanente |
| `perm_singularidade` | Núcleo de Singularidade Maker | 🌌 | 1.000.000 | 300.000 | `perm_nanobots` | **+150% MPS Global** e triplica o clique base |
| `perm_plasma_cutter` | Cortador a Plasma Estelar | ✨ | 5.000.000 | 1.500.000 | `perm_titanio` | **+25.0 Poder de Clique** base |
| `perm_fusao_fria` | Reator de Fusão Fria Compacta | 🧪 | 20.000.000 | 6.000.000 | `perm_singularidade` | **+100% MPS Global** permanente |
| `perm_hiperconducao` | Hipercondutores de Grafeno | ⚡ | 80.000.000 | 25.000.000 | `perm_fusao_fria` | **3x Produção Base** de todas as oficinas |
| `perm_sinergia_quantica` | Sinergia Quântica de Impacto | 🔮 | 300.000.000 | 100.000.000 | `perm_overclock` | **Sinergia Quântica**: Cada clique gera **+20% do MPS** |
| `perm_laser_gama` | Emissor Laser de Raios Gama | 🌠 | 1.200.000.000 | 400.000.000 | `perm_plasma_cutter` | **+200.0 Poder de Clique** base |
| `perm_taquions` | Reator de Táquions Espacial | ⏳ | 5.000.000.000 | 1.500.000.000 | `perm_hiperconducao` | **+200% MPS Global** permanente |
| `perm_materia_escura` | Condensador de Matéria Escura | 🌑 | 20.000.000.000 | 6.000.000.000 | `perm_taquions` | **+300% MPS Global** permanente |
| `perm_hiper_clique` | Martelo de Fótons Subatômico | 🔨 | 50.000.000.000 | 15.000.000.000 | `perm_laser_gama` | **10x Multiplicador de Clique** total |
| `perm_onipotencia_maker` | Onipotência Maker Cósmica | 👑 | 99.000.000.000 | 35.000.000.000 | `perm_materia_escura` | **+500% MPS Global**, 4x oficinas e **+30% MPS por clique** |

---

## 3. 🏭 Loja de Upgrades (24 Oficinas de Produção Passiva)

- **Fórmula de Custo por Unidade:**
  $$\text{Custo}(n) = \lceil \text{baseCost} \times \text{growth}^n \rceil$$
- **Teto Máximo:** 100 unidades por tipo de oficina (`MAX_OWNED = 100`).
- **Modos de Compra:** `1x`, `10x`, `MAX` (calcula o lote máximo acessível com o saldo atual sem ultrapassar 100).

### Tabela Completa de 24 Upgrades da Loja:

| ID | Nome | Ícone | Custo Base | Growth | Produção (MPS) |
|---|---|:---:|---|---|---|
| `upgrade1` | Bancada Básica | ⚙️ | 10 | 1.10 | +0.1 MPS |
| `upgrade_1mps` | Esmerilhadeira Manual | 🪚 | 100 | 1.12 | +1.0 MPS |
| `upgrade_2mps` | Serra Mármore 1400W | ⚡ | 250 | 1.12 | +2.0 MPS |
| `upgrade_5mps` | Torno Mecânico | 🔧 | 750 | 1.13 | +5.0 MPS |
| `upgrade_10mps` | Fresadora CNC | 🎛️ | 1.800 | 1.13 | +10.0 MPS |
| `upgrade_15mps` | Robô de Solda Industrial | 🦾 | 3.500 | 1.14 | +15.0 MPS |
| `upgrade_20mps` | Cortadora a Laser CO2 | 🔴 | 6.000 | 1.14 | +20.0 MPS |
| `upgrade_25mps` | Prensa Hidráulica 50T | 🏗️ | 10.000 | 1.14 | +25.0 MPS |
| `upgrade_30mps` | Impressora 3D de Metal | 🖨️ | 16.000 | 1.15 | +30.0 MPS |
| `upgrade_50mps` | Linha de Montagem IA | 🤖 | 35.000 | 1.15 | +50.0 MPS |
| `upgrade_100mps` | Mega Fábrica Makita | 🏭 | 100.000 | 1.15 | +100.0 MPS |
| `upgrade_200mps` | Reator de Fusão Maker | ☢️ | 300.000 | 1.16 | +200.0 MPS |
| `upgrade_500mps` | Estação Espacial Orbital | 🛸 | 1.000.000 | 1.16 | +500.0 MPS |
| `upgrade_1200mps` | Acelerador de Partículas Makita | 🌀 | 3.500.000 | 1.16 | +1.200.0 MPS |
| `upgrade_3000mps` | Mineração de Asteroides | ☄️ | 12.000.000 | 1.16 | +3.000.0 MPS |
| `upgrade_8000mps` | Usina Vulcânica Maker | 🌋 | 40.000.000 | 1.17 | +8.000.0 MPS |
| `upgrade_20kmps` | Forja de Antimatéria | ⚛️ | 150.000.000 | 1.17 | +20.000.0 MPS |
| `upgrade_60kmps` | Computador Quântico UNIFEI | 💻 | 500.000.000 | 1.17 | +60.000.0 MPS |
| `upgrade_180kmps` | Esfera de Dyson Makita | ☀️ | 1.800.000.000 | 1.17 | +180.000.0 MPS |
| `upgrade_500kmps` | Portal Dimensional Maker | 🌌 | 6.000.000.000 | 1.18 | +500.000.0 MPS |
| `upgrade_1500kmps` | Manipulador Gravitacional | 🪐 | 20.000.000.000 | 1.18 | +1.500.000.0 MPS |
| `upgrade_5000kmps` | Motor de Dobra Espacial | 🚀 | 60.000.000.000 | 1.18 | +5.000.000.0 MPS |
| `upgrade_15000kmps` | Fábrica de Realidade Paralela | 🔮 | 200.000.000.000 | 1.19 | +15.000.000.0 MPS |
| `upgrade_50000kmps` | Big Bang Maker Contínuo | 💥 | 800.000.000.000 | 1.19 | +50.000.000.0 MPS |

---

## 4. 💾 Persistência de Dados e Reset

- **Arquivo no LittleFS:** `/gamestate.json`
- **Autosave:** O estado completo da partida (saldo de Makitas em double, 24 upgrades e 20 flags permanentes) é gravado automaticamente a cada **5 segundos** e imediatamente após compras.
- **Espelhamento EEPROM:** Bitmask de 32 bits com checksum XOR e magic number `0x4D4B5433` ("MKT3") restaurado automaticamente em caso de indisponibilidade da flash.
- **Carregamento Automático:** No boot, o ESP executa `loadGameState()` e restaura a partida exatamente de onde parou.
- **Comando de Reset Total:** Via WebSocket (`RESET`), apaga `/gamestate.json`, zera a EEPROM e a RAM, e transmite o estado limpo para o display LCD e navegadores.

---

## 5. 📡 Protocolo de Comunicação em Tempo Real

### WebSocket (Porta 81) — Cliente ➔ ESP:
- `CLICK`: Registra clique manual.
- `BUY:<upgradeId>:<qty|max>`: Solicita compra de lote da oficina.
- `PERM_BUY:<permId>:<cost>`: Compra e ativa melhoria permanente na árvore.
- `RESET`: Reinicia completamente o progresso do jogo.

### Serial UART0 (38400 baud) — ESP ➔ Arduino Mega:
- Formato: `MAKITA:<saldo_float>,<mps_float>,<clickPower_float>,<totalOwned_int>\n`
- Exemplo: `MAKITA:99000000000.0,15000000.0,2500.0,240`

### Serial UART0 (38400 baud) — Arduino Mega ➔ ESP:
- Formato: `CLICK\n` (Acionado pelo botão no pino 7 com debounce de 35ms).

---

## 6. 📺 Display LCD I2C 20×4 no Arduino Mega

- **Linha 0:** Cabeçalho animado com lâminas giratórias (`MAKITA CLICKER`) ou faíscas em clique.
- **Linha 1:** Saldo formatado (`[Coin] Saldo: 99.0B MKT` ou `[Trophy] Saldo: 99.0B MKT!`).
- **Linha 2:** Poder de corte e taxa de produção compactados (`[Bolt]+2.5k | [Factory] 15.0M/s`).
- **Linha 3:** Feedback instantâneo (`>> CORTE EFETUADO! <<`) ou carrossel rotativo:
  - `Site: makitaclicker.pages.dev`
  - `Top: <nome> (<saldo>)` (Líder global recebido do backend)
  - `Oficinas: X un.`
  - `Meta 99B: XX.X%` (ou `** META 99B FEITA! **`)
  - `FW: vXX (OTA Ativo)`
  - `IP: 192.168.x.x`
  - `MakerSpace UNIFEI`
