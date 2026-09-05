# Guia de Arquitetura e Desenvolvimento para IAs (MakitaClicker)

Este documento descreve as regras, arquitetura e convenções do projeto **MakitaClicker**. Qualquer assistente de IA ou agente que modificar o código deve ler estas diretrizes antes de realizar alterações.

## 🏗️ 1. Arquitetura Geral

O projeto é um jogo incremental (Cookie Clicker) **híbrido** (Físico + Web).
- **Web (`web/`):** Frontend Vanilla JS + CSS, empacotado via **Vite**.
- **Backend (`functions/api/`):** API Serverless na Cloudflare Functions com Cloudflare KV (Banco de dados de Chave-Valor).
- **Firmware (`firmware/codigo_esp/`):** C++ rodando em um ESP8266 NodeMCU.
- **Build (`dist/`):** O comando `npm run build` cria a build Web via Vite e em seguida executa o script `build-firmware.sh` para compilar o `.bin` da ESP8266 (usando `arduino-cli`). Tudo é exportado para `dist/` e servido no Cloudflare Pages.

## 🧠 2. Padrões Técnicos e Regras de Negócio

### Frontend (Vanilla JS)
- **Não adicione frameworks** (como React ou Vue). O frontend é 100% Vanilla JS focado em performance.
- **Motor Gráfico:** O arquivo `game.js` roda o ciclo principal (cálculo passivo de saldo e redraws rápidos) a **60 FPS** usando `requestAnimationFrame`. 
- **DOM Throttling:** Atualizações no DOM que não exigem 60 FPS (como atualizar listas de oficinas, textos descritivos) devem ser feitas via throttling (~6 FPS) para manter o uso de CPU mínimo.

### Backend (Reconciliação e CRDT)
- **Sincronismo Assíncrono:** O Frontend e a ESP enviam dados via POST para `/api/state`. Como há duas fontes de verdade gerando cliques ao mesmo tempo, usamos **Monotonic Ratchet (CRDT)**.
- **Regra de Ouro:** O saldo nunca retrocede por atualizações, ele sempre soma os deltas pendentes. O nível de uma melhoria é sempre o `Math.max` entre o servidor e o cliente. Se for retirar saldo (compra de item), essa operação deve ser atômica e autorizada.
- O Cloudflare KV tem limite de gravações gratuitas. O backend faz cache em memória no Worker e salva o estado apenas em *checkpoints* (a cada 60s) ou imediatamente se houver uma compra.

### Firmware (C++ ESP8266)
- **Sem bloqueios:** É proibido usar `delay()` no loop principal. Toda temporização deve ser não-bloqueante usando `millis()` ou `yield()`.
- **Display LCD (I2C):** O LCD (20x4) usa "double-buffering". Só envie comandos via I2C (`printLinhaFormatada`) para caracteres/linhas que de fato mudaram. Isso evita cintilação (flicker).
- **LittleFS:** O estado é persistido em `/gamestate.json`. Nunca bloqueie o loop principal com gravações longas desnecessárias. Apenas a cada 15 segundos.

## 🛠️ 3. Como Adicionar Funcionalidades (Playbook)

### Adicionar uma Nova Oficina (Loja)
As configurações devem estar espelhadas em três lugares. Não esqueça de nenhum!
1. **Frontend (`web/game.js`):** Adicione no array `upgrades` (nome, custo base, mps base).
2. **Backend (`functions/api/state.js`):** Adicione no array `UPGRADES` (cost, mps, nome) para garantir que o servidor valide compras.
3. **Firmware (`firmware/codigo_esp/codigo_esp.ino`):** Adicione na struct `UPGRADE_CONFIGS` para que a ESP calcule corretamente a produção passiva offline/local.

### Adicionar uma Habilidade (Skill Tree Permanentes)
1. Defina o nó no array `PERMANENT_UPGRADES` em `functions/api/state.js` e em `web/game.js`. Você precisará de: `id`, `name`, `req` (meta acumulada exigida) e `parent` (ID do pré-requisito).
2. Se a habilidade altera os multiplicadores físicos do hardware, vá até `firmware/codigo_esp/codigo_esp.ino` na função `recalculateStats()` e leia o ID correspondente da string que chega do JSON na sincronização HTTPS.

### Modificar o Handshake de Reset
O reset é bidirecional para evitar ressurreição de dados offline antigos.
- Se alterar a lógica, mantenha o ciclo: 
  `Web envia reset` -> `KV liga resetOrder=true` -> `ESP recebe ordem` -> `ESP apaga Flash` -> `ESP envia resetAck=true` -> `KV desliga resetOrder`.

## 🚀 4. Desenvolvimento Local
- **Frontend:** Rode `npm run dev`. O frontend vai detectar o modo local e cortar as requisições HTTPS para simular o jogo offline perfeitamente.
- **ESP8266:** Só precisa compilar a primeira vez via USB. A placa auto-atualiza o `.bin` via rede a cada push na branch `main`.
