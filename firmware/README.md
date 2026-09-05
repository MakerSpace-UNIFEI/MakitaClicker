# 🔌 Firmware — MakitaClicker

Manual completo do firmware embarcado do **MakitaClicker** para o microcontrolador **ESP8266 NodeMCU (ESP-12E / ESP-12F)**.

O firmware é 100% autônomo: controla o display LCD 20×4 I2C, gerencia o botão físico com resposta instantânea (0ms), persiste o progresso na memória flash via **LittleFS**, reporta telemetria em tempo real e se auto-atualiza pela nuvem via **OTA (Over-The-Air)**.

---

## 📂 Estrutura de Arquivos

```
firmware/
├── codigo_esp/
│   └── codigo_esp.ino    # Código C++ Arduino unificado para NodeMCU
│
├── projeto/              # Projeto de hardware no KiCad
│   ├── projeto.kicad_pcb # Layout da placa de circuito impresso
│   ├── projeto.kicad_sch # Esquemático eletrônico
│   └── projeto.kicad_pro # Projeto KiCad
│
└── GAME_DESIGN.md        # Tabela completa de balanceamento, custos e fórmulas
```

---

## ⚡ Conexões Elétricas e Pinagem (ESP8266 NodeMCU)

| Dispositivo | Pino no Componente | Pino na NodeMCU | Função / Tipo | Observações |
|---|---|---|---|---|
| **LCD 20×4 I2C** | **GND** | **GND** | Alimentação Negativa | Terra comum do circuito |
| **LCD 20×4 I2C** | **VCC** | **VV (ou VU)** | Alimentação Positiva 5V | **Obrigatório 5V** vindo da USB para contraste nítido do LCD |
| **LCD 20×4 I2C** | **SDA** | **D2 (GPIO 4)** | Barramento I2C Dados | Suporta auto-detecção de endereço (`0x27` ou `0x3F`) |
| **LCD 20×4 I2C** | **SCL** | **D1 (GPIO 5)** | Barramento I2C Clock | Frequência padrão de 100 kHz |
| **Botão Físico** | **Pino 1** | **D5 (GPIO 14)** | Entrada Digital | Modo `INPUT_PULLUP` interno (resistor pull-up ativo) |
| **Botão Físico** | **Pino 2** | **GND** | Referência | O botão fecha no GND ao ser pressionado |

> ⚠️ **Importante sobre a Tensão no LCD (Pino VV):**  
> O display LCD 2004 com módulo PCF8574 necessita de **5V** para polarizar corretamente os cristais líquidos. Alimentar o VCC do LCD no pino 3V3 resultará em tela sem texto legível (contraste insuficiente). O pino **VV** (ou **VU** em alguns clones da NodeMCU) fornece diretamente os 5V provenientes do conector micro-USB.

---

## 📺 Funcionamento do Display LCD 20×4

O display possui 4 linhas de 20 caracteres e utiliza double-buffering inteligente (apenas linhas com alterações reais são enviadas ao barramento I2C, eliminando qualquer cintilação):

```text
Linha 0: [⚙] MAKITA CLICKER [⚙]
Linha 1: [🪙] Saldo: 12.3k MKT
Linha 2:  Site: makitaclicker
Linha 3:        .pages.dev
```

### 1. Linha 0 (Cabeçalho & Animação)
- Exibe o título do jogo ladeado por dois ícones customizados gravados na memória CGRAM da controladora HD44780.
- Durante o jogo, o ícone alterna frames simulando a rotação de um disco de corte de serra Makita.
- Ao clicar no botão físico, o ícone muda temporariamente para faíscas de corte (`iconSpark`).

### 2. Linha 1 (Saldo)
- Exibe o ícone de moeda (`iconCoin`) ou troféu (`iconTrophy` quando atinge a Meta 99B).
- Formata o saldo em notação compacta inteligente (`k`, `M`, `B`, `T`, `Qa`).

### 3. Linhas 2 e 3 (Exibição sem Cortes do Site)
- O endereço do jogo na Cloudflare é `makitaclicker.pages.dev` (23 caracteres).
- Para nunca cortar o domínio no limite de 20 colunas, o firmware utiliza as **Linhas 2 e 3 simultaneamente**:
  - Linha 2: ` Site: makitaclicker` (13 caracteres do subdomínio)
  - Linha 3: `       .pages.dev   ` (10 caracteres do domínio)

### 4. Linhas 2 e 3 (Modos Normais de Jogo)
Quando não está exibindo o site, a tela opera com:
- **Linha 2:** Poder de corte por clique e produção passiva por segundo (`⚡+1.0 | 🏭 5.0/s`).
- **Linha 3:** Rotação de informações a cada 3,2 segundos entre 7 telas:
  1. Endereço do Site (usando linhas 2 e 3)
  2. **Top Player:** Jogador líder global do site (`Top: Nome (Saldo)`)
  3. Progresso da Meta (`Meta 99B: XX.XX%`)
  4. Total de Oficinas (`Oficinas: XX un.`)
  5. Versão do Firmware (`FW: vXX (OTA Ativo)`)
  6. IP Local na Rede Wi-Fi (`IP: 192.168.x.x`)
  7. Créditos (` MakerSpace UNIFEI  `)
- **Feedback de Clique:** Se o jogador pressiona o botão, a Linha 3 exibe instantaneamente `>> CORTE EFETUADO! <<` por 600ms.

---

## 💾 Persistência Flash (LittleFS)

- O save local é gravado no arquivo `/gamestate.json` usando o sistema de arquivos **LittleFS**.
- **Autosave Periódico:** Ocorre a cada 15 segundos em segundo plano.
- **Autosave de Emergência:** Disparado imediatamente antes de qualquer reboot por OTA.
- Garante retenção de saldo e oficinas mesmo se o dispositivo for desligado da tomada.

---

## 📡 Telemetria e Sincronização HTTPS

A cada 5 segundos (`syncWithCloud`), a ESP8266 envia um pacote JSON via HTTPS para `/api/state`:

```json
{
  "action": "sync",
  "source": "esp",
  "clicks": 3,
  "makitas": 15400.0,
  "fwVersion": 63,
  "ip": "192.168.1.150",
  "rssi": -58,
  "uptime": 12450,
  "freeHeap": 41200
}
```

A nuvem responde com o estado mestre atualizado, quaisquer ordens pendentes e os dados do **Top Player** da web:
```json
{
  "makitas": 25000.0,
  "topPlayer": {
    "name": "Carlos Maker",
    "makitas": 450000.0
  }
}
```
A ESP8266 armazena esses dados em RAM e os exibe periodicamente na Linha 3 do LCD 20×4!

---

## 🔄 Protocolo de Reset Remoto (Handshake)

Se o usuário acionar o reset no site:
1. A nuvem envia `resetOrder: true` para a ESP.
2. A ESP8266 zera suas variáveis em RAM, exclui o arquivo `/gamestate.json` da flash e exibe no LCD a confirmação de limpeza.
3. No ciclo seguinte, a ESP envia `resetAck: true`.
4. A nuvem só desliga a ordem de reset quando recebe o `resetAck`, eliminando o risco de dados antigos ressurgirem.

---

## 🚀 Auto-Update de Firmware via Nuvem (OTA)

1. A ESP8266 verifica o arquivo `https://makitaclicker.pages.dev/version.json` no boot e a cada 5 minutos.
2. Se `firmware_version` for maior que `CURRENT_FIRMWARE_VER`:
   - O LCD é limpo e exibe a tela de atualização:
     ```text
     ====================
        ATUALIZANDO...   
      v62 -> v63         
     >> BAIXANDO OTA...<<
     ```
   - O firmware é baixado via streaming seguro e gravado na flash do chip.
   - A ESP8266 reinicia automaticamente rodando o novo código!

---

## 🛠️ Como Gravar o Firmware Inicialmente (USB)

Apenas o **primeiro flash** precisa ser feito via cabo USB. Depois disso, todos os futuros updates ocorrem pelo ar via OTA.

1. Instale a [Arduino IDE](https://www.arduino.cc/en/software).
2. Adicione a URL do Core ESP8266 em **Preferências > URLs Adicionais para Gerenciadores de Placas**:
   ```
   https://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
3. Instale a placa **esp8266** no Gerenciador de Placas.
4. No menu **Gerenciador de Bibliotecas**, instale:
   - **ArduinoJson** (versão 6 ou 7)
   - **LiquidCrystal I2C**
5. Conecte a NodeMCU no computador via micro-USB.
6. Selecione a placa **NodeMCU 1.0 (ESP-12E Module)** e a porta COM/tty correspondente.
7. Abra `codigo_esp/codigo_esp.ino`, verifique as credenciais do Wi-Fi (`ssid` e `password`) e clique em **Upload**.
