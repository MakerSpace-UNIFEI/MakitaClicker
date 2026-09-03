// =====================================================================
// MAKITA CLICKER - MOTOR DE JOGO (CLIENTE 60 FPS) + SYNC COM NUVEM
// Sincronização Inteligente (Cloud Master / Local Slave) + LocalStorage
// =====================================================================

let isLocalMode = false;
let pendingClicks = 0;
let serverClickPower = 1.0;

// ---------- estado ----------
let makitas = 0;
let mps = 0;
let buyQty = 1;
const MAX_OWNED = 100;

// Total de makitas acumuladas no histórico
let totalMakitasMade = 0;
let prevMakitas = 0;

// Controle de Renderização Otimizada (desacoplada do tick de 60 FPS)
let isDirty = true;
let lastThrottledRender = 0;
const THROTTLE_RENDER_MS = 150; // Atualiza estados de botões/árvore a ~6 FPS

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

// ---------- PERSISTÊNCIA LOCAL (LOCALSTORAGE) ----------
const LOCAL_STORAGE_KEY = 'makitaclicker_save';

function saveLocalState() {
    try {
        const permsState = {};
        permanentUpgrades.forEach(p => { permsState[p.id] = p.purchased; });
        const saveObj = {
            makitas,
            totalMakitasMade,
            owned,
            perms: permsState,
            timestamp: Date.now()
        };
        localStorage.setItem(LOCAL_STORAGE_KEY, JSON.stringify(saveObj));
    } catch (e) {
        // LocalStorage desabilitado ou cheio
    }
}

function loadLocalState() {
    try {
        const raw = localStorage.getItem(LOCAL_STORAGE_KEY);
        if (!raw) return false;
        const data = JSON.parse(raw);
        if (typeof data.makitas === 'number') makitas = data.makitas;
        if (typeof data.totalMakitasMade === 'number') totalMakitasMade = data.totalMakitasMade;
        if (data.owned && typeof data.owned === 'object') {
            upgrades.forEach(u => {
                if (typeof data.owned[u.id] === 'number') {
                    owned[u.id] = data.owned[u.id];
                }
            });
        }
        if (data.perms && typeof data.perms === 'object') {
            permanentUpgrades.forEach(p => {
                if (data.perms[p.id] === true) {
                    p.purchased = true;
                }
            });
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

    syncWithCloud({ action: 'buy', upgradeId: upgrade.id, qty: buyQty });
    saveLocalState();
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

    syncWithCloud({ action: 'perm_buy', permId: perm.id });
    saveLocalState();
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

    if (statPermCountEl) {
        const purchasedCount = permanentUpgrades.filter(u => u.purchased).length;
        statPermCountEl.textContent = `${purchasedCount}/${permanentUpgrades.length}`;
    }

    // Meta 99B
    const goalTotal = 99000000000;
    const currentProgress = Math.max(makitas, totalMakitasMade);
    const pct = Math.min(100, (currentProgress / goalTotal) * 100);

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
function showFloatPlus(amount) {
    const rect = makitaBtn.getBoundingClientRect();
    const areaRect = clickArea.getBoundingClientRect();
    const el = document.createElement('span');
    el.className = 'float-plus';
    el.textContent = '+' + (amount >= 1000 ? formatCompactNumber(amount) : (amount % 1 === 0 ? amount : amount.toFixed(1)));
    el.style.left = (rect.left - areaRect.left + rect.width / 2) + 'px';
    el.style.top = (rect.top - areaRect.top) + 'px';
    clickArea.appendChild(el);
    el.addEventListener('animationend', () => el.remove());
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
    pendingClicks++;
    isDirty = true;
    playClickFeedback(gain);
});

// ---------- RESET TOTAL UNIFICADO ----------
function resetAllProgress(sendToServer = true) {
    try {
        localStorage.removeItem(LOCAL_STORAGE_KEY);
        localStorage.removeItem('makita_perm_upgrades');
        localStorage.removeItem('makita_total_produced');
        localStorage.removeItem('makita_local_state');
    } catch (e) {}

    makitas = 0;
    mps = 0;
    totalMakitasMade = 0;
    prevMakitas = 0;
    pendingClicks = 0;
    upgrades.forEach(u => { owned[u.id] = 0; });
    permanentUpgrades.forEach(u => { u.purchased = false; });
    serverClickPower = 1.0;
    isDirty = true;

    if (sendToServer) {
        syncWithCloud({ action: 'reset' });
    }

    renderUI();
}

// ---------- COMUNICAÇÃO HTTP COM CLOUDFLARE PAGES & KV ----------
let isSyncing = false;

function applyServerState(data) {
    if (!data) return;

    if (data.isReset === true) {
        resetAllProgress(false);
        return;
    }

    if (typeof data.makitas === 'number') {
        const localPendingGain = pendingClicks * calculateLocalClickPower();
        // A nuvem é o MASTER: se os dados oficiais da nuvem chegaram, reconcilia
        if (data.makitas + localPendingGain > makitas) {
            totalMakitasMade += ((data.makitas + localPendingGain) - makitas);
            makitas = data.makitas + localPendingGain;
        } else if (Math.abs(data.makitas - makitas) > 200) {
            makitas = data.makitas + localPendingGain;
        }
    }
    
    if (data.perms && typeof data.perms === 'object') {
        permanentUpgrades.forEach(u => {
            if (u.id in data.perms) {
                u.purchased = (data.perms[u.id] === true);
            }
        });
    }

    if (data.owned && typeof data.owned === 'object') {
        upgrades.forEach(upgrade => {
            if (upgrade.id in data.owned) {
                owned[upgrade.id] = data.owned[upgrade.id];
            }
        });
    }

    if (typeof data.mps === 'number' && data.mps >= 0) {
        mps = data.mps;
    } else {
        mps = calculateLocalMps();
    }
    serverClickPower = calculateLocalClickPower();

    isDirty = true;
    saveLocalState();
}

async function syncWithCloud(actionPayload = null) {
    if (!window.location.hostname || window.location.protocol === 'file:') {
        logEl.textContent = '🛠️ Modo de Teste Local Ativo (Simulador Offline 60 FPS)';
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
        body.makitas = makitas;
        body.totalMakitasMade = totalMakitasMade;
        body.owned = owned;
        body.perms = permsPayload;
    } else {
        clicksSent = pendingClicks;
        pendingClicks = 0;
        body = {
            action: 'sync',
            clicks: clicksSent,
            makitas,
            totalMakitasMade,
            owned,
            perms: permsPayload
        };
    }

    try {
        const res = await fetch('/api/state', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);

        const data = await res.json();
        applyServerState(data);

        logEl.textContent = '🟢 Conectado ao Cloudflare Pages & KV (Sync Ativo)';
        logEl.style.color = 'var(--text-lo)';
    } catch (err) {
        // Recupera cliques caso falhe o envio
        pendingClicks += clicksSent;
        logEl.textContent = '⚠️ Modo autônomo local (aguardando conexão com a nuvem)';
        logEl.style.color = 'var(--orange)';
    } finally {
        isSyncing = false;
    }
}

// Sincronização periódica a cada 10 segundos em segundo plano
setInterval(() => {
    syncWithCloud();
}, 10000);

// Envio seguro ao trocar de aba ou fechar o navegador
window.addEventListener('beforeunload', () => {
    if (pendingClicks > 0) {
        const payload = JSON.stringify({ action: 'sync', clicks: pendingClicks, makitas });
        if (navigator.sendBeacon) {
            navigator.sendBeacon('/api/state', new Blob([payload], { type: 'application/json' }));
        }
    }
    saveLocalState();
});

document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'hidden') {
        saveLocalState();
        if (pendingClicks > 0) {
            syncWithCloud();
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

// ---------- MOTOR DE RENDERIZAÇÃO CONTÍNUA DO CLIENTE (60 FPS) ----------
let lastFrameTime = performance.now();

function gameLoop(now) {
    const dt = Math.min(0.2, (now - lastFrameTime) / 1000.0);
    lastFrameTime = now;

    // Produção contínua e suave a 60 FPS
    if (mps > 0) {
        const gain = mps * dt;
        makitas += gain;
        totalMakitasMade += gain;
    }

    // Atualização rápida de texto a 60 FPS (apenas 2 elementos DOM)
    counterEl.textContent = formatCompactNumber(makitas);
    counterEl.title = formatFullNumber(makitas) + ' Makitas';
    rateEl.textContent = mps >= 1000 ? formatCompactNumber(mps) : mps.toFixed(1);
    rateEl.title = mps.toLocaleString('pt-BR') + ' por segundo';

    // Renderização throttled de listas/botões (6 FPS ou quando dirty) para máxima eficiência
    if (isDirty || (now - lastThrottledRender >= THROTTLE_RENDER_MS)) {
        isDirty = false;
        lastThrottledRender = now;
        renderUI();
    }

    requestAnimationFrame(gameLoop);
}

// ---------- INICIALIZAÇÃO DO JOGO ----------
loadLocalState();
mps = calculateLocalMps();
serverClickPower = calculateLocalClickPower();

buildShopList();
buildPermTree();
renderUI();
syncWithCloud();

// Inicia o motor gráfico suave a 60 FPS
requestAnimationFrame(gameLoop);
