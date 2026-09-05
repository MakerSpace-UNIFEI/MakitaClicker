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
  - **Coluna Esquerda:** Big Button de clique (Makita giratória), contador de saldo fluido e taxa de Makitas Por Segundo (MPS).
  - **Coluna Central:** Barra de perfil de usuário (`.profile-bar`), abas de navegação (**🌳 Melhorias Permanentes**, **📊 Estatísticas**, **📡 Status & Hardware**).
  - **Coluna Direita:** Loja de Oficinas com seletores de quantidade (`1`, `10`, `MAX`).
  - **Modal de Perfis (`#profileModal`):** Interface de abertura/troca com lista de perfis salvos no KV e formulário para criar novos jogadores instantaneamente.
- **`style.css`**: Design system temático industrial escuro baseado nas cores da Makita (azul-petróleo `#008080`, laranja `#ff8f43` e vermelho `#ff3d00`). Animações aceleradas por GPU (`@keyframes pulseGreen`, `@keyframes spinBlade`), modal de login e badges de status de salvamento.
- **`game.js`**: Motor autônomo desacoplado em camadas:
  1. *Camada de Simulação Contínua:* `gameLoop` fluido com `requestAnimationFrame` na taxa nativa do monitor, calculando produção passiva pelo delta de tempo (`dt`).
  2. *Camada de Renderização Throttled:* Executa a ~6 FPS para botões de compra, mantendo uso de CPU desprezível.
  3. *Camada de Perfis e Persistência Local:* Armazena o save isolado em `localStorage` sob `makita_clicker_state_<userId>` e perfil ativo em `makita_clicker_profile_id`.
  4. *Camada de Sincronização Cloud:* Auto-save no Cloudflare KV a cada 3 minutos, salvamento manual com feedback visual e guarda `beforeunload` para progresso não salvo há mais de 5 minutos.
  5. *Camada de Telemetria:* Monitoramento em tempo real do microcontrolador físico e latência da Cloudflare.

---

## 👤 Sistema de Perfis de Usuário
- **Sem Senha:** Pensado para usabilidade instantânea no laboratório e na web; basta digitar um apelido ou escolher um perfil existente.
- **Isolamento de Progresso:** Saldo, oficinas adquiridas e nós da árvore tecnológica são exclusivos de cada jogador.
- **Leaderboard Global / Top Player:** O jogador com maior saldo é calculado no backend e transmitido tanto para o display LCD da ESP8266 física quanto para a aba de estatísticas.

---

## 📊 Aba de Estatísticas Detalhadas
A aba **Estatísticas** organiza em tempo real métricas de sessão, persistência e produção:
1. **Perfil & Sincronização na Nuvem:**
   - **Perfil Ativo:** Apelido do jogador em sessão.
   - **Status na Nuvem:** Estado visual de sincronização (`🟢 Salvo na Nuvem` ou `🟡 Alterações pendentes`).
   - **Último Save na Nuvem:** Data e horário da última persistência confirmada no Cloudflare KV.
   - **Criação do Perfil:** Data/hora de quando o jogador foi cadastrado.
   - **Tempo Nesta Sessão:** Cronômetro contínuo de tempo jogado na aba ativa.
   - **Líder Global (Top Player):** Jogador líder do ranking exibido também no LCD da ESP8266.
2. **Economia & Produção:**
   - Makitas atuais e Total histórico produzido.
   - Progresso percentual da Meta Lendária 99B.
   - Produção passiva atual (MPS) e Poder efetivo por clique.
   - Contador de cliques manuais efetuados.
   - Total somado de unidades de oficinas construídas.
   - Total de melhorias adquiridas na árvore tecnológica (X/20).

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
- **`GET /api/state?action=list_users`**: Retorna a lista de todos os perfis cadastrados (`id`, `name`, `createdAt`, `lastSeen`, `makitas`).
- **`GET /api/state?userId=<id>`**: Carrega o estado individual do jogador especificado + dados do `topPlayer`.
- **`GET /api/state`**: Retorna o estado global (utilizado pelo hardware da ESP8266) + dados do `topPlayer`.
- **`POST /api/state`**: Executa ações autoritativas e salvamentos no Cloudflare KV.

### Formato Compactado do Estado (KV):
Para manter o tráfego minúsculo e respeitar a cota diária gratuita do Cloudflare KV:
```json
{
  "makitas": 1250000.5,
  "totalAccumulated": 3500000.0,
  "upgrades": [15, 10, 5, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
  "perms": [0, 1, 2, 4]
}
```

### Ações Suportadas (`POST`):
- `action: "create_user"`: Cria instantaneamente um perfil no KV (`users:list` e `user:<id>:state`).
- `action: "save_user_state"`: Salva o progresso compactado do jogador no KV.
- `action: "sync"`: Utilizado pela ESP8266 para enviar telemetria, receber o estado global e dados do `topPlayer`.
- `action: "reset"`: Dispara handshake de limpeza global.

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
