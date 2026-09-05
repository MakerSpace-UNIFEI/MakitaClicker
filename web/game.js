// =====================================================================
// MAKITA CLICKER - MOTOR DE JOGO (RENDERIZAÇÃO NATIVA) + SYNC COM NUVEM
// Sistema de Perfis de Usuário + Cloudflare KV + LocalStorage
// =====================================================================

let isLocalMode = false;
let pendingClicks = 0;
let serverClickPower = 1.0;

// ---------- PERFIL ATIVO & CONTROLE DE SALVAMENTO ----------
let currentUserId = null;
let currentUserName = null;
let currentUserCreatedAt = null;
let lastCloudSaveTime = 0;
let hasUnsavedChanges = false;
let totalClicks = 0;
const sessionStartTime = Date.now();
let latestTopPlayer = null;

// ---------- estado ----------
let makitas = 0;
let mps = 0;
let buyQty = 1;
const MAX_OWNED = 100;

// Total de makitas acumuladas no histórico
let totalMakitasMade = 0;
let prevMakitas = 0;

// Controle de Renderização Otimizada
let isDirty = true;
let lastThrottledRender = 0;
const THROTTLE_RENDER_MS = 150; // Atualiza estados de botões/árvore com cadência suave

// ---------- 24 UPGRADES DA LOJA (PROGRESSÃO ATÉ 99B+) ----------
const upgrades = [
    { id: 'upgrade1',          name: 'Bancada Básica',            icon: '⚙️', baseCost: 10,           growth: 1.10, mps: 0.1 },
    { id: 'upgrade_1mps',      name: 'Esmerilhadeira Manual',      icon: '🪚', baseCost: 100,          growth: 1.12, mps: 1.0 },
    { id: 'upgrade_2mps',      name: 'Serra Mármore 1400W',       icon: '⚡', baseCost: 250,          growth: 1.12, mps: 2.0 },
    { id: 'upgrade_5mps',      name: 'Torno Mecânico',            icon: '🔧', baseCost: 750,          growth: 1.13, mps: 5.0 },
    { id: 'upgrade_10mps',     name: 'Fresadora CNC',             icon: '🎛️', baseCost: 1800,         growth: 1.13, mps: 10.0 },
    { id: 'upgrade_15mps',     name: 'Robô de Solda Industrial',   icon: '🦾', baseCost: 3500,         growth: 1.14, mps: 15.0 },
    { id: 'upgrade_20mps',     name: 'Cortadora a Laser CO2',     icon: '🔴', baseCost: 6000,         growth: 1.14, mps: 20.0 },
    { id: 'upgrade_25mps',     name: 'Prensa Hidráulica 50T',      icon: '🏗️', baseCost: 10000,        growth: 1.14, mps: 25.0 },
    { id: 'upgrade_30mps',     name: 'Impressora 3D de Metal',    icon: '🖨️', baseCost: 16000,        growth: 1.15, mps: 30.0 },
    { id: 'upgrade_50mps',     name: 'Linha de Montagem IA',      icon: '🤖', baseCost: 35000,        growth: 1.15, mps: 50.0 },
    { id: 'upgrade_100mps',    name: 'Mega Fábrica Makita',       icon: '🏭', baseCost: 100000,       growth: 1.15, mps: 100.0 },
    { id: 'upgrade_200mps',    name: 'Reator de Fusão Maker',     icon: '☢️', baseCost: 300000,       growth: 1.16, mps: 200.0 },
    { id: 'upgrade_500mps',    name: 'Estação Espacial Orbital',   icon: '🛸', baseCost: 1000000,      growth: 1.16, mps: 500.0 },
    { id: 'upgrade_1200mps',   name: 'Acelerador de Partículas',  icon: '🌀', baseCost: 3500000,      growth: 1.16, mps: 1200.0 },
    { id: 'upgrade_3000mps',   name: 'Mineração de Asteroides',   icon: '☄️', baseCost: 12000000,     growth: 1.16, mps: 3000.0 },
    { id: 'upgrade_8000mps',   name: 'Usina Vulcânica Maker',     icon: '🌋', baseCost: 40000000,     growth: 1.17, mps: 8000.0 },
    { id: 'upgrade_20kmps',    name: 'Forja de Antimatéria',       icon: '⚛️', baseCost: 150000000,    growth: 1.17, mps: 20000.0 },
    { id: 'upgrade_60kmps',    name: 'Computador Quântico UNIFEI',icon: '💻', baseCost: 500000000,    growth: 1.17, mps: 60000.0 },
    { id: 'upgrade_180kmps',   name: 'Esfera de Dyson Makita',    icon: '☀️', baseCost: 1800000000,   growth: 1.17, mps: 180000.0 },
    { id: 'upgrade_500kmps',   name: 'Portal Dimensional Maker',  icon: '🌌', baseCost: 6000000000,   growth: 1.18, mps: 500000.0 },
    { id: 'upgrade_1500kmps',  name: 'Manipulador Gravitacional', icon: '🪐', baseCost: 20000000000,  growth: 1.18, mps: 1500000.0 },
    { id: 'upgrade_5000kmps',  name: 'Motor de Dobra Espacial',   icon: '🚀', baseCost: 60000000000,  growth: 1.18, mps: 5000000.0 },
    { id: 'upgrade_15000kmps', name: 'Fábrica de Realidade Paralela',icon: '🔮', baseCost: 200000000000,growth: 1.19, mps: 15000000.0 },
    { id: 'upgrade_50000kmps', name: 'Big Bang Maker Contínuo',   icon: '💥', baseCost: 800000000000, growth: 1.19, mps: 50000000.0 }
];

const owned = {};
upgrades.forEach(u => { owned[u.id] = 0; });

// ---------- 20 HABILIDADES NA ÁRVORE PERMANENTE (ATÉ 99B) ----------
const permanentUpgrades = [
    {
        id: 'perm_lubrificante',
        name: 'Óleo Sintético Premium',
        icon: '🛢️',
        cost: 25,
        reqMakitas: 10,
        reqUpgrade: null,
        desc: 'Reduz o atrito dos motores. Aumenta a velocidade de fabricação em +10%.',
        purchased: false
    },
    {
        id: 'perm_disco_diamante',
        name: 'Disco Diamantado Reforçado',
        icon: '💠',
        cost: 100,
        reqMakitas: 50,
        reqUpgrade: 'perm_lubrificante',
        desc: 'Cortes ultra afiados. Aumenta o poder de clique manual em +1 por clique.',
        purchased: false
    },
    {
        id: 'perm_motor_brushless',
        name: 'Motor Brushless Industrial',
        icon: '⚡',
        cost: 300,
        reqMakitas: 150,
        reqUpgrade: 'perm_lubrificante',
        desc: 'Motores sem escova de alta eficiência. Dobra o ganho base de todas as oficinas.',
        purchased: false
    },
    {
        id: 'perm_empunhadura',
        name: 'Empunhadura Ergonômica Pro',
        icon: '🧤',
        cost: 600,
        reqMakitas: 250,
        reqUpgrade: 'perm_disco_diamante',
        desc: 'Menor fadiga ao operar. Cliques manuais geram 5% do MPS atual instantaneamente.',
        purchased: false
    },
    {
        id: 'perm_bateria_litio',
        name: 'Bateria Makita 40V Max XGT',
        icon: '🔋',
        cost: 1500,
        reqMakitas: 600,
        reqUpgrade: 'perm_motor_brushless',
        desc: 'Alimentação contínua de lítio. +25% de produção permanente em todas as fontes.',
        purchased: false
    },
    {
        id: 'perm_ia_maker',
        name: 'MakerBot Autônomo com IA',
        icon: '🤖',
        cost: 5000,
        reqMakitas: 2000,
        reqUpgrade: 'perm_bateria_litio',
        desc: 'Automação inteligente de fabricação. Aumenta a produção global em +50%.',
        purchased: false
    },
    {
        id: 'perm_refrigeracao',
        name: 'Sistema Criogênico de Nitrogênio',
        icon: '❄️',
        cost: 15000,
        reqMakitas: 6000,
        reqUpgrade: 'perm_motor_brushless',
        desc: 'Resfriamento ultra rápido. +20% na velocidade global de produção.',
        purchased: false
    },
    {
        id: 'perm_titanio',
        name: 'Lâmina de Titânio Forjada a Plasma',
        icon: '🗡️',
        cost: 35000,
        reqMakitas: 12000,
        reqUpgrade: 'perm_disco_diamante',
        desc: 'Dureza atômica. Adiciona +3.0 de poder a cada clique manual.',
        purchased: false
    },
    {
        id: 'perm_overclock',
        name: 'Circuito de Overclock Extremo',
        icon: '⚡',
        cost: 100000,
        reqMakitas: 30000,
        reqUpgrade: 'perm_empunhadura',
        desc: 'Sinergia amplificada: Cliques manuais geram 10% do MPS atual instantaneamente.',
        purchased: false
    },
    {
        id: 'perm_nanobots',
        name: 'Enxame de Nanobots Montadores',
        icon: '🔬',
        cost: 250000,
        reqMakitas: 80000,
        reqUpgrade: 'perm_ia_maker',
        desc: 'Construção a nível molecular. +75% de bônus em todas as oficinas.',
        purchased: false
    },
    {
        id: 'perm_singularidade',
        name: 'Núcleo de Singularidade Maker',
        icon: '🌌',
        cost: 1000000,
        reqMakitas: 300000,
        reqUpgrade: 'perm_nanobots',
        desc: 'Dobra e meia (+150%) o MPS total e triplica o poder de clique base.',
        purchased: false
    },
    {
        id: 'perm_plasma_cutter',
        name: 'Cortador a Plasma Estelar',
        icon: '✨',
        cost: 5000000,
        reqMakitas: 1500000,
        reqUpgrade: 'perm_titanio',
        desc: 'Corte por jato térmico cósmico. +25.0 de poder a cada clique manual.',
        purchased: false
    },
    {
        id: 'perm_fusao_fria',
        name: 'Reator de Fusão Fria Compacta',
        icon: '🧪',
        cost: 20000000,
        reqMakitas: 6000000,
        reqUpgrade: 'perm_singularidade',
        desc: 'Energia infinita limpa. +100% de produção passiva global permanente.',
        purchased: false
    },
    {
        id: 'perm_hiperconducao',
        name: 'Hipercondutores de Grafeno',
        icon: '⚡',
        cost: 80000000,
        reqMakitas: 25000000,
        reqUpgrade: 'perm_fusao_fria',
        desc: 'Zero resistência elétrica. Triplica a eficiência base de todas as oficinas.',
        purchased: false
    },
    {
        id: 'perm_sinergia_quantica',
        name: 'Sinergia Quântica de Impacto',
        icon: '🔮',
        cost: 300000000,
        reqMakitas: 100000000,
        reqUpgrade: 'perm_overclock',
        desc: 'Ressonância subatômica: Cada clique manual gera 20% do MPS atual!',
        purchased: false
    },
    {
        id: 'perm_laser_gama',
        name: 'Emissor Laser de Raios Gama',
        icon: '🌠',
        cost: 1200000000,
        reqMakitas: 400000000,
        reqUpgrade: 'perm_plasma_cutter',
        desc: 'Feixe de altíssima frequência. Adiciona +200.0 de poder de clique base.',
        purchased: false
    },
    {
        id: 'perm_taquions',
        name: 'Reator de Táquions Espacial',
        icon: '⏳',
        cost: 5000000000,
        reqMakitas: 1500000000,
        reqUpgrade: 'perm_hiperconducao',
        desc: 'Dobra a velocidade da linha temporal: +200% de produção global passiva.',
        purchased: false
    },
    {
        id: 'perm_materia_escura',
        name: 'Condensador de Matéria Escura',
        icon: '🌑',
        cost: 20000000000,
        reqMakitas: 6000000000,
        reqUpgrade: 'perm_taquions',
        desc: 'Colheita da substância que move o cosmos: +300% de produção global passiva.',
        purchased: false
    },
    {
        id: 'perm_hiper_clique',
        name: 'Martelo de Fótons Subatômico',
        icon: '🔨',
        cost: 50000000000,
        reqMakitas: 15000000000,
        reqUpgrade: 'perm_laser_gama',
        desc: 'Multiplica todo o poder de clique manual por 10x!',
        purchased: false
    },
    {
        id: 'perm_onipotencia_maker',
        name: 'Onipotência Maker Cósmica',
        icon: '👑',
        cost: 99000000000,
        reqMakitas: 35000000000,
        reqUpgrade: 'perm_materia_escura',
        desc: 'Atinge a perfeição maker: +500% MPS global, quadruplica oficinas e +30% MPS por clique!',
        purchased: false
    }
];

// Dicionário para busca O(1) de melhorias permanentes (elimina .find() repetitivo)
const permById = {};
permanentUpgrades.forEach(p => { permById[p.id] = p; });

// ---------- elementos ----------
const counterEl = document.getElementById('counter');
const rateEl = document.getElementById('rate');
const makitaBtn = document.getElementById('makitaBtn');
const clickArea = document.getElementById('clickArea');
const shopListEl = document.getElementById('shopList');
const shopQtyEl = document.getElementById('shopQty');
const logEl = document.getElementById('log');
const permTreeGridEl = document.getElementById('permTreeGrid');

// Elementos de estatísticas
const statMakitasEl = document.getElementById('statMakitas');
const statTotalMakitasEl = document.getElementById('statTotalMakitas');
const statMpsEl = document.getElementById('statMps');
const statClickPowerEl = document.getElementById('statClickPower');
const statPermCountEl = document.getElementById('statPermCount');
const goalPercentTextEl = document.getElementById('goalPercentText');
const goalProgressBarEl = document.getElementById('goalProgressBar');
const goalStatusMsgEl = document.getElementById('goalStatusMsg');

const statProfileNameEl = document.getElementById('statProfileName');
const statCloudStatusEl = document.getElementById('statCloudStatus');
const statLastSaveEl = document.getElementById('statLastSave');
const statCreatedAtEl = document.getElementById('statCreatedAt');
const statSessionTimeEl = document.getElementById('statSessionTime');
const statTopPlayerLeaderEl = document.getElementById('statTopPlayerLeader');
const statGoalProgressEl = document.getElementById('statGoalProgress');
const statTotalClicksEl = document.getElementById('statTotalClicks');
const statTotalOwnedEl = document.getElementById('statTotalOwned');

// ---------- CONTROLE DE ABAS ----------
const tabBtns = document.querySelectorAll('.center-nav .navbtn');
tabBtns.forEach(btn => {
    btn.addEventListener('click', () => {
        const targetTab = btn.dataset.tab;
        tabBtns.forEach(b => b.classList.toggle('is-active', b === btn));
        document.querySelectorAll('.tab-pane').forEach(pane => {
            pane.classList.toggle('is-active', pane.id === targetTab);
        });
        isDirty = true;
    });
});

// ---------- PERSISTÊNCIA LOCAL (LOCALSTORAGE POR PERFIL) ----------
function getLocalStorageKey() {
    return currentUserId ? `makitaclicker_save_${currentUserId}` : 'makitaclicker_save';
}

function getCompactGameState() {
    const upgradesArr = upgrades.map(u => owned[u.id] || 0);
    const permsArr = [];
    permanentUpgrades.forEach((p, idx) => {
        if (p.purchased) permsArr.push(idx);
    });
    return {
        makitas,
        totalMakitasMade,
        totalClicks,
        upgrades: upgradesArr,
        perms: permsArr,
        lastUpdate: Date.now()
    };
}

function applyCompactState(data) {
    if (!data) return;
    if (typeof data.makitas === 'number') {
        makitas = data.makitas;
    }
    if (typeof data.totalMakitasMade === 'number') {
        totalMakitasMade = data.totalMakitasMade;
    }
    if (typeof data.totalClicks === 'number') {
        totalClicks = data.totalClicks;
    }
    if (Array.isArray(data.upgrades)) {
        upgrades.forEach((u, idx) => {
            owned[u.id] = data.upgrades[idx] || 0;
        });
    } else if (data.owned && typeof data.owned === 'object') {
        upgrades.forEach(u => {
            owned[u.id] = data.owned[u.id] || 0;
        });
    }
    if (Array.isArray(data.perms)) {
        permanentUpgrades.forEach((p, idx) => {
            p.purchased = data.perms.includes(idx);
        });
    } else if (data.perms && typeof data.perms === 'object') {
        permanentUpgrades.forEach(p => {
            p.purchased = data.perms[p.id] === true;
        });
    }
    mps = calculateLocalMps();
    serverClickPower = calculateLocalClickPower();
    isDirty = true;
    renderUI();
}

function saveLocalState() {
    try {
        const saveObj = getCompactGameState();
        saveObj.name = currentUserName;
        saveObj.createdAt = currentUserCreatedAt;
        saveObj.lastCloudSaveTime = lastCloudSaveTime;
        localStorage.setItem(getLocalStorageKey(), JSON.stringify(saveObj));
    } catch (e) {
        // LocalStorage desabilitado ou cheio
    }
}

function loadLocalState() {
    try {
        const raw = localStorage.getItem(getLocalStorageKey());
        if (!raw) return false;
        const data = JSON.parse(raw);
        applyCompactState(data);
        if (data.createdAt) {
            currentUserCreatedAt = data.createdAt;
        }
        if (typeof data.lastCloudSaveTime === 'number' && data.lastCloudSaveTime > 0) {
            lastCloudSaveTime = data.lastCloudSaveTime;
        }
        return true;
    } catch (e) {
        return false;
    }
}

function savePermanentProgress() {
    saveLocalState();
}

function loadLocalPermanentProgress() {
    loadLocalState();
}

// ---------- CÁLCULOS LOCAIS OTIMIZADOS ----------
function calculateLocalMps() {
    let baseMps = 0.0;
    upgrades.forEach(u => {
        baseMps += (owned[u.id] || 0) * u.mps;
    });

    let workshopMult = 1.0;
    if (permById.perm_motor_brushless?.purchased) workshopMult *= 2.0;
    if (permById.perm_hiperconducao?.purchased) workshopMult *= 3.0;
    if (permById.perm_onipotencia_maker?.purchased) workshopMult *= 4.0;
    baseMps *= workshopMult;

    let mult = 1.0;
    if (permById.perm_lubrificante?.purchased) mult += 0.10;
    if (permById.perm_refrigeracao?.purchased) mult += 0.20;
    if (permById.perm_bateria_litio?.purchased) mult += 0.25;
    if (permById.perm_ia_maker?.purchased) mult += 0.50;
    if (permById.perm_nanobots?.purchased) mult += 0.75;
    if (permById.perm_fusao_fria?.purchased) mult += 1.00;
    if (permById.perm_singularidade?.purchased) mult += 1.50;
    if (permById.perm_taquions?.purchased) mult += 2.00;
    if (permById.perm_materia_escura?.purchased) mult += 3.00;
    if (permById.perm_onipotencia_maker?.purchased) mult += 5.00;

    return baseMps * mult;
}

function calculateLocalClickPower() {
    let power = 1.0;
    if (permById.perm_disco_diamante?.purchased) power += 1.0;
    if (permById.perm_titanio?.purchased) power += 3.0;
    if (permById.perm_plasma_cutter?.purchased) power += 25.0;
    if (permById.perm_laser_gama?.purchased) power += 200.0;
    if (permById.perm_singularidade?.purchased) power *= 3.0;
    if (permById.perm_hiper_clique?.purchased) power *= 10.0;
    return power;
}

function unitCost(upgrade, count) {
    return Math.ceil(upgrade.baseCost * Math.pow(upgrade.growth, count));
}

function costForQuantity(upgrade, qty) {
    let total = 0;
    for (let i = 0; i < qty; i++) {
        total += unitCost(upgrade, owned[upgrade.id] + i);
    }
    return total;
}

function formatCompactNumber(n) {
    if (n == null || isNaN(n)) return '0';
    if (n < 1000) return Math.floor(n).toString();
    if (n < 1e6) {
        const k = n / 1e3;
        return k.toLocaleString('pt-BR', { minimumFractionDigits: (k < 10 ? 2 : (k < 100 ? 1 : 0)), maximumFractionDigits: (k < 10 ? 2 : (k < 100 ? 1 : 0)) }) + ' k';
    }
    if (n < 1e9) {
        const m = n / 1e6;
        return m.toLocaleString('pt-BR', { minimumFractionDigits: (m < 10 ? 2 : (m < 100 ? 1 : 0)), maximumFractionDigits: (m < 10 ? 2 : (m < 100 ? 1 : 0)) }) + ' M';
    }
    if (n < 1e12) {
        const b = n / 1e9;
        return b.toLocaleString('pt-BR', { minimumFractionDigits: (b < 10 ? 2 : (b < 100 ? 1 : 0)), maximumFractionDigits: (b < 10 ? 2 : (b < 100 ? 1 : 0)) }) + ' B';
    }
    if (n < 1e15) {
        const t = n / 1e12;
        return t.toLocaleString('pt-BR', { minimumFractionDigits: (t < 10 ? 2 : (t < 100 ? 1 : 0)), maximumFractionDigits: (t < 10 ? 2 : (t < 100 ? 1 : 0)) }) + ' T';
    }
    const qa = n / 1e15;
    return qa.toLocaleString('pt-BR', { minimumFractionDigits: (qa < 10 ? 2 : 1), maximumFractionDigits: (qa < 10 ? 2 : 1) }) + ' Qa';
}

function formatFullNumber(n) {
    if (n == null || isNaN(n)) return '0';
    return Math.floor(n).toLocaleString('pt-BR');
}

function descForQty(upgrade, qty) {
    const gain = upgrade.mps * qty;
    return `+${gain >= 1000 ? formatCompactNumber(gain) : gain.toFixed(1)} makita/s (${qty}x)`;
}

function computeBuy(upgrade) {
    const remaining = MAX_OWNED - owned[upgrade.id];
    if (remaining <= 0) return { qty: 0, cost: 0 };

    if (buyQty === 'max') {
        let qty = 0;
        let cost = 0;
        while (qty < remaining) {
            const next = unitCost(upgrade, owned[upgrade.id] + qty);
            if (cost + next > makitas) break;
            cost += next;
            qty++;
        }
        return { qty, cost };
    }

    const qty = Math.min(buyQty, remaining);
    return { qty, cost: costForQuantity(upgrade, qty) };
}

function setBuyQty(qty) {
    buyQty = qty;
    shopQtyEl.querySelectorAll('button').forEach(btn => {
        btn.classList.toggle('is-active', btn.dataset.qty === String(qty));
    });
    isDirty = true;
}

shopQtyEl.querySelectorAll('button').forEach(btn => {
    const raw = btn.dataset.qty;
    btn.addEventListener('click', () => setBuyQty(raw === 'max' ? 'max' : Number(raw)));
});

// ---------- CONSTRUÇÃO INICIAL DO DOM ----------
function buildShopList() {
    shopListEl.innerHTML = '';
    upgrades.forEach(upgrade => {
        const btn = document.createElement('button');
        btn.className = 'shop-item';
        btn.id = 'buy-' + upgrade.id;
        btn.innerHTML = `
            <span class="shop-item__icon">${upgrade.icon}</span>
            <span class="shop-item__info">
                <span class="shop-item__name">${upgrade.name}</span>
                <span class="shop-item__desc" id="desc-${upgrade.id}"></span>
            </span>
            <span class="shop-item__cost" id="cost-${upgrade.id}"></span>
            <span class="shop-item__owned" id="owned-${upgrade.id}"></span>
        `;
        btn.addEventListener('click', () => buyUpgrade(upgrade));
        shopListEl.appendChild(btn);
    });
}

function buildPermTree() {
    permTreeGridEl.innerHTML = '';
    permanentUpgrades.forEach(perm => {
        const node = document.createElement('div');
        node.className = 'perm-node status-locked';
        node.id = 'perm-node-' + perm.id;
        node.innerHTML = `
            <div class="perm-node__top">
                <div class="perm-node__icon" id="perm-icon-${perm.id}">${perm.icon}</div>
                <div class="perm-node__title-wrap">
                    <span class="perm-node__name" id="perm-name-${perm.id}">${perm.name}</span>
                    <span class="perm-node__badge" id="perm-badge-${perm.id}">Bloqueado</span>
                </div>
            </div>
            <div class="perm-node__desc" id="perm-desc-${perm.id}">${perm.desc}</div>
            <div class="perm-node__reqs" id="perm-reqs-${perm.id}" style="display:none;"></div>
            <button class="perm-node__btn" id="btn-perm-${perm.id}">
                <span id="btn-perm-label-${perm.id}">Comprar</span>
                <span id="btn-perm-cost-${perm.id}">${formatCompactNumber(perm.cost)}</span>
            </button>
        `;
        const btn = node.querySelector(`#btn-perm-${perm.id}`);
        btn.addEventListener('click', () => buyPermanentUpgrade(perm));
        permTreeGridEl.appendChild(node);
    });
}

// ---------- COMPRAS NO CLIENTE + ENVIO À NUVEM ----------
function buyUpgrade(upgrade) {
    const { qty, cost } = computeBuy(upgrade);
    if (qty <= 0 || makitas < cost) return;

    makitas -= cost;
    owned[upgrade.id] = (owned[upgrade.id] || 0) + qty;
    mps = calculateLocalMps();
    isDirty = true;
    hasUnsavedChanges = true;

    saveLocalState();
    updateSaveIndicator();
}

function buyPermanentUpgrade(perm) {
    if (perm.purchased || makitas < perm.cost) return;

    if (perm.reqUpgrade) {
        const req = permById[perm.reqUpgrade];
        if (req && !req.purchased) return;
    }

    perm.purchased = true;
    makitas -= perm.cost;
    mps = calculateLocalMps();
    serverClickPower = calculateLocalClickPower();
    isDirty = true;
    hasUnsavedChanges = true;

    saveLocalState();
    updateSaveIndicator();
}

// ---------- RENDERIZAÇÃO INTELIGENTE E DESACOPLADA ----------
function renderPermTree() {
    const totalAccum = Math.max(totalMakitasMade, makitas);

    permanentUpgrades.forEach(perm => {
        const node = document.getElementById('perm-node-' + perm.id);
        if (!node) return;

        let reqUpgradeMet = true;
        let reqUpgradeName = '';
        if (perm.reqUpgrade) {
            const parent = permById[perm.reqUpgrade];
            if (parent) {
                reqUpgradeName = parent.name;
                reqUpgradeMet = parent.purchased;
            }
        }

        const isRevealed = perm.purchased || reqUpgradeMet || (totalAccum >= perm.reqMakitas);
        const iconEl = document.getElementById('perm-icon-' + perm.id);
        const nameEl = document.getElementById('perm-name-' + perm.id);
        const badgeEl = document.getElementById('perm-badge-' + perm.id);
        const descEl = document.getElementById('perm-desc-' + perm.id);
        const reqsEl = document.getElementById('perm-reqs-' + perm.id);
        const btn = document.getElementById('btn-perm-' + perm.id);
        const btnLabel = document.getElementById('btn-perm-label-' + perm.id);
        const btnCost = document.getElementById('btn-perm-cost-' + perm.id);

        if (!isRevealed) {
            node.className = 'perm-node status-locked';
            if (iconEl) iconEl.textContent = '🔒';
            if (nameEl) nameEl.textContent = 'Tecnologia Oculta';
            if (badgeEl) badgeEl.textContent = 'Bloqueado';
            if (descEl) descEl.textContent = 'Produza mais makitas ou desbloqueie tecnologias anteriores para revelar esta melhoria.';
            if (reqsEl) {
                reqsEl.style.display = 'block';
                reqsEl.textContent = `Requer: ${formatCompactNumber(perm.reqMakitas)} makitas acumuladas`;
            }
            if (btn) btn.style.display = 'none';
            return;
        }

        // Revelado
        if (iconEl) iconEl.textContent = perm.icon;
        if (nameEl) nameEl.textContent = perm.name;
        if (descEl) descEl.textContent = perm.desc;

        let statusClass = 'status-available';
        let badgeText = 'Disponível';
        let isPurchasable = reqUpgradeMet && (makitas >= perm.cost) && !perm.purchased;

        if (perm.purchased) {
            statusClass = 'status-purchased';
            badgeText = 'Adquirido ✓';
        } else if (!reqUpgradeMet) {
            statusClass = 'status-locked';
            badgeText = 'Requisito Pendente';
        }

        node.className = `perm-node ${statusClass}`;
        if (badgeEl) badgeEl.textContent = badgeText;

        if (reqsEl) {
            if (perm.reqUpgrade && !reqUpgradeMet) {
                reqsEl.style.display = 'block';
                reqsEl.textContent = `Pré-requisito: ${reqUpgradeName}`;
            } else {
                reqsEl.style.display = 'none';
            }
        }

        if (btn) {
            btn.style.display = 'flex';
            if (perm.purchased) {
                btn.disabled = true;
                btn.style.background = '#1b5e20';
                if (btnLabel) btnLabel.textContent = 'Ativo';
                if (btnCost) btnCost.textContent = '✓';
            } else {
                btn.disabled = !isPurchasable;
                btn.style.background = '';
                if (btnLabel) btnLabel.textContent = 'Comprar';
                if (btnCost) btnCost.textContent = formatCompactNumber(perm.cost);
                btn.title = `${formatFullNumber(perm.cost)} Makitas`;
            }
        }
    });
}

function renderStats() {
    // 1. Dados do Perfil e Nuvem
    if (statProfileNameEl) {
        statProfileNameEl.textContent = currentUserName || 'Sem Perfil';
    }

    if (statCloudStatusEl) {
        if (!currentUserId) {
            statCloudStatusEl.textContent = '⚪ Sem Perfil';
            statCloudStatusEl.style.color = 'var(--text-lo)';
        } else if (hasUnsavedChanges) {
            const elapsedSec = Math.round((Date.now() - (lastCloudSaveTime || Date.now())) / 1000);
            statCloudStatusEl.textContent = `🟡 Alterações pendentes (${elapsedSec < 60 ? elapsedSec + 's' : Math.round(elapsedSec / 60) + 'm'})`;
            statCloudStatusEl.style.color = 'var(--orange)';
        } else {
            statCloudStatusEl.textContent = '🟢 Salvo na Nuvem';
            statCloudStatusEl.style.color = 'var(--green)';
        }
    }

    if (statLastSaveEl) {
        if (!lastCloudSaveTime || lastCloudSaveTime === 0) {
            statLastSaveEl.textContent = 'Ainda não salvo';
        } else {
            const d = new Date(lastCloudSaveTime);
            statLastSaveEl.textContent = d.toLocaleDateString('pt-BR') + ' ' + d.toLocaleTimeString('pt-BR');
        }
    }

    if (statCreatedAtEl) {
        if (!currentUserCreatedAt) {
            statCreatedAtEl.textContent = '—';
        } else {
            const d = new Date(currentUserCreatedAt);
            statCreatedAtEl.textContent = d.toLocaleDateString('pt-BR') + ' ' + d.toLocaleTimeString('pt-BR');
        }
    }

    if (statSessionTimeEl) {
        const diff = Math.max(0, Math.floor((Date.now() - sessionStartTime) / 1000));
        const hours = Math.floor(diff / 3600);
        const mins = Math.floor((diff % 3600) / 60);
        const secs = diff % 60;
        statSessionTimeEl.textContent = hours > 0 ? `${hours}h ${mins}m ${secs}s` : `${mins}m ${secs}s`;
    }

    if (statTopPlayerLeaderEl) {
        if (latestTopPlayer && latestTopPlayer.name) {
            const topScore = formatCompactNumber(latestTopPlayer.totalMakitasMade || latestTopPlayer.makitas || 0);
            statTopPlayerLeaderEl.textContent = `${latestTopPlayer.name} (${topScore})`;
        } else {
            statTopPlayerLeaderEl.textContent = 'MakerSpace (0)';
        }
    }

    // 2. Economia e Produção
    if (statMakitasEl) {
        statMakitasEl.textContent = formatCompactNumber(makitas);
        statMakitasEl.title = formatFullNumber(makitas) + ' Makitas';
    }
    
    const total = Math.max(totalMakitasMade, makitas);
    if (statTotalMakitasEl) {
        statTotalMakitasEl.textContent = formatCompactNumber(total);
        statTotalMakitasEl.title = formatFullNumber(total) + ' Makitas';
    }
    
    if (statMpsEl) {
        statMpsEl.textContent = mps >= 1000 ? formatCompactNumber(mps) + '/s' : mps.toFixed(1) + '/s';
        statMpsEl.title = mps.toLocaleString('pt-BR') + ' Makitas por segundo';
    }

    let clickPower = calculateLocalClickPower() || 1.0;
    
    let synergyPct = 0;
    if (permById.perm_onipotencia_maker?.purchased) {
        synergyPct = 0.30;
    } else if (permById.perm_sinergia_quantica?.purchased) {
        synergyPct = 0.20;
    } else if (permById.perm_overclock?.purchased) {
        synergyPct = 0.10;
    } else if (permById.perm_empunhadura?.purchased) {
        synergyPct = 0.05;
    }

    if (statClickPowerEl) {
        if (synergyPct > 0) {
            const extra = (mps * synergyPct);
            statClickPowerEl.textContent = `${formatCompactNumber(clickPower)} (+${formatCompactNumber(extra)}/s)`;
        } else {
            statClickPowerEl.textContent = formatCompactNumber(clickPower);
        }
        statClickPowerEl.title = clickPower.toLocaleString('pt-BR') + ' por clique base';
    }

    if (statTotalClicksEl) {
        statTotalClicksEl.textContent = totalClicks.toLocaleString('pt-BR');
    }

    if (statTotalOwnedEl) {
        const totalUnits = Object.values(owned).reduce((sum, val) => sum + (val || 0), 0);
        statTotalOwnedEl.textContent = `${totalUnits} un.`;
    }

    if (statPermCountEl) {
        const purchasedCount = permanentUpgrades.filter(u => u.purchased).length;
        statPermCountEl.textContent = `${purchasedCount}/${permanentUpgrades.length}`;
    }

    // Meta 99B
    const goalTotal = 99000000000;
    const currentProgress = Math.max(makitas, totalMakitasMade);
    const pct = Math.min(100, (currentProgress / goalTotal) * 100);

    if (statGoalProgressEl) {
        statGoalProgressEl.textContent = pct >= 100 ? '100.00% 👑' : (pct < 0.01 && currentProgress > 0 ? '>0.01%' : pct.toFixed(2) + '%');
    }

    if (goalPercentTextEl && goalProgressBarEl) {
        goalPercentTextEl.textContent = pct >= 100 ? '100.00% CONCLUÍDO! 👑' : (pct < 0.01 && currentProgress > 0 ? '>0.01%' : pct.toFixed(2) + '%');
        goalProgressBarEl.style.width = Math.min(100, pct) + '%';
        if (pct >= 100) {
            goalStatusMsgEl.textContent = '🎉 PARABÉNS! Você atingiu a Meta Lendária de 99 Bilhões no MakerSpace UNIFEI!';
            goalStatusMsgEl.style.color = '#a5d6a7';
        } else {
            const remaining = Math.max(0, goalTotal - currentProgress);
            goalStatusMsgEl.textContent = `Faltam ${formatCompactNumber(remaining)} makitas para a Onipotência Maker (99B).`;
            goalStatusMsgEl.style.color = 'var(--text-lo)';
        }
    }
}

// Renderização de elementos DOM desacoplada (evita 100+ manipulações DOM por frame)
function renderUI() {
    upgrades.forEach(upgrade => {
        const capped = owned[upgrade.id] >= MAX_OWNED;
        const { qty, cost } = computeBuy(upgrade);

        const costEl = document.getElementById('cost-' + upgrade.id);
        if (costEl) {
            costEl.textContent = capped ? '—' : formatCompactNumber(cost);
            costEl.title = capped ? 'Máximo atingido' : formatFullNumber(cost) + ' Makitas';
        }
        const ownedEl = document.getElementById('owned-' + upgrade.id);
        if (ownedEl) ownedEl.textContent = owned[upgrade.id];

        const descEl = document.getElementById('desc-' + upgrade.id);
        if (descEl) {
            descEl.textContent = capped ? 'nível máximo atingido (100 un.)' : descForQty(upgrade, qty);
        }

        const buyBtn = document.getElementById('buy-' + upgrade.id);
        if (buyBtn) {
            buyBtn.disabled = capped || qty <= 0 || makitas < cost;
        }
    });

    renderPermTree();
    renderStats();
}

// ---------- EFEITOS VISUAIS E CLIQUES ----------
function showFloatText(text) {
    if (!makitaBtn || !clickArea) return;
    const rect = makitaBtn.getBoundingClientRect();
    const areaRect = clickArea.getBoundingClientRect();
    const el = document.createElement('span');
    el.className = 'float-plus';
    el.textContent = text;
    el.style.left = (rect.left - areaRect.left + rect.width / 2) + 'px';
    el.style.top = (rect.top - areaRect.top) + 'px';
    clickArea.appendChild(el);
    el.addEventListener('animationend', () => el.remove());
}

function showFloatPlus(amount) {
    if (typeof amount !== 'number') {
        showFloatText(String(amount));
        return;
    }
    const text = '+' + (amount >= 1000 ? formatCompactNumber(amount) : (amount % 1 === 0 ? amount : amount.toFixed(1)));
    showFloatText(text);
}

function spawnFlyingMakita() {
    const rect = makitaBtn.getBoundingClientRect();
    const areaRect = clickArea.getBoundingClientRect();

    const img = document.createElement('img');
    img.className = 'flying-makita';
    img.src = '/makitaCoracao.png';
    img.alt = '';

    const size = 1.8 + Math.random() * 1.6;
    img.style.width = size + 'rem';
    img.style.height = size + 'rem';

    const startX = rect.left - areaRect.left + rect.width / 2;
    const startY = rect.top - areaRect.top + rect.height / 2;
    img.style.left = startX + 'px';
    img.style.top = startY + 'px';

    clickArea.appendChild(img);

    const angle = Math.random() * Math.PI * 2;
    const distance = 90 + Math.random() * 140;
    const tx = Math.cos(angle) * distance;
    const ty = Math.sin(angle) * distance - 40;
    const rot = Math.random() * 720 - 360;

    img.offsetWidth;

    img.style.transform = `translate(calc(-50% + ${tx.toFixed(1)}px), calc(-50% + ${ty.toFixed(1)}px)) rotate(${rot.toFixed(0)}deg) scale(.8)`;
    img.classList.add('is-flying');

    let removed = false;
    const remove = () => {
        if (removed) return;
        removed = true;
        img.remove();
    };
    img.addEventListener('transitionend', remove);
    setTimeout(remove, 1200);
}

function playClickFeedback(gain) {
    showFloatPlus(gain);
    spawnFlyingMakita();
}

makitaBtn.addEventListener('click', () => {
    let gain = calculateLocalClickPower();
    if (permById.perm_onipotencia_maker?.purchased) {
        gain += (mps * 0.30);
    } else if (permById.perm_sinergia_quantica?.purchased) {
        gain += (mps * 0.20);
    } else if (permById.perm_overclock?.purchased) {
        gain += (mps * 0.10);
    } else if (permById.perm_empunhadura?.purchased) {
        gain += (mps * 0.05);
    }

    makitas += gain;
    totalMakitasMade += gain;
    totalClicks++;
    pendingClicks++;
    hasUnsavedChanges = true;
    isDirty = true;
    playClickFeedback(gain);
});

// ---------- RESET TOTAL UNIFICADO ----------
function resetAllProgress(sendToServer = true) {
    try {
        localStorage.removeItem(getLocalStorageKey());
        localStorage.removeItem('makitaclicker_save');
        localStorage.removeItem('makita_perm_upgrades');
        localStorage.removeItem('makita_total_produced');
        localStorage.removeItem('makita_local_state');
    } catch (e) {}

    makitas = 0;
    mps = 0;
    totalMakitasMade = 0;
    totalClicks = 0;
    prevMakitas = 0;
    pendingClicks = 0;
    upgrades.forEach(u => { owned[u.id] = 0; });
    permanentUpgrades.forEach(u => { u.purchased = false; });
    serverClickPower = 1.0;
    isDirty = true;
    hasUnsavedChanges = false;
    lastCloudSaveTime = Date.now();

    saveLocalState();

    if (sendToServer && currentUserId) {
        fetch('/api/state', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'reset_user_state', userId: currentUserId })
        }).then(res => res.json()).then(data => {
            if (data.topPlayer) latestTopPlayer = data.topPlayer;
            updateSaveIndicator();
            renderStats();
        }).catch(err => console.warn('Erro ao resetar perfil no servidor:', err));
    } else if (sendToServer) {
        syncWithCloud({ action: 'reset' });
    }

    renderUI();
    renderStats();
}

// ---------- COMUNICAÇÃO HTTP COM CLOUDFLARE PAGES & KV ----------
let isSyncing = false;

function applyServerState(data) {
    if (!data) return;

    if (data.isReset === true) {
        resetAllProgress(false);
        return;
    }

    // Reconciliação Monotônica (Ratchet / CRDT):
    // Se o usuário está logado em um perfil, suas makitas/upgrades pertencem EXCLUSIVAMENTE ao seu perfil!
    // NÃO adotar o saldo global da ESP8266 física!
    if (!currentUserId) {
        if (typeof data.makitas === 'number') {
            const localPendingClicksGain = pendingClicks * calculateLocalClickPower();
            const serverEffective = data.makitas + localPendingClicksGain;
            if (serverEffective > makitas) {
                makitas = serverEffective;
            }
            if (typeof data.totalMakitasMade === 'number') {
                totalMakitasMade = Math.max(totalMakitasMade, data.totalMakitasMade);
            }
        }

        // 2. Upgrades da loja: NUNCA perde upgrades. Mantém sempre o maior valor entre local e servidor.
        if (data.owned && typeof data.owned === 'object') {
            upgrades.forEach(upgrade => {
                if (typeof data.owned[upgrade.id] === 'number') {
                    owned[upgrade.id] = Math.max(owned[upgrade.id] || 0, data.owned[upgrade.id]);
                }
            });
        }

        // 3. Tecnologias permanentes: Se foi desbloqueada no servidor, ativa localmente
        if (data.perms && typeof data.perms === 'object') {
            permanentUpgrades.forEach(u => {
                if (u.id in data.perms && data.perms[u.id] === true) {
                    u.purchased = true;
                }
            });
        }

        // MPS: usa o oficial do servidor se > 0, senão calcula localmente
        const serverMps = typeof data.mps === 'number' ? data.mps : 0;
        mps = serverMps > 0 ? serverMps : calculateLocalMps();
        serverClickPower = calculateLocalClickPower();
    }

    // Feedback de status da ordem de reset para a ESP
    if (data.resetPendingEsp === true) {
        logEl.textContent = '⏳ Ordem de Reset emitida! Aguardando a ESP confirmar a limpeza...';
        logEl.style.color = 'var(--orange)';
    } else if (data.lastResetAckAt && (Date.now() - data.lastResetAckAt < 30000)) {
        logEl.textContent = '✅ A ESP confirmou que limpou sua memória e reiniciou!';
        logEl.style.color = 'var(--green)';
    } else if (data._kv_connected !== undefined) {
        if (data._kv_connected) {
            logEl.textContent = `🟢 Nuvem ativa: Cloudflare KV (${data._kv_binding}) conectado`;
            logEl.style.color = 'var(--text-lo)';
        } else {
            logEl.textContent = data._kv_diag ? `⚠️ ${data._kv_diag}` : '⚠️ KV não vinculado no Cloudflare Pages (Pages > Settings > Functions > KV)';
            logEl.style.color = 'var(--orange)';
        }
    }

    if (data.topPlayer) {
        latestTopPlayer = data.topPlayer;
    }

    latestServerData = data;
    isDirty = true;
    saveLocalState();
    updateStatusUI();
}

// ---------- TELEMETRIA E STATUS DA ESP8266 & CLOUD ----------
let remoteFirmwareVersion = null;
let measuredPingMs = null;
let latestServerData = null;

async function fetchRemoteVersion() {
    try {
        const res = await fetch('/version.json?t=' + Date.now());
        if (res.ok) {
            const data = await res.json();
            if (typeof data.firmware_version === 'number') {
                remoteFirmwareVersion = data.firmware_version;
            }
        }
    } catch (e) {
        console.warn('Erro ao consultar version.json:', e);
    }
    updateStatusUI();
}

function formatUptime(seconds) {
    if (!seconds || seconds <= 0) return '0s';
    const d = Math.floor(seconds / 86400);
    const h = Math.floor((seconds % 86400) / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = Math.floor(seconds % 60);
    const parts = [];
    if (d > 0) parts.push(`${d}d`);
    if (h > 0) parts.push(`${h}h`);
    if (m > 0) parts.push(`${m}m`);
    if (s > 0 || parts.length === 0) parts.push(`${s}s`);
    return parts.join(' ');
}

function updateStatusUI() {
    // 1. Firmware Remoto
    const statRemoteFw = document.getElementById('statRemoteFw');
    if (statRemoteFw) {
        statRemoteFw.textContent = remoteFirmwareVersion !== null ? `v${remoteFirmwareVersion}` : 'Consultando...';
    }

    // Telemetria da ESP recebida pelo backend
    const esp = latestServerData?.espTelemetry;
    const now = Date.now();
    const espLastPing = esp?.lastPing || 0;
    const secondsSinceEsp = espLastPing > 0 ? Math.max(0, Math.round((now - espLastPing) / 1000)) : null;

    // 2. Hero Card & Indicador de Status da ESP8266
    const statusDot = document.getElementById('statusDot');
    const statusEspTitle = document.getElementById('statusEspTitle');
    const statusEspSubtitle = document.getElementById('statusEspSubtitle');

    if (statusDot && statusEspTitle && statusEspSubtitle) {
        if (secondsSinceEsp !== null && secondsSinceEsp < 90) {
            statusDot.style.background = 'var(--green)';
            statusDot.style.boxShadow = '0 0 10px rgba(0, 230, 118, 0.6)';
            statusDot.style.animation = 'pulseGreen 2s infinite';
            statusEspTitle.textContent = 'ESP8266 Conectada & Sincronizada';
            statusEspTitle.style.color = 'var(--green)';
            statusEspSubtitle.textContent = `Último contato há ${secondsSinceEsp}s • IP: ${esp.ip || 'desconhecido'}`;
        } else if (secondsSinceEsp !== null && secondsSinceEsp < 300) {
            statusDot.style.background = 'var(--orange)';
            statusDot.style.boxShadow = '0 0 10px rgba(255, 145, 0, 0.6)';
            statusDot.style.animation = 'pulseOrange 2s infinite';
            statusEspTitle.textContent = 'ESP8266 Sem Sinal Recente';
            statusEspTitle.style.color = 'var(--orange)';
            statusEspSubtitle.textContent = `Último contato há ${Math.round(secondsSinceEsp / 60)} min`;
        } else {
            statusDot.style.background = 'var(--text-lo)';
            statusDot.style.boxShadow = 'none';
            statusDot.style.animation = 'none';
            statusEspTitle.textContent = 'ESP8266 Offline / Aguardando';
            statusEspTitle.style.color = 'var(--text-lo)';
            statusEspSubtitle.textContent = secondsSinceEsp !== null ? `Visto há ${Math.round(secondsSinceEsp / 60)} min` : 'Nenhum sinal recebido';
        }
    }

    // 3. Versão Local na ESP e Comparação com a Nuvem
    const statLocalFw = document.getElementById('statLocalFw');
    const statFwSyncBadge = document.getElementById('statFwSyncBadge');
    if (statLocalFw && statFwSyncBadge) {
        if (esp && typeof esp.fwVersion === 'number' && esp.fwVersion > 0) {
            statLocalFw.textContent = `v${esp.fwVersion}`;
            if (remoteFirmwareVersion !== null) {
                if (esp.fwVersion >= remoteFirmwareVersion) {
                    statFwSyncBadge.textContent = '✅ Atualizado com a nuvem';
                    statFwSyncBadge.style.color = 'var(--green)';
                } else {
                    statFwSyncBadge.textContent = `⚠️ OTA Pendente (Nuvem: v${remoteFirmwareVersion})`;
                    statFwSyncBadge.style.color = 'var(--orange)';
                }
            } else {
                statFwSyncBadge.textContent = 'Versão reportada via telemetria';
                statFwSyncBadge.style.color = 'var(--text-lo)';
            }
        } else {
            statLocalFw.textContent = 'v--';
            statFwSyncBadge.textContent = 'Aguardando telemetria da ESP';
            statFwSyncBadge.style.color = 'var(--text-lo)';
        }
    }

    // 4. Ping / Latência Web ↔ Servidor Cloudflare
    const statPing = document.getElementById('statPing');
    const statPingHint = document.getElementById('statPingHint');
    if (statPing && statPingHint) {
        if (measuredPingMs !== null) {
            statPing.textContent = `${measuredPingMs} ms`;
            if (measuredPingMs < 120) {
                statPing.style.color = 'var(--green)';
                statPingHint.textContent = '⚡ Latência excelente (< 120ms)';
                statPingHint.style.color = 'var(--green)';
            } else if (measuredPingMs < 350) {
                statPing.style.color = 'var(--accent)';
                statPingHint.textContent = '🟢 Latência estável (< 350ms)';
                statPingHint.style.color = 'var(--accent)';
            } else {
                statPing.style.color = 'var(--orange)';
                statPingHint.textContent = '🟡 Latência elevada';
                statPingHint.style.color = 'var(--orange)';
            }
        } else {
            statPing.textContent = '-- ms';
            statPing.style.color = 'var(--text-hi)';
            statPingHint.textContent = 'Medido a cada ciclo de sync';
            statPingHint.style.color = 'var(--text-lo)';
        }
    }

    // 5. Sinal Wi-Fi (ESP RSSI)
    const statRssi = document.getElementById('statRssi');
    const statRssiQuality = document.getElementById('statRssiQuality');
    if (statRssi && statRssiQuality) {
        if (esp && typeof esp.rssi === 'number' && esp.rssi !== 0) {
            statRssi.textContent = `${esp.rssi} dBm`;
            if (esp.rssi >= -60) {
                statRssi.style.color = 'var(--green)';
                statRssiQuality.textContent = '🟢 Excelente (> -60 dBm)';
                statRssiQuality.style.color = 'var(--green)';
            } else if (esp.rssi >= -70) {
                statRssi.style.color = 'var(--accent)';
                statRssiQuality.textContent = '🟡 Bom (-60 a -70 dBm)';
                statRssiQuality.style.color = 'var(--accent)';
            } else if (esp.rssi >= -80) {
                statRssi.style.color = 'var(--orange)';
                statRssiQuality.textContent = '🟠 Regular (-70 a -80 dBm)';
                statRssiQuality.style.color = 'var(--orange)';
            } else {
                statRssi.style.color = 'var(--red)';
                statRssiQuality.textContent = '🔴 Sinal fraco (< -80 dBm)';
                statRssiQuality.style.color = 'var(--red)';
            }
        } else {
            statRssi.textContent = '-- dBm';
            statRssi.style.color = 'var(--text-hi)';
            statRssiQuality.textContent = 'Aguardando telemetria';
            statRssiQuality.style.color = 'var(--text-lo)';
        }
    }

    // 6. IP Local da ESP
    const statEspIp = document.getElementById('statEspIp');
    if (statEspIp) {
        statEspIp.textContent = (esp && esp.ip) ? esp.ip : '--';
    }

    // 7. Uptime & RAM Heap da ESP
    const statEspUptime = document.getElementById('statEspUptime');
    const statEspHeap = document.getElementById('statEspHeap');
    if (statEspUptime) {
        statEspUptime.textContent = (esp && esp.uptime) ? formatUptime(esp.uptime) : '--';
    }
    if (statEspHeap) {
        if (esp && typeof esp.freeHeap === 'number' && esp.freeHeap > 0) {
            statEspHeap.textContent = `RAM Livre: ${(esp.freeHeap / 1024).toFixed(1)} KB`;
        } else {
            statEspHeap.textContent = 'RAM Livre: --';
        }
    }

    // 8. Cloudflare KV Database
    const statKvStatus = document.getElementById('statKvStatus');
    const statKvBinding = document.getElementById('statKvBinding');
    if (statKvStatus && statKvBinding) {
        if (latestServerData) {
            if (latestServerData._kv_connected) {
                statKvStatus.textContent = '🟢 Conectado e Persistente';
                statKvStatus.style.color = 'var(--green)';
                statKvBinding.textContent = `Binding: ${latestServerData._kv_binding || 'MAKITA_KV'}`;
            } else {
                statKvStatus.textContent = '🟡 Memória Volátil (Sem KV)';
                statKvStatus.style.color = 'var(--orange)';
                statKvBinding.textContent = latestServerData._kv_diag ? latestServerData._kv_diag : 'Binding ausente no Cloudflare';
            }
        }
    }

    // 9. Ordem de Reset Remoto
    const statResetOrder = document.getElementById('statResetOrder');
    const statResetAck = document.getElementById('statResetAck');
    if (statResetOrder && statResetAck) {
        if (latestServerData?.resetOrder) {
            statResetOrder.textContent = '⚠️ Ordem Ativa (Pendente)';
            statResetOrder.style.color = 'var(--orange)';
            statResetAck.textContent = 'Aguardando microcontrolador executar reset';
            statResetAck.style.color = 'var(--orange)';
        } else {
            statResetOrder.textContent = '✅ Normal / Sincronizado';
            statResetOrder.style.color = 'var(--green)';
            statResetAck.textContent = 'Nenhuma ordem de reset pendente';
            statResetAck.style.color = 'var(--text-lo)';
        }
    }
}

// ---------- GERENCIAMENTO DE PERFIS DE USUÁRIO & SAVE NA NUVEM ----------
const profileModalEl = document.getElementById('profileModal');
const profileListContainerEl = document.getElementById('profileListContainer');
const newProfileFormEl = document.getElementById('newProfileForm');
const newProfileInputEl = document.getElementById('newProfileInput');
const currentProfileNameEl = document.getElementById('currentProfileName');
const btnSwitchProfileEl = document.getElementById('btnSwitchProfile');
const btnSaveCloudEl = document.getElementById('btnSaveCloud');
const saveStatusTextEl = document.getElementById('saveStatusText');

function updateProfileUI() {
    if (currentProfileNameEl) {
        currentProfileNameEl.textContent = currentUserName || 'Sem Perfil';
    }
    updateSaveIndicator();
}

function updateSaveIndicator() {
    if (!saveStatusTextEl) return;
    if (!currentUserId) {
        saveStatusTextEl.textContent = 'Sem Perfil';
        saveStatusTextEl.style.color = 'var(--text-lo)';
        return;
    }

    const elapsedMs = Date.now() - lastCloudSaveTime;
    const elapsedSec = Math.round(elapsedMs / 1000);
    const elapsedMin = Math.round(elapsedMs / 60000);

    if (elapsedMs > 5 * 60 * 1000 && hasUnsavedChanges) {
        saveStatusTextEl.textContent = `⚠️ Não salvo há ${elapsedMin} min`;
        saveStatusTextEl.style.color = 'var(--orange)';
    } else if (hasUnsavedChanges) {
        saveStatusTextEl.textContent = `🟡 Alterações locais (${elapsedSec < 60 ? elapsedSec + 's' : elapsedMin + 'm'})`;
        saveStatusTextEl.style.color = 'var(--orange)';
    } else {
        saveStatusTextEl.textContent = elapsedSec < 10 ? '🟢 Salvo agora' : `🟢 Salvo há ${elapsedSec < 60 ? elapsedSec + 's' : elapsedMin + 'm'}`;
        saveStatusTextEl.style.color = 'var(--green)';
    }
}

async function openProfileModal() {
    if (!profileModalEl) return;
    profileModalEl.style.display = 'flex';
    if (profileListContainerEl) {
        profileListContainerEl.innerHTML = '<div class="profile-list-empty">Carregando perfis da nuvem...</div>';
    }

    try {
        const res = await fetch('/api/state?action=list_users');
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        renderProfileList(data.users || []);
    } catch (e) {
        console.warn('Erro ao carregar lista de usuários:', e);
        if (profileListContainerEl) {
            profileListContainerEl.innerHTML = '<div class="profile-list-empty">Não foi possível carregar os perfis. Crie um novo abaixo!</div>';
        }
    }
}

function renderProfileList(users) {
    if (!profileListContainerEl) return;
    if (!users || users.length === 0) {
        profileListContainerEl.innerHTML = '<div class="profile-list-empty">Nenhum perfil criado ainda. Seja o primeiro a criar!</div>';
        return;
    }

    users.sort((a, b) => (b.totalMakitasMade || b.makitas || 0) - (a.totalMakitasMade || a.makitas || 0));

    profileListContainerEl.innerHTML = '';
    users.forEach((user, idx) => {
        const item = document.createElement('div');
        const isCurrent = user.id === currentUserId;
        item.className = 'profile-item' + (isCurrent ? ' is-active-user' : '');
        
        const medal = idx === 0 ? '🥇' : (idx === 1 ? '🥈' : (idx === 2 ? '🥉' : '👤'));
        const score = formatCompactNumber(user.totalMakitasMade || user.makitas || 0);

        item.innerHTML = `
            <div class="profile-item__left">
                <span class="profile-item__icon">${medal}</span>
                <div>
                    <div class="profile-item__name">${user.name} ${isCurrent ? '<small style="color: var(--green); font-weight: normal;">(Atual)</small>' : ''}</div>
                    <div class="profile-item__score">${score} Makitas acumuladas</div>
                </div>
            </div>
            <button class="profile-item__btn">${isCurrent ? 'Continuar' : 'Jogar'}</button>
        `;

        item.addEventListener('click', () => {
            selectProfile(user);
        });

        profileListContainerEl.appendChild(item);
    });
}

function selectProfile(user) {
    if (!user || !user.id) return;
    currentUserId = user.id;
    currentUserName = user.name;
    currentUserCreatedAt = user.createdAt || Date.now();

    try {
        localStorage.setItem('makita_active_user_id', currentUserId);
        localStorage.setItem('makita_active_user_name', currentUserName);
        localStorage.setItem('makita_active_user_created_at', String(currentUserCreatedAt));
    } catch (e) {}

    updateProfileUI();
    if (profileModalEl) profileModalEl.style.display = 'none';

    // Primeiro carrega o save local desse perfil (se houver) para resposta imediata
    const hadLocal = loadLocalState();
    if (!hadLocal) {
        makitas = 0;
        mps = 0;
        totalMakitasMade = 0;
        totalClicks = 0;
        prevMakitas = 0;
        pendingClicks = 0;
        upgrades.forEach(u => { owned[u.id] = 0; });
        permanentUpgrades.forEach(u => { u.purchased = false; });
        mps = calculateLocalMps();
        serverClickPower = calculateLocalClickPower();
        isDirty = true;
        renderUI();
    }

    // Busca o save mais recente na nuvem
    fetchUserProfileState(currentUserId);
}

async function fetchUserProfileState(userId) {
    try {
        const res = await fetch(`/api/state?userId=${encodeURIComponent(userId)}`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        applyCompactState(data);
        saveLocalState();
        if (typeof data.lastSavedAt === 'number' && data.lastSavedAt > 0) {
            lastCloudSaveTime = data.lastSavedAt;
        } else {
            lastCloudSaveTime = Date.now();
        }
        if (data.topPlayer) {
            latestTopPlayer = data.topPlayer;
        }
        hasUnsavedChanges = false;
        updateSaveIndicator();
        renderStats();
    } catch (e) {
        console.warn('Erro ao carregar estado do perfil na nuvem:', e);
    }
}

async function createNewProfile(name) {
    const cleanName = String(name || '').trim();
    if (!cleanName) return;

    const btnCreate = document.getElementById('btnCreateProfile');
    if (btnCreate) {
        btnCreate.disabled = true;
        btnCreate.textContent = 'Salvando no KV...';
    }

    try {
        const res = await fetch('/api/state', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'create_user', name: cleanName })
        });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        if (data.success && data.user) {
            selectProfile(data.user);
        }
    } catch (e) {
        alert('Erro ao criar perfil na nuvem: ' + e.message);
    } finally {
        if (btnCreate) {
            btnCreate.disabled = false;
            btnCreate.textContent = 'Criar Perfil e Jogar 🚀';
        }
    }
}

async function saveUserProgressToCloud(isManual = false) {
    if (!currentUserId) return;

    if (isManual && btnSaveCloudEl) {
        btnSaveCloudEl.disabled = true;
        btnSaveCloudEl.textContent = '⏳ Salvando...';
    }
    if (saveStatusTextEl) {
        saveStatusTextEl.textContent = '🟡 Salvando na Nuvem...';
        saveStatusTextEl.style.color = 'var(--orange)';
    }

    const payload = {
        action: 'save_user_state',
        userId: currentUserId,
        state: getCompactGameState()
    };

    try {
        const res = await fetch('/api/state', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        lastCloudSaveTime = Date.now();
        hasUnsavedChanges = false;

        if (data && data.topPlayer) {
            latestTopPlayer = data.topPlayer;
        }

        saveLocalState();
        updateSaveIndicator();
        renderStats();

        if (isManual) {
            showFloatText('💾 Salvo!');
            if (btnSaveCloudEl) {
                btnSaveCloudEl.disabled = false;
                btnSaveCloudEl.textContent = '✅ Salvo!';
                setTimeout(() => {
                    if (btnSaveCloudEl && btnSaveCloudEl.textContent === '✅ Salvo!') {
                        btnSaveCloudEl.textContent = '💾 Salvar na Nuvem';
                    }
                }, 1800);
            }
        }
    } catch (e) {
        console.warn('Falha ao salvar progresso na nuvem:', e);
        if (saveStatusTextEl) {
            saveStatusTextEl.textContent = '⚠️ Falha ao salvar: ' + (e.message || 'offline');
            saveStatusTextEl.style.color = 'var(--orange)';
        }
        if (isManual && btnSaveCloudEl) {
            btnSaveCloudEl.disabled = false;
            btnSaveCloudEl.textContent = '❌ Erro ao salvar';
            setTimeout(() => {
                if (btnSaveCloudEl && btnSaveCloudEl.textContent === '❌ Erro ao salvar') {
                    btnSaveCloudEl.textContent = '💾 Salvar na Nuvem';
                }
            }, 2500);
        }
    } finally {
        if (btnSaveCloudEl && !isManual) {
            btnSaveCloudEl.disabled = false;
            btnSaveCloudEl.textContent = '💾 Salvar na Nuvem';
        }
    }
}

// Auto-Save periódico no KV a cada 3 minutos (180.000 ms)
setInterval(() => {
    if (currentUserId && hasUnsavedChanges) {
        saveUserProgressToCloud(false);
    }
}, 180000);

// Indicador visual de tempo decorrido do save atualizado a cada 2s
setInterval(updateSaveIndicator, 2000);

// Proteção antes de fechar a página (se não salvo por mais de 5 minutos)
window.addEventListener('beforeunload', (e) => {
    saveLocalState();

    const unsavedMs = Date.now() - lastCloudSaveTime;
    const FIVE_MIN_MS = 5 * 60 * 1000;

    if (hasUnsavedChanges && unsavedMs > FIVE_MIN_MS) {
        e.preventDefault();
        e.returnValue = 'Você tem progresso não salvo na nuvem por mais de 5 minutos! Deseja realmente sair sem salvar?';
        return e.returnValue;
    }
});

const btnTestPing = document.getElementById('btnTestPing');
if (btnTestPing) {
    btnTestPing.addEventListener('click', async () => {
        btnTestPing.disabled = true;
        const prevText = btnTestPing.textContent;
        btnTestPing.textContent = '🔄 Medindo...';
        await Promise.all([syncWithCloud(), fetchRemoteVersion()]);
        btnTestPing.textContent = prevText;
        btnTestPing.disabled = false;
    });
}

async function syncWithCloud(actionPayload = null) {
    if (!window.location.hostname || window.location.protocol === 'file:') {
        logEl.textContent = '🛠️ Modo de Teste Local Ativo (Simulador Offline)';
        logEl.style.color = 'var(--orange)';
        return;
    }

    if (isSyncing && !actionPayload) return;
    isSyncing = true;

    let body = null;
    let clicksSent = 0;

    const permsPayload = {};
    permanentUpgrades.forEach(p => { permsPayload[p.id] = p.purchased; });

    if (actionPayload) {
        body = { ...actionPayload };
        if (pendingClicks > 0 && actionPayload.action !== 'reset') {
            body.clicks = (body.clicks || 0) + pendingClicks;
            clicksSent = pendingClicks;
            pendingClicks = 0;
        }
        body.userId = currentUserId;
        body.makitas = makitas;
        body.totalMakitasMade = totalMakitasMade;
        body.owned = owned;
        body.perms = permsPayload;
    } else {
        clicksSent = pendingClicks;
        pendingClicks = 0;
        body = {
            action: 'sync',
            source: 'web',
            userId: currentUserId,
            clicks: clicksSent,
            makitas,
            totalMakitasMade,
            owned,
            perms: permsPayload
        };
    }

    const pingStart = performance.now();
    try {
        const res = await fetch('/api/state', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);

        measuredPingMs = Math.round(performance.now() - pingStart);
        const data = await res.json();
        applyServerState(data);
    } catch (err) {
        measuredPingMs = null;
        pendingClicks += clicksSent;
        logEl.textContent = '⚠️ Modo autônomo local (aguardando conexão com a nuvem)';
        logEl.style.color = 'var(--orange)';
    } finally {
        isSyncing = false;
        updateStatusUI();
    }
}

// Sincronização periódica de telemetria a cada 15 segundos em segundo plano
setInterval(() => {
    syncWithCloud();
}, 15000);

document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'hidden') {
        saveLocalState();
        if (currentUserId && hasUnsavedChanges) {
            saveUserProgressToCloud(false);
        }
    }
});

// ---------- BOTÃO DE RESET TOTAL ----------
const resetGameBtn = document.getElementById('resetGameBtn');
if (resetGameBtn) {
    resetGameBtn.addEventListener('click', () => {
        if (confirm('Tem certeza que deseja resetar TODO o seu progresso (saldo, melhorias e estatísticas)?')) {
            resetAllProgress(true);
        }
    });
}

// ---------- MOTOR DE RENDERIZAÇÃO CONTÍNUA DO CLIENTE (TAXA NATIVA DO MONITOR) ----------
let lastFrameTime = performance.now();

function gameLoop(now) {
    const dt = Math.min(0.2, (now - lastFrameTime) / 1000.0);
    lastFrameTime = now;

    // Produção contínua e suave na taxa de quadros nativa da GPU/Monitor
    if (mps > 0) {
        const gain = mps * dt;
        makitas += gain;
        totalMakitasMade += gain;
        hasUnsavedChanges = true;
    }

    // Atualização rápida de texto (apenas 2 elementos DOM)
    counterEl.textContent = formatCompactNumber(makitas);
    counterEl.title = formatFullNumber(makitas) + ' Makitas';
    rateEl.textContent = mps >= 1000 ? formatCompactNumber(mps) : mps.toFixed(1);
    rateEl.title = mps.toLocaleString('pt-BR') + ' por segundo';

    // Renderização throttled de listas/botões para máxima eficiência
    if (isDirty || (now - lastThrottledRender >= THROTTLE_RENDER_MS)) {
        isDirty = false;
        lastThrottledRender = now;
        renderUI();
    }

    requestAnimationFrame(gameLoop);
}

// ---------- INICIALIZAÇÃO DO JOGO ----------
function initGame() {
    currentUserId = localStorage.getItem('makita_active_user_id') || null;
    currentUserName = localStorage.getItem('makita_active_user_name') || null;
    const storedCreatedAt = localStorage.getItem('makita_active_user_created_at');
    if (storedCreatedAt) {
        currentUserCreatedAt = Number(storedCreatedAt);
    }

    if (btnSwitchProfileEl) {
        btnSwitchProfileEl.addEventListener('click', () => {
            openProfileModal();
        });
    }

    if (btnSaveCloudEl) {
        btnSaveCloudEl.addEventListener('click', () => {
            saveUserProgressToCloud(true);
        });
    }

    if (newProfileFormEl) {
        newProfileFormEl.addEventListener('submit', (e) => {
            e.preventDefault();
            if (newProfileInputEl) {
                createNewProfile(newProfileInputEl.value);
                newProfileInputEl.value = '';
            }
        });
    }

    buildShopList();
    buildPermTree();

    if (currentUserId) {
        updateProfileUI();
        loadLocalState();
        fetchUserProfileState(currentUserId);
    } else {
        openProfileModal();
    }

    renderUI();
    syncWithCloud();
    fetchRemoteVersion();

    setInterval(fetchRemoteVersion, 60000); // Consulta nova versão remota a cada 1 minuto
    setInterval(() => {
        updateStatusUI();
        renderStats();
    }, 1000); // Atualiza contadores, telemetria e estatísticas a cada segundo

    // Inicia o motor gráfico irrestrito (suave e fluido)
    requestAnimationFrame(gameLoop);
}

initGame();

