# 🎮 Arquitetura e Organização do Jogo — MakitaClicker

> Documento de referência rápida para o funcionamento, regras de negócio, mecânicas de gameplay, árvore de habilidades e comunicação hardware/software.

---

## 1. ⚙️ Visão Geral do Sistema

O **MakitaClicker** é um jogo híbrido físico/digital. O hardware do **ESP8266** atua como o servidor autoritativo da partida (saldo de Makitas, produção passiva, compras de upgrades e multiplicadores), enquanto a **Interface Web** e o **Arduino Mega** são clientes de interação.

```
┌─────────────────┐      Serial 9600 baud      ┌─────────────────────────┐
│  Arduino Mega   │ ◄────────────────────────► │         ESP8266         │
│ (Botão 7 + LCD) │   CLICK / MAKITA:X,MPS     │ (Servidor Autoritativo) │
└─────────────────┘                            └────────────┬────────────┘
                                                            │ WebSocket :81
                                                            │ (HTTP :80 LittleFS)
                                                            ▼
                                               ┌─────────────────────────┐
                                               │   Interface Web (JS)    │
                                               │   - Clicker & Efeitos   │
                                               │   - Loja de Oficinas    │
                                               │   - Árvore Habilidades  │
                                               │   - Estatísticas        │
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
- **Teto Máximo:** 100 unidades por tipo de oficina (`MAX_OWNED = 100`).
- **Modos de Compra:** `1x`, `10x`, `MAX` (calcula o lote máximo acessível com o saldo atual sem ultrapassar 100).

---

## 4. 📡 Protocolo de Comunicação em Tempo Real

### WebSocket (Porta 81) — Cliente ➔ ESP:
- `CLICK`: Registra clique manual (computa bônus de clique + sinergia de MPS).
- `BUY:<upgradeId>:<qty|max>`: Solicita compra de lote da oficina.
- `PERM_BUY:<permId>:<cost>`: Compra e ativa melhoria permanente na árvore.

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

### Serial UART (9600 baud) — ESP ➔ Arduino Mega:
- Formato: `MAKITA:<saldo_inteiro>,<mps_float>\n`
- Exemplo: `MAKITA:1500,14.5`

### Serial UART (9600 baud) — Arduino Mega ➔ ESP:
- Formato: `CLICK\n` (Acionado pelo botão no pino 7 com debounce de 40ms).

---

## 5. 🔄 Ciclo de Build, CI/CD e OTA Automático

- **Cloudflare Pages:** Ao receber um `git push`, o `build.sh` compila o firmware com `arduino-cli`, empacota o `LittleFS` com `mklittlefs` e versiona automaticamente através do contador de commits (`git rev-list --count HEAD`).
- **ESP8266 Boot:** Conecta ao Wi-Fi, consulta `version.json` e atualiza firmware/filesystem sem necessidade de cabos.
