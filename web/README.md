# 🌐 Web — MakitaClicker

Interface web e API serverless do MakitaClicker, hospedadas no **Cloudflare Pages**.

---

## 📂 Estrutura

```
web/                            # Assets estáticos servidos pela CDN
├── index.html                  # Estrutura HTML limpa e semântica
├── style.css                   # Folha de estilo completa (design system, animações, layout responsivo)
├── game.js                     # Motor de jogo 60 FPS, reconciliação de estado e persistência LocalStorage
├── images/                     # Ícones e sprites
└── makitaCoracao.png           # Logo

functions/                      # Cloudflare Pages Functions (API Serverless)
└── api/
    └── state.js                # GET e POST /api/state (Cloudflare KV com lógica Master/Slave)
```

### Separação Modular da Web
- **`index.html`**: Apenas a árvore DOM semântica, referenciando o stylesheet e o motor de jogo externo. Todas as estilizações inline foram migradas para classes CSS dedicadas.
- **`style.css`**: Folha de estilos centralizada com design system em variáveis CSS (`:root`), responsividade mobile, temas de abas e componentes da Skill Tree e Meta 99B.
- **`game.js`**: Motor gráfico a 60 FPS com renderização desacoplada (throttling em 6 FPS para botões/listas DOM), otimização de consultas O(1) para a árvore de habilidades, persistência contínua via `localStorage` e reconciliação Master/Slave com o Cloudflare KV.

---

## ⚙️ Arquitetura de Sincronização (Cloud Master / Client Slave)

O sistema adota o Cloudflare KV como autoridade central de progresso com resolução inteligente:

1. **Nuvem como Master:** Por padrão, o estado salvo no Cloudflare KV dita o saldo, upgrades e tecnologias para todos os clientes conectados.
2. **Push de Progresso Offline:** Caso o jogador atue offline e seu saldo/progresso local supere o estado da nuvem, a requisição de sincronização faz o push automático do save local (saldo, oficinas e habilidades) para o Cloudflare KV.
3. **Persistência Local (LocalStorage):** Mesmo em caso de perda temporária de conexão ou fechamento da aba, o progresso é salvo no `localStorage` do navegador e enviado via `sendBeacon` no descarregamento.

---

## ⚙️ API — `/api/state`

### `GET /api/state`
Retorna o estado oficial da partida (saldo, MPS, upgrades, perms). Calcula e avança a produção passiva decorrida no servidor e salva no KV.

### `POST /api/state`
Endpoints e ações suportadas:

| `action` | Campos | Descrição |
|---|---|---|
| `sync` / `click` | `clicks`, `makitas`, `owned`, `perms` | Sincroniza cliques ou realiza push de progresso offline |
| `buy` | `upgradeId`, `qty` (`1`, `10`, `max`) | Valida e compra oficinas de produção |
| `perm_buy` | `permId` | Valida pré-requisitos, volume acumulado e adquire tecnologia permanente |
| `reset` | — | Reinicia o progresso na nuvem e emite flag `isReset: true` para os clientes |

---

## 🚀 Deploy

O deploy acontece automaticamente via **Cloudflare Pages** a cada `git push` na branch `main`.

- **URL:** `https://makitaclicker.pages.dev`
- **Build Command:** `bash build.sh`
- **Publish Directory:** `online/`
- **KV Binding:** `MAKITA_KV`
