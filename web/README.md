# 🌐 Web — MakitaClicker

Documentação técnica da interface web e da API serverless do **MakitaClicker**, hospedadas na infraestrutura global da **Cloudflare Pages**.

- **URL do Jogo:** [https://makitaclicker.pages.dev](https://makitaclicker.pages.dev)
- **Endpoint da API:** [https://makitaclicker.pages.dev/api/state](https://makitaclicker.pages.dev/api/state)
- **Manifesto de Versão:** [https://makitaclicker.pages.dev/version.json](https://makitaclicker.pages.dev/version.json)

---

## 📂 Estrutura de Arquivos

```
web/
├── index.html       # Marcação semântica, layout em 3 colunas e abas de navegação
├── style.css        # Sistema de design centralizado, variáveis CSS e responsividade
├── game.js          # Motor a 60 FPS, árvore de habilidades, reconciliação e telemetria
├── images/          # Sprites e ícones das ferramentas e melhorias
└── makitaCoracao.png# Logo oficial
```

---

## ⚙️ Arquitetura do Frontend

### 1. Separação Estrita de Responsabilidades
- **`index.html`**: Isento de lógica ou estilos inline. Organizado em três colunas industriais:
  - **Coluna Esquerda:** Big Button de clique (Makita giratória), contador de saldo a 60 FPS e taxa de Makitas Por Segundo (MPS).
  - **Coluna Central:** Abas de navegação (**🌳 Melhorias Permanentes**, **📊 Estatísticas**, **📡 Status & Hardware**).
  - **Coluna Direita:** Loja de Oficinas com seletores de quantidade (`1`, `10`, `MAX`).
- **`style.css`**: Design system temático industrial escuro baseado nas cores da Makita (azul-petróleo `#008080`, laranja `#ff8f43` e vermelho `#ff3d00`). Animações aceleradas por GPU (`@keyframes pulseGreen`, `@keyframes spinBlade`).
- **`game.js`**: Motor autônomo desacoplado em camadas:
  1. *Camada de Simulação Contínua:* `gameLoop` a 60 FPS com delta de tempo.
  2. *Camada de Renderização Throttled:* Executa a ~6 FPS para botões de compra, mantendo uso de CPU desprezível.
  3. *Camada de Persistência Local:* `localStorage` atualizado a cada evento e salvo com `sendBeacon` ao fechar o navegador.
  4. *Camada de Rede e Telemetria:* Sincronização em segundo plano via `fetch('/api/state')` a cada 5 segundos.

---

## 📡 Aba de Status & Telemetria do Hardware

Implementada para monitorar a saúde e o status do console físico em tempo real:

| Componente na Tela | Fonte dos Dados | Descrição / Comportamento |
|---|---|---|
| **Hero Card (Status Geral)** | `latestServerData.espTelemetry.lastPing` | Indicador com LED pulsante: Verde (< 90s), Laranja (90s–5min), Cinza (> 5min / offline). |
| **Versão Remota** | `/version.json` | Versão de firmware compilada mais recente disponível na Cloudflare. |
| **Versão na ESP8266** | `espTelemetry.fwVersion` | Versão atualmente em execução no chip físico. Exibe badge `✅ Atualizado` ou `⚠️ OTA Pendente`. |
| **Latência HTTP (Ping)** | `performance.now()` | Medição em milissegundos da ida e volta da requisição `/api/state`. Classifica a conexão (< 120ms excelente, < 350ms normal). |
| **Sinal Wi-Fi (RSSI)** | `espTelemetry.rssi` | Intensidade em dBm do sinal da antena da ESP, com classificação visual. |
| **IP Local da ESP** | `espTelemetry.ip` | Endereço IP atribuído à NodeMCU no roteador do MakerSpace. |
| **Uptime da ESP** | `espTelemetry.uptime` | Tempo contínuo de atividade desde o último boot (`Xh Ym Zs`). |
| **RAM Livre (Heap)** | `espTelemetry.freeHeap` | Memória RAM livre em KB, útil para checar vazamentos de memória. |
| **Cloudflare KV** | `_kv_connected` e `_kv_binding` | Diagnóstico de conexão do banco de dados na Cloudflare Pages. |
| **Ordem de Reset** | `resetOrder` | Status do handshake de reset bidirecional. |
| **Botão "🔄 Atualizar Agora"** | `btnTestPing` | Força sincronização imediata e medição de ping sob demanda. |

---

## ☁️ API Serverless (`functions/api/state.js`)

A API roda em Workers da Cloudflare no modelo Edge Computing (baixa latência mundial).

### Endpoints
- **`GET /api/state`**: Retorna o estado autoritativo completo e processa a produção passiva decorrida.
- **`POST /api/state`**: Executa ações de jogo com garantia de convergência.

### Payload de Ações (`POST`):
```json
{
  "action": "sync" | "buy" | "perm_buy" | "reset",
  "source": "esp" | "web",
  "clicks": 5,
  "makitas": 12500.5,
  "upgradeId": "upgrade1",
  "qty": "max",
  "permId": "perm_lubrificante",
  "resetAck": true,
  "fwVersion": 63,
  "ip": "192.168.1.150",
  "rssi": -55,
  "uptime": 3600,
  "freeHeap": 41500
}
```

### Regras de Reconciliação Monotônica (CRDT Ratchet)
1. **Saldos:** O saldo da partida nunca retrocede (`state.makitas = Math.max(state.makitas, clientMakitas)`).
2. **Oficinas:** Cada oficina adota sempre o maior nível registrado entre todos os clientes (`Math.max(ownedLocal, ownedRemoto)`).
3. **Tecnologias:** Se qualquer nó desbloqueou uma habilidade na árvore, ela permanece desbloqueada para sempre.
4. **Cliques Pendentes:** Cliques enviados em lote são somados ao saldo usando o poder de clique autoritativo do servidor.

### Proteção de Cota de Gravação no Cloudflare KV
O plano gratuito do Cloudflare KV permite até 1.000 gravações (*writes*) por dia. Para não estourar a cota:
- Grava **imediatamente** em: compras de oficinas, compras de habilidades, ordens de reset, confirmações de reset da ESP ou cliques ativos.
- Em sincronizações ociosas (sem compras ou cliques), os dados ficam em cache de memória do Worker e só são gravados no KV em intervalos de **60 segundos** (checkpoint periódico).

---

## 🛠️ Como Testar Localmente

1. Rode o servidor de desenvolvimento do Vite na raiz do repositório:
   ```bash
   npm install
   npm run dev
   ```
2. O motor entrará automaticamente em **Modo Simulador Offline**, funcionando plenamente a 60 FPS sem requisições de rede.
3. Para validar a build web para produção e compilar o projeto:
   ```bash
   npm run build:web
   ```
