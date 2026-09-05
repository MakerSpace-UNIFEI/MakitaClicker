// =====================================================================
// MAKITA CLICKER - CLOUDFLARE PAGES FUNCTION: /api/state
// API Serverless autoritativa com persistência no Cloudflare KV
// Master de Estado + Sincronização Inteligente (Cloud Master / Client Slave)
// =====================================================================

const KV_KEY = 'gamestate';
const USERS_LIST_KEY = 'users:list';
const MAX_OWNED = 100;
// Hash SHA-256 criptográfico de 'ADMMAKER' para autenticação segura e irreversível no painel administrativo
const ADMIN_AUTH_HASH = 'c9a2abd67ad59717195e5d8a6f917ba5084d81af244b0a8d40c8b30f234742d7';

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
let memoryFallbackUsers = [];
let memoryFallbackUserStates = {};

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
    resetId: 0,
    resetPendingEsp: false,
    lastResetAckAt: 0,
    espTelemetry: null,
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

// =====================================================================
// SERIALIZAÇÃO COMPACTA INDEXADA (ECONOMIA DE KV & REDE)
// =====================================================================

function getUserStateKey(userId) {
  return `user:${userId}:state`;
}

// Converte estado operacional para representação compacta no KV
function compactUserState(state) {
  const upgradesArr = UPGRADES.map(u => {
    if (state.owned && typeof state.owned[u.id] === 'number') {
      return state.owned[u.id];
    }
    return 0;
  });

  const permsArr = [];
  PERMANENT_UPGRADES.forEach((p, idx) => {
    if (state.perms && state.perms[p.id] === true) {
      permsArr.push(idx);
    }
  });

  return {
    makitas: typeof state.makitas === 'number' ? state.makitas : 0,
    totalMakitasMade: typeof state.totalMakitasMade === 'number' ? state.totalMakitasMade : (state.makitas || 0),
    upgrades: upgradesArr,
    perms: permsArr,
    resetEpoch: state.resetEpoch || 0,
    lastUpdate: state.lastUpdate || Date.now(),
    lastSavedAt: Date.now()
  };
}

// Expande estado compacto para o formato operacional completo
function expandUserState(raw) {
  if (!raw || typeof raw !== 'object') return getDefaultState();

  const owned = {};
  UPGRADES.forEach((u, idx) => {
    if (Array.isArray(raw.upgrades)) {
      owned[u.id] = Math.max(0, Math.min(MAX_OWNED, parseInt(raw.upgrades[idx] || 0, 10)));
    } else if (raw.owned && typeof raw.owned === 'object') {
      owned[u.id] = Math.max(0, Math.min(MAX_OWNED, parseInt(raw.owned[u.id] || 0, 10)));
    } else {
      owned[u.id] = 0;
    }
  });

  const perms = {};
  PERMANENT_UPGRADES.forEach((p, idx) => {
    if (Array.isArray(raw.perms)) {
      perms[p.id] = raw.perms.includes(idx);
    } else if (raw.perms && typeof raw.perms === 'object') {
      perms[p.id] = raw.perms[p.id] === true;
    } else {
      perms[p.id] = false;
    }
  });

  const mps = calculateMps(owned, perms);
  const clickPower = calculateClickPower(perms);
  const totalOwned = getTotalOwned(owned);
  const makitas = typeof raw.makitas === 'number' ? raw.makitas : 0;
  const totalMakitasMade = typeof raw.totalMakitasMade === 'number' ? raw.totalMakitasMade : makitas;

  return {
    makitas,
    mps,
    clickPower,
    totalOwned,
    totalMakitasMade,
    owned,
    perms,
    resetEpoch: raw.resetEpoch || 0,
    lastUpdate: raw.lastUpdate || Date.now(),
    lastSavedAt: raw.lastSavedAt || Date.now()
  };
}

// Identifica o jogador com o maior progresso para telemetria no LCD da ESP8266
function getTopPlayer(usersList) {
  if (!Array.isArray(usersList) || usersList.length === 0) {
    return { name: 'MakerSpace', makitas: 0, totalMakitasMade: 0 };
  }
  let top = usersList[0];
  for (const u of usersList) {
    const currentScore = typeof u.totalMakitasMade === 'number' ? u.totalMakitasMade : (u.makitas || 0);
    const topScore = typeof top.totalMakitasMade === 'number' ? top.totalMakitasMade : (top.makitas || 0);
    if (currentScore > topScore) {
      top = u;
    }
  }
  return {
    name: top.name || 'Maker',
    makitas: top.makitas || 0,
    totalMakitasMade: top.totalMakitasMade || top.makitas || 0
  };
}

function getKV(env) {
  if (!env) {
    return { kv: null, name: null, kvName: null, kvConnected: false, diag: 'Objeto context.env não fornecido pelo Cloudflare Pages.' };
  }
  if (env.MAKITA_KV && typeof env.MAKITA_KV.get === 'function') {
    return { kv: env.MAKITA_KV, name: 'MAKITA_KV', kvName: 'MAKITA_KV', kvConnected: true, diag: 'MAKITA_KV conectado com sucesso.' };
  }
  for (const [key, val] of Object.entries(env)) {
    if (key === 'ASSETS') continue; // Ignora o repositório interno de assets estáticos do Pages
    if (val && typeof val.get === 'function' && typeof val.put === 'function') {
      return { kv: val, name: key, kvName: key, kvConnected: true, diag: `KV detectado via binding alternativo: '${key}'` };
    }
  }

  // Diagnóstico detalhado para troubleshooting no Cloudflare Pages
  const keys = Object.keys(env).filter(k => k !== 'ASSETS');
  const makitaType = typeof env.MAKITA_KV;
  let diag = '';

  if (makitaType === 'string') {
    diag = "Atenção: MAKITA_KV está definida como STRING (Variável de Ambiente comum). No Cloudflare Pages, ela deve ser vinculada como 'KV namespace binding' em Settings > Functions > KV namespace bindings.";
  } else if (makitaType === 'undefined') {
    if (keys.length === 0) {
      diag = "Nenhum binding ou variável foi injetado neste deploy. Se você já configurou o binding no painel, é OBRIGATÓRIO disparar um NOVO deploy (ou clicar em 'Retry deployment') para que a Cloudflare aplique as alterações.";
    } else {
      diag = `Binding MAKITA_KV não encontrado em env (chaves presentes: [${keys.join(', ')}]). Se configurou recentemente, dispare um novo deploy.`;
    }
  } else {
    diag = `MAKITA_KV está presente como '${makitaType}', mas não possui os métodos esperados de KV (.get / .put).`;
  }

  return { kv: null, name: null, kvName: null, kvConnected: false, diag };
}

// Produção passiva autoritativa baseada no delta de tempo.
function advancePassiveProduction(state, now) {
  const last = state.lastUpdate || now;
  const dt = Math.max(0, (now - last) / 1000.0);
  // Garante que o MPS não fique zerado se houver upgrades
  const calculatedMps = calculateMps(state.owned, state.perms);
  if ((!state.mps || state.mps <= 0) && calculatedMps > 0) {
    state.mps = calculatedMps;
  }
  const currentMps = state.mps || calculatedMps || 0;
  if (dt > 0 && currentMps > 0) {
    const gain = currentMps * dt;
    state.makitas = (state.makitas || 0) + gain;
    state.totalMakitasMade = (state.totalMakitasMade || 0) + gain;
  }
  state.lastUpdate = now;
  state.clickPower = calculateClickPower(state.perms);
  state.totalOwned = getTotalOwned(state.owned);
}

async function loadUsersList(env) {
  const { kv } = getKV(env);
  if (kv) {
    try {
      const data = await kv.get(USERS_LIST_KEY, { type: 'json' });
      if (Array.isArray(data)) return data;
    } catch (err) {
      console.error('[KV] Erro ao ler lista de usuários:', err);
    }
  }
  return memoryFallbackUsers;
}

async function saveUsersList(env, list) {
  const { kv } = getKV(env);
  if (kv) {
    try {
      await kv.put(USERS_LIST_KEY, JSON.stringify(list));
    } catch (err) {
      console.error('[KV] Erro ao salvar lista de usuários:', err);
    }
  }
  memoryFallbackUsers = list;
}

async function loadUserState(env, userId) {
  const { kv, name, kvConnected, diag } = getKV(env);
  const key = getUserStateKey(userId);
  if (kv) {
    try {
      const raw = await kv.get(key, { type: 'json' });
      if (raw && typeof raw === 'object') {
        const state = expandUserState(raw);
        return { state, kvName: name, kvConnected: true, kvDiag: diag };
      }
    } catch (err) {
      console.error(`[KV] Erro ao ler estado do usuário ${userId}:`, err);
    }
  }
  const fallback = memoryFallbackUserStates[userId] || getDefaultState();
  return { state: fallback, kvName: name, kvConnected: !!kv, kvDiag: diag };
}

async function saveUserState(env, userId, state) {
  const { kv } = getKV(env);
  const key = getUserStateKey(userId);
  const compact = compactUserState(state);
  if (kv) {
    try {
      await kv.put(key, JSON.stringify(compact));
    } catch (err) {
      console.error(`[KV] Erro ao salvar estado do usuário ${userId}:`, err);
    }
  }
  memoryFallbackUserStates[userId] = state;
}

async function deleteUserState(env, userId) {
  const { kv } = getKV(env);
  const key = getUserStateKey(userId);
  if (kv) {
    try {
      await kv.delete(key);
    } catch (err) {
      console.error(`[KV] Erro ao deletar estado do usuário ${userId}:`, err);
    }
  }
  delete memoryFallbackUserStates[userId];
}

async function loadState(env) {
  const { kv, name, diag } = getKV(env);
  if (kv) {
    try {
      const data = await kv.get(KV_KEY, { type: 'json' });
      if (data && typeof data === 'object') {
        return { state: data, kvName: name, kvConnected: true, kvDiag: diag };
      }
    } catch (err) {
      console.error(`[KV] Erro ao ler KV (${name}):`, err);
    }
  }
  if (!memoryFallbackState) {
    memoryFallbackState = getDefaultState();
  }
  return { state: memoryFallbackState, kvName: name, kvConnected: !!kv, kvDiag: diag };
}

async function saveState(env, state) {
  const { kv, name } = getKV(env);
  if (kv) {
    try {
      await kv.put(KV_KEY, JSON.stringify(state));
    } catch (err) {
      console.error(`[KV] Erro ao gravar KV (${name}):`, err);
    }
  }
  memoryFallbackState = state;
}

// ------------------- HANDLERS -------------------

export async function onRequestOptions() {
  return new Response(null, { status: 204, headers: CORS_HEADERS });
}

export async function onRequestGet(context) {
  const { request, env } = context;
  const url = new URL(request.url);
  const action = url.searchParams.get('action');
  const userId = url.searchParams.get('userId');

  const { kvName, kvConnected, diag } = getKV(env);
  const usersList = await loadUsersList(env);
  const topPlayer = getTopPlayer(usersList);

  // 1. Rota de Listagem de Perfis
  if (action === 'list_users') {
    return new Response(JSON.stringify({
      users: usersList,
      topPlayer,
      _kv_connected: kvConnected,
      _kv_binding: kvName || 'NONE',
      _kv_diag: diag
    }), {
      status: 200,
      headers: CORS_HEADERS
    });
  }

  // 2. Rota de Estado Individual de Perfil
  if (userId) {
    const { state } = await loadUserState(env, userId);
    const now = Date.now();
    advancePassiveProduction(state, now);

    return new Response(JSON.stringify({
      ...state,
      topPlayer,
      _kv_connected: kvConnected,
      _kv_binding: kvName || 'NONE',
      _kv_diag: diag
    }), {
      status: 200,
      headers: CORS_HEADERS
    });
  }

  // 3. Rota Padrão / Estado Global (ESP8266 & Legado)
  const { state } = await loadState(env);
  const now = Date.now();
  advancePassiveProduction(state, now);

  return new Response(JSON.stringify({
    ...state,
    topPlayer,
    resetOrder: state.resetPendingEsp === true,
    _kv_connected: kvConnected,
    _kv_binding: kvName || 'NONE',
    _kv_diag: diag
  }), {
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

  const url = new URL(request.url);
  const action = body.action || url.searchParams.get('action') || (body.clicks ? 'sync' : '');
  const isEsp = body.source === 'esp';
  const { kvName, kvConnected, diag } = getKV(env);

  const usersList = await loadUsersList(env);
  let topPlayer = getTopPlayer(usersList);

  // -------------------------------------------------------------
  // AÇÃO 1: CRIAR NOVO PERFIL DE USUÁRIO (Salvo imediatamente no KV)
  // -------------------------------------------------------------
  if (action === 'create_user') {
    const rawName = String(body.name || '').trim().replace(/[\r\n\t]/g, '');
    const name = rawName.slice(0, 25) || 'Maker ' + Math.floor(Math.random() * 1000);
    const now = Date.now();
    const userId = 'u_' + now.toString(36) + '_' + Math.random().toString(36).slice(2, 6);

    const initialState = getDefaultState();
    await saveUserState(env, userId, initialState);

    const newUserEntry = {
      id: userId,
      name,
      createdAt: now,
      lastSavedAt: now,
      makitas: 0,
      totalMakitasMade: 0
    };

    usersList.push(newUserEntry);
    await saveUsersList(env, usersList);
    topPlayer = getTopPlayer(usersList);

    return new Response(JSON.stringify({
      success: true,
      user: newUserEntry,
      state: initialState,
      topPlayer,
      _kv_connected: kvConnected,
      _kv_binding: kvName || 'NONE',
      _kv_diag: diag
    }), {
      status: 200,
      headers: CORS_HEADERS
    });
  }

  // -------------------------------------------------------------
  // AÇÃO 2: SALVAR ESTADO DO PERFIL (Save Manual ou Auto-Save)
  // -------------------------------------------------------------
  if (action === 'save_user_state') {
    const userId = body.userId;
    if (!userId) {
      return new Response(JSON.stringify({ error: 'userId obrigatório' }), {
        status: 400,
        headers: CORS_HEADERS
      });
    }

    const now = Date.now();
    const statePayload = body.state || body;

    // Proteção contra consistência eventual / ressurreição pós-reset:
    // Se o estado já salvo no servidor possui um resetEpoch mais recente que o payload enviado,
    // significa que este payload veio de uma requisição/aba anterior ao reset. Rejeitamos para não ressuscitar!
    const { state: currentState } = await loadUserState(env, userId);
    if (currentState && currentState.resetEpoch && (!statePayload.resetEpoch || statePayload.resetEpoch < currentState.resetEpoch)) {
      return new Response(JSON.stringify({
        success: true,
        staleRejected: true,
        state: currentState,
        topPlayer,
        _kv_connected: kvConnected,
        _kv_binding: kvName || 'NONE',
        _kv_diag: 'Save defasado descartado pelo servidor para evitar ressurreição de dados.'
      }), {
        status: 200,
        headers: CORS_HEADERS
      });
    }

    const expanded = expandUserState(statePayload);

    if (typeof statePayload.makitas === 'number') {
      expanded.makitas = statePayload.makitas;
    }
    if (typeof statePayload.totalMakitasMade === 'number') {
      expanded.totalMakitasMade = statePayload.totalMakitasMade;
    }
    expanded.lastSavedAt = now;
    expanded.lastUpdate = now;
    if (currentState && currentState.resetEpoch) {
      expanded.resetEpoch = currentState.resetEpoch;
    }

    await saveUserState(env, userId, expanded);

    // Atualiza resumo no users:list para ranking rápido
    const userIndex = usersList.findIndex(u => u.id === userId);
    if (userIndex >= 0) {
      usersList[userIndex].lastSavedAt = now;
      usersList[userIndex].makitas = expanded.makitas;
      usersList[userIndex].totalMakitasMade = expanded.totalMakitasMade;
      await saveUsersList(env, usersList);
    }
    topPlayer = getTopPlayer(usersList);

    return new Response(JSON.stringify({
      success: true,
      lastSavedAt: now,
      state: expanded,
      topPlayer,
      _kv_connected: kvConnected,
      _kv_binding: kvName || 'NONE',
      _kv_diag: diag
    }), {
      status: 200,
      headers: CORS_HEADERS
    });
  }

  // -------------------------------------------------------------
  // AÇÃO 3: RESETAR ESTADO DE UM PERFIL ESPECÍFICO
  // -------------------------------------------------------------
  if (action === 'reset_user_state') {
    const userId = body.userId;
    if (!userId) {
      return new Response(JSON.stringify({ error: 'userId obrigatório' }), {
        status: 400,
        headers: CORS_HEADERS
      });
    }

    const now = Date.now();
    const freshState = getDefaultState();
    freshState.lastSavedAt = now;
    freshState.lastUpdate = now;
    freshState.resetEpoch = now; // Marca temporal de reset absoluto

    await saveUserState(env, userId, freshState);

    const userIndex = usersList.findIndex(u => u.id === userId);
    if (userIndex >= 0) {
      usersList[userIndex].makitas = 0;
      usersList[userIndex].totalMakitasMade = 0;
      usersList[userIndex].lastSavedAt = now;
      await saveUsersList(env, usersList);
    }
    topPlayer = getTopPlayer(usersList);

    return new Response(JSON.stringify({
      success: true,
      isReset: true,
      state: freshState,
      topPlayer,
      _kv_connected: kvConnected,
      _kv_binding: kvName || 'NONE',
      _kv_diag: diag
    }), {
      status: 200,
      headers: CORS_HEADERS
    });
  }

  // -------------------------------------------------------------
  // AÇÕES ADMINISTRATIVAS (Protegidas por Hash Criptográfico SHA-256)
  // -------------------------------------------------------------
  if (action === 'admin_verify') {
    const authHash = String(body.authHash || '').trim().toLowerCase();
    if (authHash !== ADMIN_AUTH_HASH) {
      return new Response(JSON.stringify({ success: false, error: 'Senha administrativa incorreta.' }), {
        status: 401,
        headers: CORS_HEADERS
      });
    }
    return new Response(JSON.stringify({
      success: true,
      users: usersList,
      topPlayer,
      _kv_connected: kvConnected,
      _kv_binding: kvName || 'NONE',
      _kv_diag: diag
    }), {
      status: 200,
      headers: CORS_HEADERS
    });
  }

  if (action === 'admin_delete_user') {
    const authHash = String(body.authHash || '').trim().toLowerCase();
    if (authHash !== ADMIN_AUTH_HASH) {
      return new Response(JSON.stringify({ success: false, error: 'Não autorizado.' }), {
        status: 401,
        headers: CORS_HEADERS
      });
    }
    const userId = body.userId;
    if (!userId) {
      return new Response(JSON.stringify({ error: 'userId obrigatório' }), {
        status: 400,
        headers: CORS_HEADERS
      });
    }

    await deleteUserState(env, userId);
    const updatedUsers = usersList.filter(u => u.id !== userId);
    await saveUsersList(env, updatedUsers);
    topPlayer = getTopPlayer(updatedUsers);

    return new Response(JSON.stringify({
      success: true,
      deletedUserId: userId,
      users: updatedUsers,
      topPlayer,
      _kv_connected: kvConnected,
      _kv_binding: kvName || 'NONE',
      _kv_diag: diag
    }), {
      status: 200,
      headers: CORS_HEADERS
    });
  }

  if (action === 'admin_delete_all_users') {
    const authHash = String(body.authHash || '').trim().toLowerCase();
    if (authHash !== ADMIN_AUTH_HASH) {
      return new Response(JSON.stringify({ success: false, error: 'Não autorizado.' }), {
        status: 401,
        headers: CORS_HEADERS
      });
    }

    for (const u of usersList) {
      await deleteUserState(env, u.id);
    }
    await saveUsersList(env, []);
    topPlayer = { name: 'MakerSpace', makitas: 0, totalMakitasMade: 0 };

    return new Response(JSON.stringify({
      success: true,
      users: [],
      topPlayer,
      _kv_connected: kvConnected,
      _kv_binding: kvName || 'NONE',
      _kv_diag: diag
    }), {
      status: 200,
      headers: CORS_HEADERS
    });
  }

  if (action === 'admin_reset_hardware') {
    const authHash = String(body.authHash || '').trim().toLowerCase();
    if (authHash !== ADMIN_AUTH_HASH) {
      return new Response(JSON.stringify({ success: false, error: 'Não autorizado.' }), {
        status: 401,
        headers: CORS_HEADERS
      });
    }

    const resetId = Date.now();
    const newState = getDefaultState();
    newState.resetId = resetId;
    newState.resetPendingEsp = true; // Flag latente: persiste até confirmação da ESP via ACK
    newState.lastUpdate = resetId;
    newState.lastKvSave = resetId;
    await saveState(env, newState);

    return new Response(JSON.stringify({
      success: true,
      ...newState,
      topPlayer,
      isReset: true,
      resetOrder: true,
      _kv_connected: kvConnected,
      _kv_binding: kvName || 'NONE',
      _kv_diag: diag
    }), {
      status: 200,
      headers: CORS_HEADERS
    });
  }

  // -------------------------------------------------------------
  // AÇÃO 3: FLUXO GLOBAL / HARDWARE ESP8266 & RESET
  // -------------------------------------------------------------
  const { state } = await loadState(env);
  const now = Date.now();
  const previousMakitas = state.makitas || 0;
  advancePassiveProduction(state, now);

  let clientMakitas = typeof body.makitas === 'number' ? body.makitas : null;
  let clientTotal = typeof body.totalMakitasMade === 'number' ? body.totalMakitasMade : null;
  let clicks = Math.max(0, Math.min(parseInt(body.clicks || body.count || 0, 10), 5000));

  // Reset total disparado pelo website: emite ordem latente para a ESP
  if (action === 'reset') {
    const resetId = Date.now();
    const newState = getDefaultState();
    newState.resetId = resetId;
    newState.resetPendingEsp = true; // Emite ordem contínua e latente para a ESP
    newState.lastUpdate = resetId;
    newState.lastKvSave = resetId;
    await saveState(env, newState);
    return new Response(JSON.stringify({
      ...newState,
      topPlayer,
      isReset: true,
      resetOrder: true,
      _kv_connected: kvConnected,
      _kv_binding: kvName || 'NONE',
      _kv_diag: diag
    }), {
      status: 200,
      headers: CORS_HEADERS
    });
  }

  // TRATAMENTO DA ORDEM DE RESET LATENTE:
  // A ordem de reset para a ESP permanece ativa no KV até a ESP executar a limpeza e enviar resetAck: true.
  let espAckReceived = false;
  if (state.resetPendingEsp) {
    if (isEsp && body.resetAck === true) {
      state.resetPendingEsp = false;
      state.lastResetAckAt = now;
      espAckReceived = true;
      // Garante que o estado permaneça zerado e limpo
      state.makitas = 0.0;
      state.totalMakitasMade = 0.0;
      state.owned = {};
      state.perms = {};
      state.mps = 0.0;
      state.clickPower = 1.0;
    } else {
      // Enquanto a ESP não executar o reset e enviar o ACK, rejeita qualquer dado antigo
      clientMakitas = null;
      clientTotal = null;
      clicks = 0;
      body.owned = null;
      body.perms = null;
      state.makitas = 0.0;
      state.totalMakitasMade = 0.0;
    }
  }

  // Telemetria reportada pela ESP8266
  if (isEsp) {
    state.espTelemetry = {
      lastPing: now,
      fwVersion: typeof body.fwVersion === 'number' ? body.fwVersion : (state.espTelemetry?.fwVersion || 0),
      ip: typeof body.ip === 'string' ? body.ip : (state.espTelemetry?.ip || 'desconhecido'),
      rssi: typeof body.rssi === 'number' ? body.rssi : (state.espTelemetry?.rssi || null),
      uptime: typeof body.uptime === 'number' ? body.uptime : (state.espTelemetry?.uptime || 0),
      freeHeap: typeof body.freeHeap === 'number' ? body.freeHeap : (state.espTelemetry?.freeHeap || 0)
    };
  }

  // RECONCILIAÇÃO MONOTÔNICA E CONVERGÊNCIA (CRDT / RATCHET):
  // 1. Saldo e Total: O saldo só pode AUMENTAR. Nunca adota um saldo menor no sync.
  if (clientMakitas !== null) {
    if (clientMakitas > state.makitas) {
      state.makitas = clientMakitas;
    }
    if (clientTotal && clientTotal > (state.totalMakitasMade || 0)) {
      state.totalMakitasMade = clientTotal;
    } else if (state.makitas > (state.totalMakitasMade || 0)) {
      state.totalMakitasMade = state.makitas;
    }
  }

  // Processa cliques pendentes enviados
  if (clicks > 0) {
    const gainPerClick = getSingleClickGain(state.perms, state.mps);
    const totalGain = gainPerClick * clicks;
    state.makitas += totalGain;
    state.totalMakitasMade = (state.totalMakitasMade || 0) + totalGain;
  }

  // 2. Upgrades da Loja: Convergência aditiva — sempre mantém o MAIOR nível de cada oficina
  if (body.owned && typeof body.owned === 'object') {
    state.owned = state.owned || {};
    for (const u of UPGRADES) {
      if (typeof body.owned[u.id] === 'number') {
        state.owned[u.id] = Math.max(state.owned[u.id] || 0, Math.min(MAX_OWNED, body.owned[u.id]));
      }
    }
  }

  // 3. Tecnologias Permanentes: Se qualquer nó ativou uma tecnologia, ela permanece ativa
  if (body.perms && typeof body.perms === 'object') {
    state.perms = state.perms || {};
    for (const p of PERMANENT_UPGRADES) {
      if (body.perms[p.id] === true) {
        state.perms[p.id] = true;
      }
    }
  }

  state.mps = calculateMps(state.owned, state.perms);
  state.clickPower = calculateClickPower(state.perms);

  // AÇÕES DE COMPRA (Web Master para Upgrades):
  if (action === 'buy') {
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
      state.mps = calculateMps(state.owned, state.perms);
      state.clickPower = calculateClickPower(state.perms);
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
        state.mps = calculateMps(state.owned, state.perms);
        state.clickPower = calculateClickPower(state.perms);
      }
    }
  }

  state.totalOwned = getTotalOwned(state.owned);
  state.lastUpdate = now;

  // COTA INTELIGENTE DE ESCRITA NO KV (1.000 writes/dia no plano gratuito):
  // Grava imediatamente em compras, reset, cliques ou novos saldos.
  // Em syncs periódicos sem cliques, grava a cada 60 segundos como checkpoint.
  const hasStateChanged = 
    action === 'buy' || 
    action === 'perm_buy' || 
    action === 'reset' ||
    espAckReceived ||
    clicks > 0 ||
    Math.abs(state.makitas - previousMakitas) > 5.0 ||
    (now - (state.lastKvSave || 0) >= 60000);

  if (hasStateChanged) {
    state.lastKvSave = now;
    await saveState(env, state);
  }

  return new Response(JSON.stringify({
    ...state,
    topPlayer,
    resetOrder: state.resetPendingEsp === true,
    _kv_connected: kvConnected,
    _kv_binding: kvName || 'NONE',
    _kv_diag: diag
  }), {
    status: 200,
    headers: CORS_HEADERS
  });
}
