document.addEventListener('DOMContentLoaded', () => {
    // ==========================================
    // State & Auth Variables
    // ==========================================
    let currentUser = null;
    let authToken = localStorage.getItem('pastebin_token') || '';
    let activeKey = '';
    let currentRawContent = '';
    let pendingEncryptedContent = '';

    // DOM Navigation
    const tabBtns = document.querySelectorAll('.tab-btn');
    const viewPanels = document.querySelectorAll('.view-panel');

    // Header Auth Elements
    const authGuestBox = document.getElementById('auth-guest-box');
    const authUserBox = document.getElementById('auth-user-box');
    const headerUsername = document.getElementById('header-username');
    const headerUserInitial = document.getElementById('header-user-initial');
    const btnOpenAuthModal = document.getElementById('btn-open-auth-modal');
    const btnLogout = document.getElementById('btn-logout');

    // Auth Modal Elements
    const authModal = document.getElementById('auth-modal');
    const btnCloseAuthModal = document.getElementById('btn-close-auth-modal');
    const authTabLogin = document.getElementById('auth-tab-login');
    const authTabRegister = document.getElementById('auth-tab-register');
    const authForm = document.getElementById('auth-form');
    const authUsernameInput = document.getElementById('auth-username');
    const authPasswordInput = document.getElementById('auth-password');
    const btnAuthSubmit = document.getElementById('btn-auth-submit');
    const authErrorMsg = document.getElementById('auth-error-msg');
    let isRegisterMode = false;

    // Create View Elements
    const pasteTitleInput = document.getElementById('paste-title-input');
    const pasteInput = document.getElementById('paste-input');
    const lineNumbers = document.getElementById('line-numbers');
    const statLines = document.getElementById('stat-lines');
    const statChars = document.getElementById('stat-chars');
    const statSize = document.getElementById('stat-size');
    const btnCreatePaste = document.getElementById('btn-create-paste');
    const btnClear = document.getElementById('btn-clear');
    const radioPrivate = document.getElementById('radio-private');
    const pillPrivate = document.getElementById('pill-private');
    const privateRadioWrapper = document.getElementById('private-radio-wrapper');
    const privateGuestHint = document.getElementById('private-guest-hint');
    const chkEncrypt = document.getElementById('chk-encrypt');
    const passphraseWrapper = document.getElementById('passphrase-wrapper');
    const pastePassphrase = document.getElementById('paste-passphrase');
    const createSuccessCard = document.getElementById('create-success-card');
    const successPrivacyDesc = document.getElementById('success-privacy-desc');
    const resultKey = document.getElementById('result-key');
    const resultBadges = document.getElementById('result-badges');
    const btnCopyKey = document.getElementById('btn-copy-key');
    const btnCopyLink = document.getElementById('btn-copy-link');
    const btnViewPaste = document.getElementById('btn-view-paste');

    // Lookup View Elements
    const lookupForm = document.getElementById('lookup-form');
    const lookupKeyInput = document.getElementById('lookup-key-input');
    const decryptPromptCard = document.getElementById('decrypt-prompt-card');
    const decryptForm = document.getElementById('decrypt-form');
    const decryptPassphraseInput = document.getElementById('decrypt-passphrase-input');
    const accessDeniedCard = document.getElementById('access-denied-card');
    const btnDeniedLogin = document.getElementById('btn-denied-login');
    const pasteDisplayContainer = document.getElementById('paste-display-container');
    const displayKeyTag = document.getElementById('display-key-tag');
    const displayTitleTag = document.getElementById('display-title-tag');
    const displayBadgeVisibility = document.getElementById('display-badge-visibility');
    const displayBadgeEncrypted = document.getElementById('display-badge-encrypted');
    const displayStatLines = document.getElementById('display-stat-lines');
    const displayStatSize = document.getElementById('display-stat-size');
    const displayContent = document.getElementById('display-content');
    const viewerLineNumbers = document.getElementById('viewer-line-numbers');
    const btnCopyContent = document.getElementById('btn-copy-content');
    const btnDownload = document.getElementById('btn-download');
    const linkRawView = document.getElementById('link-raw-view');

    // My Pastes View Elements
    const userPasteCount = document.getElementById('user-paste-count');
    const mypastesLoggedOutPrompt = document.getElementById('mypastes-logged-out-prompt');
    const userPastesGrid = document.getElementById('user-pastes-grid');
    const btnRefreshUserPastes = document.getElementById('btn-refresh-user-pastes');
    const btnPromptLogin = document.getElementById('btn-prompt-login');

    // Toast Container
    const toastContainer = document.getElementById('toast-container');

    // ==========================================
    // Tab Navigation
    // ==========================================
    function switchTab(tabId) {
        tabBtns.forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tab === tabId);
        });

        viewPanels.forEach(panel => {
            panel.classList.toggle('active', panel.id === tabId);
        });

        if (tabId === 'mypastes-view') {
            loadUserPastes();
        }
    }

    tabBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            switchTab(btn.dataset.tab);
        });
    });

    // ==========================================
    // Client-Side AES-256-GCM Cryptography (Web Crypto API)
    // ==========================================
    async function deriveKey(passphrase, saltBytes) {
        const enc = new TextEncoder();
        const keyMaterial = await window.crypto.subtle.importKey(
            'raw',
            enc.encode(passphrase),
            { name: 'PBKDF2' },
            false,
            ['deriveKey']
        );

        return window.crypto.subtle.deriveKey(
            {
                name: 'PBKDF2',
                salt: saltBytes,
                iterations: 100000,
                hash: 'SHA-256'
            },
            keyMaterial,
            { name: 'AES-GCM', length: 256 },
            false,
            ['encrypt', 'decrypt']
        );
    }

    function buf2hex(buffer) {
        return Array.from(new Uint8Array(buffer))
            .map(b => b.toString(16).padStart(2, '0'))
            .join('');
    }

    function hex2buf(hexString) {
        const bytes = new Uint8Array(hexString.length / 2);
        for (let i = 0; i < hexString.length; i += 2) {
            bytes[i / 2] = parseInt(hexString.substr(i, 2), 16);
        }
        return bytes;
    }

    async function encryptText(plaintext, passphrase) {
        const enc = new TextEncoder();
        const salt = window.crypto.getRandomValues(new Uint8Array(16));
        const iv = window.crypto.getRandomValues(new Uint8Array(12));
        const key = await deriveKey(passphrase, salt);

        const ciphertextBuffer = await window.crypto.subtle.encrypt(
            { name: 'AES-GCM', iv: iv },
            key,
            enc.encode(plaintext)
        );

        return `ENCRYPTED:v1:${buf2hex(salt)}:${buf2hex(iv)}:${buf2hex(ciphertextBuffer)}`;
    }

    async function decryptText(encryptedString, passphrase) {
        if (!encryptedString.startsWith('ENCRYPTED:v1:')) {
            throw new Error('Not a valid encrypted paste format');
        }

        const parts = encryptedString.split(':');
        if (parts.length !== 5) {
            throw new Error('Malformed encrypted payload');
        }

        const salt = hex2buf(parts[2]);
        const iv = hex2buf(parts[3]);
        const ciphertext = hex2buf(parts[4]);

        const key = await deriveKey(passphrase, salt);
        const decryptedBuffer = await window.crypto.subtle.decrypt(
            { name: 'AES-GCM', iv: iv },
            key,
            ciphertext
        );

        const dec = new TextDecoder();
        return dec.decode(decryptedBuffer);
    }

    // ==========================================
    // Authentication & Session Management
    // ==========================================
    async function authFetch(url, options = {}) {
        options.headers = options.headers || {};
        if (authToken) {
            options.headers['Authorization'] = `Bearer ${authToken}`;
        }
        return fetch(url, options);
    }

    async function checkCurrentUser() {
        if (!authToken) {
            updateAuthUI(null);
            return;
        }

        try {
            const resp = await authFetch('/api/auth/me');
            if (resp.ok) {
                const user = await resp.json();
                currentUser = user;
                updateAuthUI(user);
                loadUserPastes();
            } else {
                // Token expired or invalid
                authToken = '';
                localStorage.removeItem('pastebin_token');
                updateAuthUI(null);
            }
        } catch {
            updateAuthUI(null);
        }
    }

    function updateAuthUI(user) {
        if (user) {
            authGuestBox.classList.add('hidden');
            authUserBox.classList.remove('hidden');
            headerUsername.textContent = user.username;
            headerUserInitial.textContent = user.username.charAt(0).toUpperCase();

            // Enable private option
            radioPrivate.disabled = false;
            pillPrivate.style.opacity = '1';
            pillPrivate.style.cursor = 'pointer';
            privateGuestHint.classList.add('hidden');
            privateRadioWrapper.classList.remove('disabled');
        } else {
            authGuestBox.classList.remove('hidden');
            authUserBox.classList.add('hidden');

            // Disable private option for guests
            document.querySelector('input[name="paste-visibility"][value="public"]').checked = true;
            radioPrivate.disabled = true;
            pillPrivate.style.opacity = '0.5';
            pillPrivate.style.cursor = 'not-allowed';
            privateGuestHint.classList.remove('hidden');
            privateRadioWrapper.classList.add('disabled');
        }
    }

    // Auth Modal Handlers
    function openAuthModal(registerMode = false) {
        isRegisterMode = registerMode;
        authErrorMsg.classList.add('hidden');
        authForm.reset();

        if (isRegisterMode) {
            authTabRegister.classList.add('active');
            authTabLogin.classList.remove('active');
            btnAuthSubmit.textContent = 'Create Account';
        } else {
            authTabLogin.classList.add('active');
            authTabRegister.classList.remove('active');
            btnAuthSubmit.textContent = 'Log In';
        }

        authModal.classList.remove('hidden');
        authUsernameInput.focus();
    }

    function closeAuthModal() {
        authModal.classList.add('hidden');
    }

    btnOpenAuthModal.addEventListener('click', () => openAuthModal(false));
    btnCloseAuthModal.addEventListener('click', closeAuthModal);
    btnDeniedLogin.addEventListener('click', () => openAuthModal(false));
    btnPromptLogin.addEventListener('click', () => openAuthModal(false));

    authTabLogin.addEventListener('click', () => openAuthModal(false));
    authTabRegister.addEventListener('click', () => openAuthModal(true));

    authForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const username = authUsernameInput.value.trim();
        const password = authPasswordInput.value;

        if (!username || !password) return;

        btnAuthSubmit.disabled = true;
        authErrorMsg.classList.add('hidden');

        const endpoint = isRegisterMode ? '/api/auth/register' : '/api/auth/login';

        try {
            const resp = await fetch(endpoint, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username, password })
            });

            const data = await resp.json();

            if (!resp.ok) {
                throw new Error(data.error || 'Authentication failed');
            }

            authToken = data.token;
            localStorage.setItem('pastebin_token', authToken);
            currentUser = data.user;
            updateAuthUI(currentUser);
            closeAuthModal();

            showToast(isRegisterMode ? `Account created! Welcome, ${username}` : `Logged in as ${username}`, 'success');

            // If we were on access denied, re-try fetch
            if (activeKey && !accessDeniedCard.classList.contains('hidden')) {
                lookupKey(activeKey);
            }
        } catch (err) {
            authErrorMsg.textContent = err.message;
            authErrorMsg.classList.remove('hidden');
        } finally {
            btnAuthSubmit.disabled = false;
        }
    });

    btnLogout.addEventListener('click', async () => {
        if (authToken) {
            try {
                await authFetch('/api/auth/logout', { method: 'POST' });
            } catch {
                // Ignore network error on logout
            }
        }
        authToken = '';
        currentUser = null;
        localStorage.removeItem('pastebin_token');
        updateAuthUI(null);
        showToast('Logged out successfully', 'info');
        if (!viewPanels[2].classList.contains('active')) {
            // Re-render
        } else {
            loadUserPastes();
        }
    });

    // ==========================================
    // Editor, Stats & Encryption Controls
    // ==========================================
    function updateEditorStats() {
        const text = pasteInput.value;
        const lines = text.split('\n');
        const lineCount = lines.length;

        lineNumbers.innerText = Array.from({ length: lineCount }, (_, i) => i + 1).join('\n');

        const charCount = text.length;
        const byteCount = new Blob([text]).size;

        statLines.innerText = `${lineCount} ${lineCount === 1 ? 'line' : 'lines'}`;
        statChars.innerText = `${charCount} ${charCount === 1 ? 'char' : 'chars'}`;
        statSize.innerText = formatBytes(byteCount);
    }

    function formatBytes(bytes) {
        if (bytes < 1024) return bytes + ' B';
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
        return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
    }

    pasteInput.addEventListener('input', updateEditorStats);
    pasteInput.addEventListener('scroll', () => {
        lineNumbers.scrollTop = pasteInput.scrollTop;
    });

    pasteInput.addEventListener('keydown', (e) => {
        if (e.key === 'Tab') {
            e.preventDefault();
            const start = pasteInput.selectionStart;
            const end = pasteInput.selectionEnd;
            pasteInput.value = pasteInput.value.substring(0, start) + '    ' + pasteInput.value.substring(end);
            pasteInput.selectionStart = pasteInput.selectionEnd = start + 4;
            updateEditorStats();
        } else if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) {
            e.preventDefault();
            btnCreatePaste.click();
        }
    });

    chkEncrypt.addEventListener('change', () => {
        if (chkEncrypt.checked) {
            passphraseWrapper.classList.remove('hidden');
            pastePassphrase.focus();
        } else {
            passphraseWrapper.classList.add('hidden');
        }
    });

    btnClear.addEventListener('click', () => {
        pasteInput.value = '';
        pasteTitleInput.value = '';
        updateEditorStats();
        createSuccessCard.classList.add('hidden');
        pasteInput.focus();
    });

    // ==========================================
    // API: Create / Publish Paste
    // ==========================================
    btnCreatePaste.addEventListener('click', async () => {
        const rawContent = pasteInput.value;
        if (!rawContent.trim()) {
            showToast('Please enter some text before creating a paste', 'error');
            pasteInput.focus();
            return;
        }

        const isEncrypt = chkEncrypt.checked;
        const passphrase = pastePassphrase.value;

        if (isEncrypt && !passphrase) {
            showToast('Please enter a decryption passphrase or uncheck encryption', 'error');
            pastePassphrase.focus();
            return;
        }

        const visibility = document.querySelector('input[name="paste-visibility"]:checked').value;
        const isPublic = (visibility === 'public');
        const title = pasteTitleInput.value.trim();

        btnCreatePaste.disabled = true;
        btnCreatePaste.innerHTML = `<span class="spinner"></span> Encrypting & Publishing...`;

        try {
            let finalContent = rawContent;
            if (isEncrypt) {
                finalContent = await encryptText(rawContent, passphrase);
            }

            const payload = {
                title: title,
                content: finalContent,
                is_public: isPublic,
                is_encrypted: isEncrypt
            };

            const response = await authFetch('/api/pastes', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });

            if (!response.ok) {
                const errData = await response.json().catch(() => ({}));
                throw new Error(errData.error || `Server returned ${response.status}`);
            }

            const data = await response.json();
            const key = data.key;
            activeKey = key;

            // Update result card
            resultKey.innerText = key;
            resultBadges.innerHTML = `
                <span class="badge-pill ${isPublic ? 'badge-public' : 'badge-private'}">${isPublic ? '🌐 Public' : '🔒 Private'}</span>
                ${isEncrypt ? '<span class="badge-pill badge-encrypted">🔑 Encrypted</span>' : ''}
            `;

            successPrivacyDesc.textContent = isPublic
                ? 'Public paste viewable by anyone with the link.'
                : 'Private paste restricted exclusively to your account.';

            createSuccessCard.classList.remove('hidden');
            createSuccessCard.scrollIntoView({ behavior: 'smooth', block: 'nearest' });

            showToast(`Paste published with key: ${key}`, 'success');
            loadUserPastes();
        } catch (err) {
            console.error('Failed to create paste:', err);
            showToast(`Error: ${err.message}`, 'error');
        } finally {
            btnCreatePaste.disabled = false;
            btnCreatePaste.innerHTML = `
                <svg viewBox="0 0 24 24" width="16" height="16" stroke="currentColor" stroke-width="2" fill="none"><line x1="22" y1="2" x2="11" y2="13"></line><polygon points="22 2 15 22 11 13 2 9 22 2"></polygon></svg>
                Publish Paste
            `;
        }
    });

    btnCopyKey.addEventListener('click', () => {
        if (activeKey) {
            navigator.clipboard.writeText(activeKey);
            showToast('Key copied to clipboard!', 'success');
        }
    });

    btnCopyLink.addEventListener('click', () => {
        if (activeKey) {
            const shareUrl = `${window.location.origin}/#${activeKey}`;
            navigator.clipboard.writeText(shareUrl);
            showToast('Share link copied to clipboard!', 'success');
        }
    });

    btnViewPaste.addEventListener('click', () => {
        if (activeKey) {
            lookupKey(activeKey);
        }
    });

    // ==========================================
    // API: Lookup & Decrypt Paste
    // ==========================================
    lookupForm.addEventListener('submit', (e) => {
        e.preventDefault();
        const key = lookupKeyInput.value.trim();
        if (key) {
            lookupKey(key);
        }
    });

    async function lookupKey(key) {
        switchTab('retrieve-view');
        lookupKeyInput.value = key;
        window.location.hash = key;

        // Reset views
        accessDeniedCard.classList.add('hidden');
        decryptPromptCard.classList.add('hidden');
        pasteDisplayContainer.classList.add('hidden');

        try {
            const response = await authFetch(`/api/pastes/${encodeURIComponent(key)}`, {
                headers: { 'Accept': 'application/json' }
            });

            if (response.status === 403) {
                accessDeniedCard.classList.remove('hidden');
                activeKey = key;
                return;
            }

            if (response.status === 404) {
                showToast(`Paste "${key}" not found`, 'error');
                return;
            }

            if (!response.ok) {
                throw new Error(`Failed to load paste: ${response.status}`);
            }

            const data = await response.json();
            activeKey = key;

            const isEncrypted = data.is_encrypted || (data.content && data.content.startsWith('ENCRYPTED:v1:'));
            displayTitleTag.textContent = data.title ? data.title : '';

            // Set Badges
            displayBadgeVisibility.className = `badge-pill ${data.is_public ? 'badge-public' : 'badge-private'}`;
            displayBadgeVisibility.textContent = data.is_public ? '🌐 Public' : '🔒 Private';

            if (isEncrypted) {
                displayBadgeEncrypted.classList.remove('hidden');
                pendingEncryptedContent = data.content;
                decryptPromptCard.classList.remove('hidden');
                decryptPassphraseInput.value = '';
                decryptPassphraseInput.focus();
            } else {
                displayBadgeEncrypted.classList.add('hidden');
                renderPasteContent(key, data.content, data);
            }
        } catch (err) {
            console.error('Error fetching paste:', err);
            showToast(`Error: ${err.message}`, 'error');
        }
    }

    // Decrypt Form Submission
    decryptForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const passphrase = decryptPassphraseInput.value;
        if (!passphrase) return;

        try {
            const plaintext = await decryptText(pendingEncryptedContent, passphrase);
            decryptPromptCard.classList.add('hidden');
            renderPasteContent(activeKey, plaintext, { title: displayTitleTag.textContent, is_public: true, is_encrypted: true });
            showToast('Paste decrypted successfully!', 'success');
        } catch (err) {
            console.error('Decryption failed:', err);
            showToast('Incorrect passphrase or corrupted payload', 'error');
        }
    });

    function renderPasteContent(key, text, metadata = {}) {
        currentRawContent = text;
        displayKeyTag.innerText = `Key: ${key}`;

        const lines = text.split('\n');
        const lineCount = lines.length;
        const byteSize = new Blob([text]).size;

        displayStatLines.innerText = `${lineCount} ${lineCount === 1 ? 'line' : 'lines'}`;
        displayStatSize.innerText = formatBytes(byteSize);

        viewerLineNumbers.innerText = Array.from({ length: lineCount }, (_, i) => i + 1).join('\n');
        displayContent.querySelector('code').textContent = text;
        linkRawView.href = `/raw/${encodeURIComponent(key)}`;

        pasteDisplayContainer.classList.remove('hidden');
        pasteDisplayContainer.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }

    btnCopyContent.addEventListener('click', () => {
        if (currentRawContent) {
            navigator.clipboard.writeText(currentRawContent);
            showToast('Content copied to clipboard!', 'success');
        }
    });

    btnDownload.addEventListener('click', () => {
        if (!currentRawContent) return;
        const blob = new Blob([currentRawContent], { type: 'text/plain;charset=utf-8' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `paste_${activeKey || 'snippet'}.txt`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        showToast('Download started', 'success');
    });

    // ==========================================
    // User Pastes Dashboard
    // ==========================================
    async function loadUserPastes() {
        if (!authToken) {
            mypastesLoggedOutPrompt.classList.remove('hidden');
            userPastesGrid.innerHTML = '';
            userPasteCount.innerText = '0';
            return;
        }

        mypastesLoggedOutPrompt.classList.add('hidden');

        try {
            const resp = await authFetch('/api/user/pastes');
            if (!resp.ok) throw new Error('Failed to fetch user pastes');

            const list = await resp.json();
            userPasteCount.innerText = list.length;
            renderUserPastes(list);
        } catch (err) {
            console.error('Error fetching user pastes:', err);
        }
    }

    btnRefreshUserPastes.addEventListener('click', loadUserPastes);

    function renderUserPastes(pastes) {
        userPastesGrid.innerHTML = '';

        if (pastes.length === 0) {
            userPastesGrid.innerHTML = `<div class="empty-state">You haven't published any pastes yet.</div>`;
            return;
        }

        pastes.forEach(p => {
            const card = document.createElement('div');
            card.className = 'user-paste-card';

            const title = p.title || 'Untitled Paste';
            const visClass = p.is_public ? 'badge-public' : 'badge-private';
            const visLabel = p.is_public ? '🌐 Public' : '🔒 Private';
            const encBadge = p.is_encrypted ? '<span class="badge-pill badge-encrypted">🔑 Encrypted</span>' : '';

            card.innerHTML = `
                <div>
                    <div class="card-top">
                        <div class="card-title-key">
                            <span class="card-title">${escapeHtml(title)}</span>
                            <span class="card-key">${escapeHtml(p.key)}</span>
                        </div>
                        <div class="card-badges">
                            <span class="badge-pill ${visClass}">${visLabel}</span>
                            ${encBadge}
                        </div>
                    </div>
                    <div class="card-snippet">${escapeHtml(p.snippet || '[Empty]')}</div>
                </div>
                <div class="card-footer">
                    <span>${p.created_at || ''}</span>
                    <div class="card-actions">
                        <button class="btn-secondary btn-sm btn-card-view" data-key="${p.key}">View</button>
                        <button class="btn-danger btn-sm btn-card-delete" data-key="${p.key}">Delete</button>
                    </div>
                </div>
            `;

            card.querySelector('.btn-card-view').addEventListener('click', () => {
                lookupKey(p.key);
            });

            card.querySelector('.btn-card-delete').addEventListener('click', async (e) => {
                e.stopPropagation();
                if (!confirm(`Delete paste ${p.key}?`)) return;

                try {
                    const delResp = await authFetch(`/api/pastes/${encodeURIComponent(p.key)}`, {
                        method: 'DELETE'
                    });
                    if (delResp.ok) {
                        showToast(`Paste ${p.key} deleted`, 'success');
                        loadUserPastes();
                    } else {
                        showToast('Failed to delete paste', 'error');
                    }
                } catch {
                    showToast('Error deleting paste', 'error');
                }
            });

            userPastesGrid.appendChild(card);
        });
    }

    function escapeHtml(str) {
        if (!str) return '';
        return str
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&#039;");
    }

    // ==========================================
    // Toast Notification System
    // ==========================================
    function showToast(message, type = 'info') {
        const toast = document.createElement('div');
        toast.className = `toast toast-${type}`;
        toast.innerHTML = `<span>${escapeHtml(message)}</span>`;
        toastContainer.appendChild(toast);

        setTimeout(() => {
            toast.style.opacity = '0';
            toast.style.transform = 'translateX(100%)';
            toast.style.transition = 'all 0.3s ease';
            setTimeout(() => toast.remove(), 300);
        }, 3400);
    }

    // ==========================================
    // URL Hash Routing on Load
    // ==========================================
    function checkHashRoute() {
        const hash = window.location.hash.replace(/^#/, '').trim();
        if (hash) {
            lookupKey(hash);
        }
    }

    window.addEventListener('hashchange', checkHashRoute);

    // Initial setup
    updateEditorStats();
    checkCurrentUser();
    checkHashRoute();
});
