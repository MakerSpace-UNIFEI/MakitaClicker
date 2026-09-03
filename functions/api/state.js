// =====================================================================
// MAKITA CLICKER - CLOUDFLARE PAGES FUNCTION: /api/state
// API Serverless autoritativa com persistência no Cloudflare KV
// Master de Estado + Sincronização Inteligente (Cloud Master / Client Slave)
// =====================================================================

const KV_KEY = 'gamestate';
const MAX_OWNED = 100;

// Configuração das 24 oficinas (idêntico ao ESP e Web)
const UPGRADES = [
  { id: 'upgrade1',          baseCost: 10,           growth: 1.10, mps: 0.1 },
  { id: 'upgrade_1mps',      baseCost: 100,          growth: 1.12, mps: 1.0 },
  { id: 'upgrade_2mps',      baseCost: 250,          growth: 1.12, mps: 2.0 },
  { id: 'upgrade_5mps',      baseCost: 750,          growth: 1.13, mps: 5.0 },
  { id: 'upgrade_10mps',     baseCost: 1800,         growth: 1.13, mps: 10.0 },
  { id: 'upgrade_15mps',     baseCost: 3500,         growth: 1.14, mps: 15.0 },
  { id: 'upgrade_20mps',     baseCost: 6000,         growth: 1.14, mps: 20.0 },
  { id: 'upgrade_25mps',     baseCost: 10000,        growth: 1.14, mps: 25.0 },
  { id: 'upgrade_30mps',     baseCost: 16000,        growth: 1.15, mps: 30.0 },
  { id: 'upgrade_50mps',     baseCost: 35000,        growth: 1.15, mps: 50.0 },
  { id: 'upgrade_100mps',    baseCost: 100000,       growth: 1.15, mps: 100.0 },
  { id: 'upgrade_200mps',    baseCost: 300000,       growth: 1.16, mps: 200.0 },
  { id: 'upgrade_500mps',    baseCost: 1000000,      growth: 1.16, mps: 500.0 },
  { id: 'upgrade_1200mps',   baseCost: 3500000,      growth: 1.16, mps: 1200.0 },
  { id: 'upgrade_3000mps',   baseCost: 12000000,     growth: 1.16, mps: 3000.0 },
  { id: 'upgrade_8000mps',   baseCost: 40000000,     growth: 1.17, mps: 8000.0 },
  { id: 'upgrade_20kmps',    baseCost: 150000000,    growth: 1.17, mps: 20000.0 },
  { id: 'upgrade_60kmps',    baseCost: 500000000,    growth: 1.17, mps: 60000.0 },
  { id: 'upgrade_180kmps',   baseCost: 1800000000,   growth: 1.17, mps: 180000.0 },
  { id: 'upgrade_500kmps',   baseCost: 6000000000,   growth: 1.18, mps: 500000.0 },
  { id: 'upgrade_1500kmps',  baseCost: 20000000000,  growth: 1.18, mps: 1500000.0 },
  { id: 'upgrade_5000kmps',  baseCost: 60000000000,  growth: 1.18, mps: 5000000.0 },
  { id: 'upgrade_15000kmps', baseCost: 200000000000, growth: 1.19, mps: 15000000.0 },
  { id: 'upgrade_50000kmps', baseCost: 800000000000, growth: 1.19, mps: 50000000.0 }
];

// Configuração das 20 tecnologias permanentes (Skill Tree)
const PERMANENT_UPGRADES = [
  { id: 'perm_lubrificante', cost: 25, req: 10, parent: null },
  { id: 'perm_disco_diamante', cost: 100, req: 50, parent: 'perm_lubrificante' },
  { id: 'perm_motor_brushless', cost: 300, req: 150, parent: 'perm_lubrificante' },
  { id: 'perm_empunhadura', cost: 600, req: 250, parent: 'perm_disco_diamante' },
  { id: 'perm_bateria_litio', cost: 1500, req: 600, parent: 'perm_motor_brushless' },
  { id: 'perm_ia_maker', cost: 5000, req: 2000, parent: 'perm_bateria_litio' },
  { id: 'perm_refrigeracao', cost: 15000, req: 6000, parent: 'perm_motor_brushless' },
  { id: 'perm_titanio', cost: 35000, req: 12000, parent: 'perm_disco_diamante' },
  { id: 'perm_overclock', cost: 100000, req: 30000, parent: 'perm_empunhadura' },
  { id: 'perm_nanobots', cost: 250000, req: 80000, parent: 'perm_ia_maker' },
  { id: 'perm_singularidade', cost: 1000000, req: 300000, parent: 'perm_nanobots' },
  { id: 'perm_plasma_cutter', cost: 5000000, req: 1500000, parent: 'perm_titanio' },
  { id: 'perm_fusao_fria', cost: 20000000, req: 6000000, parent: 'perm_singularidade' },
  { id: 'perm_hiperconducao', cost: 80000000, req: 25000000, parent: 'perm_fusao_fria' },
  { id: 'perm_sinergia_quantica', cost: 300000000, req: 100000000, parent: 'perm_overclock' },
  { id: 'perm_laser_gama', cost: 1200000000, req: 400000000, parent: 'perm_plasma_cutter' },
  { id: 'perm_taquions', cost: 5000000000, req: 1500000000, parent: 'perm_hiperconducao' },
  { id: 'perm_materia_escura', cost: 20000000000, req: 6000000000, parent: 'perm_taquions' },
  { id: 'perm_hiper_clique', cost: 50000000000, req: 15000000000, parent: 'perm_laser_gama' },
  { id: 'perm_onipotencia_maker', cost: 99000000000, req: 35000000000, parent: 'perm_materia_escura' }
];

const CORS_HEADERS = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
  'Access-Control-Allow-Headers': 'Content-Type',
  'Content-Type': 'application/json; charset=utf-8'
};

// Fallback em memória caso o binding MAKITA_KV ainda não esteja vinculado
let memoryFallbackState = null;

function getDefaultState() {
  const owned = {};
  UPGRADES.forEach(u => { owned[u.id] = 0; });
  const perms = {};
  PERMANENT_UPGRADES.forEach(p => { perms[p.id] = false; });

  return {
    makitas: 0.0,
    mps: 0.0,
    clickPower: 1.0,
    totalOwned: 0,
    totalMakitasMade: 0.0,
    owned,
    perms,
    lastUpdate: Date.now()
  };
}

function unitCost(upgrade, count) {
  return Math.ceil(upgrade.baseCost * Math.pow(upgrade.growth, count));
}

function calculateClickPower(perms) {
  let power = 1.0;
  if (perms?.perm_disco_diamante) power += 1.0;
  if (perms?.perm_titanio) power += 3.0;
  if (perms?.perm_plasma_cutter) power += 25.0;
  if (perms?.perm_laser_gama) power += 200.0;
  if (perms?.perm_singularidade) power *= 3.0;
  if (perms?.perm_hiper_clique) power *= 10.0;
  return power;
}

function calculateMps(owned, perms) {
  let baseMps = 0.0;
  UPGRADES.forEach(u => {
    baseMps += ((owned?.[u.id] || 0) * u.mps);
  });

  let workshopMultiplier = 1.0;
  if (perms?.perm_motor_brushless) workshopMultiplier *= 2.0;
  if (perms?.perm_hiperconducao) workshopMultiplier *= 3.0;
  if (perms?.perm_onipotencia_maker) workshopMultiplier *= 4.0;
  baseMps *= workshopMultiplier;

  let multiplier = 1.0;
  if (perms?.perm_lubrificante) multiplier += 0.10;
  if (perms?.perm_refrigeracao) multiplier += 0.20;
  if (perms?.perm_bateria_litio) multiplier += 0.25;
  if (perms?.perm_ia_maker) multiplier += 0.50;
  if (perms?.perm_nanobots) multiplier += 0.75;
  if (perms?.perm_fusao_fria) multiplier += 1.00;
  if (perms?.perm_singularidade) multiplier += 1.50;
  if (perms?.perm_taquions) multiplier += 2.00;
  if (perms?.perm_materia_escura) multiplier += 3.00;
  if (perms?.perm_onipotencia_maker) multiplier += 5.00;

  return baseMps * multiplier;
}

function getSingleClickGain(perms, mps) {
  let gain = calculateClickPower(perms);
  if (perms?.perm_onipotencia_maker) {
    gain += (mps * 0.30);
  } else if (perms?.perm_sinergia_quantica) {
    gain += (mps * 0.20);
  } else if (perms?.perm_overclock) {
    gain += (mps * 0.10);
  } else if (perms?.perm_empunhadura) {
    gain += (mps * 0.05);
  }
  return gain;
}

function getTotalOwned(owned) {
  let total = 0;
  for (const k in owned) {
    total += (owned[k] || 0);
  }
  return total;
}

// Produção passiva autoritativa baseada no delta de tempo entre chamadas
function advancePassiveProduction(state, now) {
  const last = state.lastUpdate || now;
  const dt = Math.max(0, (now - last) / 1000.0);
  const currentMps = calculateMps(state.owned, state.perms);
  if (dt > 0 && currentMps > 0) {
    const gain = currentMps * dt;
    state.makitas = (state.makitas || 0) + gain;
    state.totalMakitasMade = (state.totalMakitasMade || 0) + gain;
  }
  state.lastUpdate = now;
  state.mps = currentMps;
  state.clickPower = calculateClickPower(state.perms);
  state.totalOwned = getTotalOwned(state.owned);
}

async function loadState(env) {
  if (env && env.MAKITA_KV) {
    try {
      const data = await env.MAKITA_KV.get(KV_KEY, { type: 'json' });
      if (data) return data;
    } catch (err) {
      console.error('[KV] Erro ao ler MAKITA_KV:', err);
    }
  }
  if (!memoryFallbackState) {
    memoryFallbackState = getDefaultState();
  }
  return memoryFallbackState;
}

async function saveState(env, state) {
  if (env && env.MAKITA_KV) {
    try {
      await env.MAKITA_KV.put(KV_KEY, JSON.stringify(state));
    } catch (err) {
      console.error('[KV] Erro ao gravar MAKITA_KV:', err);
    }
  }
  memoryFallbackState = state;
}

// ------------------- HANDLERS -------------------

export async function onRequestOptions() {
  return new Response(null, { status: 204, headers: CORS_HEADERS });
}

export async function onRequestGet(context) {
  const { env } = context;
  const state = await loadState(env);
  const now = Date.now();

  advancePassiveProduction(state, now);
  await saveState(env, state);

  return new Response(JSON.stringify(state), {
    status: 200,
    headers: CORS_HEADERS
  });
}

export async function onRequestPost(context) {
  const { request, env } = context;
  let body = {};
  try {
    body = await request.json();
  } catch (e) {
    // Body vazio ou malformado
  }

  const state = await loadState(env);
  const now = Date.now();
  advancePassiveProduction(state, now);

  const action = body.action || (body.clicks ? 'sync' : '');

  // Reset total
  if (action === 'reset') {
    const newState = getDefaultState();
    await saveState(env, newState);
    return new Response(JSON.stringify({ ...newState, isReset: true }), {
      status: 200,
      headers: CORS_HEADERS
    });
  }

  // Sincronização (Servidor MASTER):
  // - Servidor é sempre autoritativo para owned/perms (upgrades).
  // - ESP: única exceção permitida é se o contador de makitas for maior; nesse caso apenas
  //   o saldo é aceito, mas owned/perms NUNCA são sobrescritos pelo cliente via sync.
  // - Web (buy/perm_buy): usa fluxo próprio de ações abaixo.
  if (action === 'sync' || action === 'click') {
    const clientMakitas = typeof body.makitas === 'number' ? body.makitas : null;
    const clientTotal = typeof body.totalMakitasMade === 'number' ? body.totalMakitasMade : null;
    const clicks = Math.max(0, Math.min(parseInt(body.clicks || body.count || 0, 10), 5000));
    const isEsp = body.source === 'esp'; // ESP deve enviar source:"esp" para identificação

    if (clientMakitas !== null && clientMakitas > state.makitas) {
      // Cliente tem saldo maior: aceita apenas o contador de makitas
      state.makitas = clientMakitas;
      if (clientTotal && clientTotal > (state.totalMakitasMade || 0)) {
        state.totalMakitasMade = clientTotal;
      } else if (state.makitas > (state.totalMakitasMade || 0)) {
        state.totalMakitasMade = state.makitas;
      }

      // owned e perms: NUNCA sobrescritos via sync — servidor é sempre master
      // (compras só ocorrem via action:'buy' e 'perm_buy')
    } else {
      // Servidor é MASTER: soma cliques físicos pendentes (ESP) sobre o estado da nuvem
      if (clicks > 0) {
        const gainPerClick = getSingleClickGain(state.perms, state.mps);
        const totalGain = gainPerClick * clicks;
        state.makitas += totalGain;
        state.totalMakitasMade = (state.totalMakitasMade || 0) + totalGain;
      }
    }
  } else if (action === 'buy') {
    const upgradeId = body.upgradeId || body.id;
    const qtyStr = String(body.qty || '1');
    const up = UPGRADES.find(u => u.id === upgradeId);

    if (up) {
      state.owned = state.owned || {};
      const currentOwned = state.owned[up.id] || 0;
      const remaining = MAX_OWNED - currentOwned;

      if (remaining > 0) {
        if (qtyStr === 'max') {
          let count = 0;
          while (count < remaining) {
            const nextCost = unitCost(up, currentOwned + count);
            if (state.makitas < nextCost) break;
            state.makitas -= nextCost;
            count++;
          }
          state.owned[up.id] = currentOwned + count;
        } else {
          const requested = Math.max(1, parseInt(qtyStr, 10) || 1);
          const toBuy = Math.min(requested, remaining);
          let totalCost = 0;
          for (let i = 0; i < toBuy; i++) {
            totalCost += unitCost(up, currentOwned + i);
          }
          if (state.makitas >= totalCost && toBuy > 0) {
            state.makitas -= totalCost;
            state.owned[up.id] = currentOwned + toBuy;
          }
        }
      }
    }
  } else if (action === 'perm_buy') {
    const permId = body.permId || body.id;
    const perm = PERMANENT_UPGRADES.find(p => p.id === permId);

    if (perm) {
      state.perms = state.perms || {};
      const alreadyBought = (state.perms[perm.id] === true);
      const parentBought = !perm.parent || (state.perms[perm.parent] === true);
      const reqMet = (state.totalMakitasMade || state.makitas) >= perm.req;

      if (!alreadyBought && parentBought && reqMet && state.makitas >= perm.cost) {
        state.makitas -= perm.cost;
        state.perms[perm.id] = true;
      }
    }
  }

  // Recalcula MPS, ClickPower e totalOwned
  state.mps = calculateMps(state.owned, state.perms);
  state.clickPower = calculateClickPower(state.perms);
  state.totalOwned = getTotalOwned(state.owned);
  state.lastUpdate = now;

  // Persiste autoritativamente no Cloudflare KV
  await saveState(env, state);

  return new Response(JSON.stringify(state), {
    status: 200,
    headers: CORS_HEADERS
  });
}
