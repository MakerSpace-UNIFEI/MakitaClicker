// =====================================================================
// MAKITA CLICKER - PAINEL ADMINISTRATIVO (ADMIN.JS)
// Autenticação Criptográfica com Hash SHA-256 (Senha Segura One-Way)
// =====================================================================

// Hash SHA-256 da senha 'ADMMAKER' (irreversível, impossível descriptografar)
const ADMIN_EXPECTED_HASH = 'c9a2abd67ad59717195e5d8a6f917ba5084d81af244b0a8d40c8b30f234742d7';

let currentAuthHash = sessionStorage.getItem('makita_admin_hash') || null;

// Elementos do DOM
const loginViewEl = document.getElementById('adminLoginView');
const dashboardViewEl = document.getElementById('adminDashboardView');
const loginFormEl = document.getElementById('adminLoginForm');
const passwordInputEl = document.getElementById('adminPasswordInput');
const loginErrorEl = document.getElementById('adminLoginError');

const totalUsersEl = document.getElementById('adminTotalUsers');
const topPlayerEl = document.getElementById('adminTopPlayer');
const kvStatusEl = document.getElementById('adminKvStatus');
const usersTableBodyEl = document.getElementById('adminUsersTableBody');

const btnRefreshEl = document.getElementById('btnAdminRefresh');
const btnDeleteAllEl = document.getElementById('btnAdminDeleteAll');
const btnResetGlobalEl = document.getElementById('btnAdminResetGlobal');

// Função criptográfica SHA-256 nativa do navegador
async function hashPassword(str) {
    const buffer = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(str));
    return Array.from(new Uint8Array(buffer))
        .map(b => b.toString(16).padStart(2, '0'))
        .join('');
}

function formatCompact(num) {
    const n = Number(num) || 0;
    if (n >= 1e12) return (n / 1e12).toFixed(2) + 'T';
    if (n >= 1e9)  return (n / 1e9).toFixed(2) + 'B';
    if (n >= 1e6)  return (n / 1e6).toFixed(2) + 'M';
    if (n >= 1e3)  return (n / 1e3).toFixed(1) + 'k';
    return Math.floor(n).toLocaleString('pt-BR');
}

function formatDate(ts) {
    if (!ts) return '—';
    const d = new Date(ts);
    return d.toLocaleDateString('pt-BR') + ' ' + d.toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit' });
}

async function verifyAndLoad(hash) {
    if (!hash) return;
    try {
        const res = await fetch('/api/state', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'admin_verify', authHash: hash })
        });
        if (!res.ok) throw new Error('Autenticação inválida');
        const data = await res.json();
        if (data.success) {
            currentAuthHash = hash;
            sessionStorage.setItem('makita_admin_hash', hash);
            showDashboard(data);
        } else {
            throw new Error(data.error || 'Falha ao autenticar');
        }
    } catch (err) {
        sessionStorage.removeItem('makita_admin_hash');
        currentAuthHash = null;
        if (loginErrorEl) loginErrorEl.textContent = 'Acesso negado: senha incorreta.';
    }
}

function showDashboard(data) {
    if (loginViewEl) loginViewEl.style.display = 'none';
    if (dashboardViewEl) dashboardViewEl.style.display = 'block';

    updateDashboardData(data.users || [], data.topPlayer, data._kv_binding);
}

function updateDashboardData(users, topPlayer, kvBinding) {
    if (totalUsersEl) totalUsersEl.textContent = users.length;
    if (topPlayerEl) {
        topPlayerEl.textContent = topPlayer ? `${topPlayer.name} (${formatCompact(topPlayer.totalMakitasMade || topPlayer.makitas)})` : 'Nenhum';
    }
    if (kvStatusEl) {
        kvStatusEl.textContent = kvBinding ? `KV Ativo (${kvBinding})` : 'Cloudflare KV';
        kvStatusEl.style.color = 'var(--green)';
    }

    renderTable(users);
}

function renderTable(users) {
    if (!usersTableBodyEl) return;
    if (!users || users.length === 0) {
        usersTableBodyEl.innerHTML = `
            <tr>
                <td colspan="8" style="text-align: center; color: var(--text-lo); padding: 2.5rem;">
                    Nenhum perfil cadastrado no banco de dados.
                </td>
            </tr>
        `;
        return;
    }

    // Ordena do maior para o menor total de Makitas
    users.sort((a, b) => (b.totalMakitasMade || b.makitas || 0) - (a.totalMakitasMade || a.makitas || 0));

    usersTableBodyEl.innerHTML = '';
    users.forEach((u, idx) => {
        const tr = document.createElement('tr');
        const medal = idx === 0 ? '🥇' : (idx === 1 ? '🥈' : (idx === 2 ? '🥉' : `#${idx + 1}`));
        
        tr.innerHTML = `
            <td style="font-weight: 700;">${medal}</td>
            <td style="font-weight: 700; color: var(--teal-hi);">${escapeHtml(u.name || 'Sem nome')}</td>
            <td style="font-family: var(--mono); font-size: 0.8rem; color: var(--text-lo);">${escapeHtml(u.id)}</td>
            <td style="font-family: var(--mono); color: var(--orange);">${formatCompact(u.makitas)}</td>
            <td style="font-family: var(--mono);">${formatCompact(u.totalMakitasMade || u.makitas)}</td>
            <td style="font-size: 0.8rem; color: var(--text-lo);">${formatDate(u.createdAt)}</td>
            <td style="font-size: 0.8rem; color: var(--text-lo);">${formatDate(u.lastSavedAt)}</td>
            <td style="text-align: right;">
                <button class="admin-del-btn" data-id="${escapeHtml(u.id)}" data-name="${escapeHtml(u.name || 'Usuário')}">🗑️ Apagar</button>
            </td>
        `;
        usersTableBodyEl.appendChild(tr);
    });

    // Eventos dos botões de exclusão individual
    usersTableBodyEl.querySelectorAll('.admin-del-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const userId = btn.dataset.id;
            const userName = btn.dataset.name;
            deleteSingleUser(userId, userName);
        });
    });
}

function escapeHtml(str) {
    return String(str || '').replace(/[&<>"']/g, m => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[m]));
}

async function refreshList() {
    if (!currentAuthHash) return;
    if (btnRefreshEl) {
        btnRefreshEl.disabled = true;
        btnRefreshEl.textContent = '🔄 Atualizando...';
    }
    try {
        const res = await fetch('/api/state', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'admin_verify', authHash: currentAuthHash })
        });
        const data = await res.json();
        if (data.success) {
            updateDashboardData(data.users || [], data.topPlayer, data._kv_binding);
        }
    } catch (e) {
        alert('Erro ao atualizar dados: ' + e.message);
    } finally {
        if (btnRefreshEl) {
            btnRefreshEl.disabled = false;
            btnRefreshEl.textContent = '🔄 Atualizar Lista';
        }
    }
}

async function deleteSingleUser(userId, userName) {
    if (!confirm(`Tem certeza que deseja apagar o perfil "${userName}" (${userId})?\nEssa ação é irreversível!`)) {
        return;
    }

    try {
        const res = await fetch('/api/state', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                action: 'admin_delete_user',
                authHash: currentAuthHash,
                userId
            })
        });
        const data = await res.json();
        if (data.success) {
            updateDashboardData(data.users || [], data.topPlayer, data._kv_binding);
        } else {
            alert('Falha ao deletar perfil: ' + (data.error || 'Erro desconhecido'));
        }
    } catch (e) {
        alert('Erro de conexão ao deletar perfil: ' + e.message);
    }
}

async function deleteAllUsers() {
    const confirmation = prompt("ATENÇÃO: Você está prestes a apagar TODOS os perfis do Cloudflare KV!\nDigite 'CONFIRMAR' para prosseguir:");
    if (confirmation !== 'CONFIRMAR') {
        alert('Ação cancelada.');
        return;
    }

    if (btnDeleteAllEl) {
        btnDeleteAllEl.disabled = true;
        btnDeleteAllEl.textContent = '⏳ Apagando tudo...';
    }

    try {
        const res = await fetch('/api/state', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                action: 'admin_delete_all_users',
                authHash: currentAuthHash
            })
        });
        const data = await res.json();
        if (data.success) {
            alert('Todos os perfis foram removidos com sucesso do KV!');
            updateDashboardData([], data.topPlayer, data._kv_binding);
        } else {
            alert('Falha ao apagar perfis: ' + (data.error || 'Erro'));
        }
    } catch (e) {
        alert('Erro na requisição: ' + e.message);
    } finally {
        if (btnDeleteAllEl) {
            btnDeleteAllEl.disabled = false;
            btnDeleteAllEl.textContent = '⚠️ Apagar TODOS os Perfis';
        }
    }
}

async function resetGlobalHardware() {
    if (!confirm('Deseja emitir ordem de reset global para o hardware ESP8266?\nO saldo mestre físico será zerado na próxima sincronização.')) {
        return;
    }

    if (btnResetGlobalEl) {
        btnResetGlobalEl.disabled = true;
        btnResetGlobalEl.textContent = '⏳ Emitindo ordem...';
    }

    try {
        const res = await fetch('/api/state', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'admin_reset_hardware', authHash: currentAuthHash })
        });
        const data = await res.json();
        if (data.isReset || data.success) {
            alert('Ordem de reset latente emitida com sucesso! A ESP8266 física apagará sua memória e responderá com ACK.');
        }
    } catch (e) {
        alert('Erro ao emitir ordem: ' + e.message);
    } finally {
        if (btnResetGlobalEl) {
            btnResetGlobalEl.disabled = false;
            btnResetGlobalEl.textContent = '⚠️ Emitir Ordem de Reset Global para ESP8266';
        }
    }
}

// Configuração dos ouvintes de eventos
if (loginFormEl) {
    loginFormEl.addEventListener('submit', async (e) => {
        e.preventDefault();
        if (loginErrorEl) loginErrorEl.textContent = '';
        const rawPassword = passwordInputEl.value;
        if (!rawPassword) return;

        const hashed = await hashPassword(rawPassword);
        if (hashed !== ADMIN_EXPECTED_HASH) {
            if (loginErrorEl) loginErrorEl.textContent = 'Senha incorreta. Acesso não permitido.';
            return;
        }

        await verifyAndLoad(hashed);
    });
}

if (btnRefreshEl) btnRefreshEl.addEventListener('click', refreshList);
if (btnDeleteAllEl) btnDeleteAllEl.addEventListener('click', deleteAllUsers);
if (btnResetGlobalEl) btnResetGlobalEl.addEventListener('click', resetGlobalHardware);

// Auto-login se houver sessão ativa
if (currentAuthHash) {
    verifyAndLoad(currentAuthHash);
}
