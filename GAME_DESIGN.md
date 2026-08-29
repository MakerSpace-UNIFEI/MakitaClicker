# 🎮 Arquitetura e Organização do Jogo — MakitaClicker

> Documento de referência técnica para o funcionamento, regras de negócio, mecânicas de gameplay, árvore de habilidades, persistência de dados e comunicação hardware/software.

---

## 1. ⚙️ Visão Geral do Sistema

O **MakitaClicker** é um jogo híbrido físico/digital. O hardware do **ESP8266** atua como o servidor autoritativo da partida (saldo de Makitas, produção passiva, compras de upgrades, árvore de habilidades e persistência em flash), enquanto a **Interface Web** e o **Arduino Mega** são clientes de interação em tempo real.

```
┌─────────────────┐      Serial0 115200 baud      ┌─────────────────────────┐
│  Arduino Mega   │ ◄───────────────────────────► │         ESP8266         │
│ (Botão 7 + LCD) │      CLICK / MAKITA:X,MPS     │ (Servidor Autoritativo) │
└─────────────────┘    (STK500v2 OTA no Boot)     └────────────┬────────────┘
                                                               │ WebSocket :81
                                                               │ (HTTP :80 LittleFS)
                                                               ▼
                                                  ┌─────────────────────────┐
                                                  │   Interface Web (JS)    │
                                                  │   - Clicker & Efeitos   │
                                                  │   - Loja de Oficinas    │
                                                  │   - Árvore Habilidades  │
                                                  │   - Persistência Flash  │
                                                  │   - Botão Reset Total   │
                                                  └─────────────────────────┘
```

---

## 2. 🌳 Árvore de Habilidades & Melhorias Permanentes

As melhorias permanentes fornecem **multiplicadores e bônus diretos** tanto na produção manual (clique) quanto na produção passiva (MPS).

### Regras de Desbloqueio e Visibilidade (Névoa de Guerra):
- **Oculto (🔒):** O jogador ainda não atingiu o saldo acumulado de Makitas necessário nem comprou a melhoria anterior.
- **Revelado / Requisito Pendente (⏳):** Visível quando o jogador atinge o volume histórico de Makitas, mas exige a compra do nó pai na árvore.
- **Disponível (🔶):** Pré-requisito atendido e saldo suficiente para compra.
- **Adquirido (🟢):** Bônus ativo de forma permanente.

### Tabela de Habilidades e Efeitos Reais:

| ID | Nome | Ícone | Custo | Req. Makitas | Pré-requisito | Efeito Real no Jogo |
|---|---|:---:|---|---|---|---|
| `perm_lubrificante` | Óleo Sintético Premium | 🛢️ | 25 | 10 | *Nenhum* | **+10% MPS Global** em todas as fontes |
| `perm_disco_diamante` | Disco Diamantado Reforçado | 💠 | 100 | 50 | `perm_lubrificante` | **+1.0 Poder de Clique** (Clique base passa de 1 para 2) |
| `perm_motor_brushless` | Motor Brushless Industrial | ⚡ | 300 | 150 | `perm_lubrificante` | **2x Produção Base** de todas as oficinas/upgrades |
| `perm_empunhadura` | Empunhadura Ergonômica Pro | 🧤 | 600 | 250 | `perm_disco_diamante` | **Clique Sinergético**: Cada clique gera +5% do MPS atual |
| `perm_bateria_lítio` | Bateria Makita 40V Max XGT | 🔋 | 1500 | 600 | `perm_motor_brushless` | **+25% MPS Global** permanente |
| `perm_ia_maker` | MakerBot Autônomo com IA | 🤖 | 5000 | 2000 | `perm_bateria_lítio` | **+50% MPS Global** permanente |

---

## 3. 🏭 Loja de Upgrades (Produção Passiva)

- **Fórmula de Custo por Unidade:**
  $$\text{Custo}(n) = \lceil \text{baseCost} \times \text{growth}^n \rceil$$
  *(com $\text{baseCost} = 10$ e $\text{growth} = 1.10$)*
- **Teto Máximo:** 100 unidades por tipo de oficina (`MAX_OWNED = 100`).
- **Modos de Compra:** `1x`, `10x`, `MAX` (calcula o lote máximo acessível com o saldo atual sem ultrapassar 100).

---

## 4. 💾 Persistência de Dados e Reset

- **Arquivo no LittleFS:** `/gamestate.json`
- **Autosave:** O estado completo da partida (saldo de Makitas, quantidade de upgrades e flags das habilidades permanentes) é gravado automaticamente a cada **10 segundos** e imediatamente após compras.
- **Carregamento Automático:** No boot, o ESP executa `loadGameState()` e restaura a partida exatamente de onde parou.
- **Comando de Reset Total:** Via WebSocket (`RESET`), apaga `/gamestate.json`, zera o estado em memória e transmite o estado limpo para o display LCD e navegadores.

---

## 5. 📡 Protocolo de Comunicação em Tempo Real

### WebSocket (Porta 81) — Cliente ➔ ESP:
- `CLICK`: Registra clique manual (computa bônus de clique + sinergia de MPS).
- `BUY:<upgradeId>:<qty|max>`: Solicita compra de lote da oficina.
- `PERM_BUY:<permId>:<cost>`: Compra e ativa melhoria permanente na árvore.
- `RESET`: Reinicia completamente o progresso do jogo.

### WebSocket (Porta 81) — ESP ➔ Cliente (JSON Broadcast):
```json
{
  "makitas": 1500.0,
  "mps": 14.5,
  "clickPower": 2.0,
  "owned": {
    "upgrade1": 12
  },
  "perms": {
    "perm_lubrificante": true,
    "perm_disco_diamante": true,
    "perm_motor_brushless": false,
    "perm_empunhadura": false,
    "perm_bateria_lítio": false,
    "perm_ia_maker": false
  }
}
```

### Serial UART0 (115200 baud) — ESP ➔ Arduino Mega:
- Formato: `MAKITA:<saldo_inteiro>,<mps_float>\n`
- Exemplo: `MAKITA:1500,14.5`

### Serial UART0 (115200 baud) — Arduino Mega ➔ ESP:
- Formato: `CLICK\n` (Acionado pelo botão no pino 7 com debounce de 40ms).

---

## 6. 🔄 Ciclo de Build, CI/CD e OTA Automático Unificado

- **Cloudflare Pages:** Ao receber um `git push`, o `build.sh` realiza `git fetch --unshallow`, compila tanto o firmware do **ESP8266** quanto o do **Arduino Mega** com `arduino-cli`, gera a imagem do `LittleFS` com `mklittlefs` e versiona automaticamente através do total de commits (`git rev-list --count HEAD`).
- **ESP8266 Boot:** Conecta ao Wi-Fi, consulta `version.json` com header `no-cache` e:
  1. Atualiza o **Arduino Mega** via protocolo STK500v2 (Pinos 0 e 1).
  2. Atualiza a interface web do **LittleFS** (sem reboot).
  3. Atualiza o firmware do **ESP8266** (com reboot automático).
