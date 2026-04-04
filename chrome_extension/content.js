const API_BASE = 'http://localhost:7070/api/v1';

function getVideoId() {
  const params = new URLSearchParams(window.location.search);
  return params.get('v');
}

function getCurrentUrl() {
  return window.location.href;
}

function injectDubButton() {
  if (document.getElementById('vd-dub-btn')) return;

  const actionsRow = document.querySelector('#actions-inner, #top-level-buttons-computed');
  if (!actionsRow) return;

  const btn = document.createElement('button');
  btn.id = 'vd-dub-btn';
  btn.innerHTML = '🎬 Dub This';
  btn.style.cssText = `
    background: #89b4fa; color: #1e1e2e; border: none;
    padding: 8px 16px; border-radius: 20px; font-weight: bold;
    cursor: pointer; font-size: 14px; margin-left: 8px;
    transition: background 0.2s;
  `;
  
  btn.onmouseenter = () => btn.style.background = '#74c7ec';
  btn.onmouseleave = () => btn.style.background = '#89b4fa';
  btn.onclick = startDubbing;

  actionsRow.appendChild(btn);
}

// Overlay shown while dubbing
let overlay = null;

function showOverlay(msg, progress) {
  if (!overlay) {
    overlay = document.createElement('div');
    overlay.id = 'vd-overlay';
    overlay.style.cssText = `
      position: fixed; top: 20px; right: 20px; z-index: 9999;
      background: #1e1e2e; color: #cdd6f4; border: 1px solid #89b4fa;
      border-radius: 12px; padding: 16px 20px; min-width: 280px;
      box-shadow: 0 4px 24px rgba(0,0,0,0.5); font-family: sans-serif;
    `;
    document.body.appendChild(overlay);
  }

  overlay.innerHTML = `
    <div style="font-weight:bold;margin-bottom:8px;">🎬 VideoDubber</div>
    <div style="color:#a6e3a1;margin-bottom:10px;">${msg}</div>
    <div style="background:#313244;border-radius:4px;height:8px;overflow:hidden;">
      <div style="background:#89b4fa;height:100%;width:${progress}%;transition:width 0.3s;"></div>
    </div>
    <div style="font-size:12px;color:#6c7086;margin-top:6px;">${progress}% complete</div>
    <button id="vd-close" style="position:absolute;top:8px;right:12px;background:none;border:none;color:#6c7086;cursor:pointer;font-size:16px;">✕</button>
  `;

  document.getElementById('vd-close')?.addEventListener('click', () => {
    overlay?.remove(); overlay = null;
  });
}

function hideOverlay() {
  setTimeout(() => { overlay?.remove(); overlay = null; }, 4000);
}

async function startDubbing() {
  const url = getCurrentUrl();
  if (!url.includes('youtube.com/watch')) {
    alert('Please open a YouTube video first.');
    return;
  }

  const settings = await chrome.storage.sync.get({
    target_lang: 'hi',
    tts_backend: 'gemini',
    mix_mode: 'overlay'
  });

  showOverlay('Sending to VideoDubber...', 2);
  
  try {
    const resp = await fetch(`${API_BASE}/dub`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        url: url,
        source_lang: 'auto',
        target_lang: settings.target_lang,
        tts_backend: settings.tts_backend,
        translation_backend: 'google',
        mix_mode: settings.mix_mode
      })
    });

    if (!resp.ok) throw new Error('API call failed: ' + resp.status);
    
    const { job_id } = await resp.json();
    showOverlay('Job started! Polling...', 5);
    pollJobStatus(job_id);

  } catch (err) {
    showOverlay(`❌ Error: ${err.message}\nIs VideoDubber running?`, 0);
  }
}

async function pollJobStatus(job_id) {
  const interval = setInterval(async () => {
    try {
      const resp = await fetch(`${API_BASE}/job/${job_id}/status`);
      const status = await resp.json();
      
      const stageLabels = {
        extraction: 'Extracting audio...',
        transcription: 'Transcribing speech...',
        translation: 'Translating...',
        tts: 'Generating voice...',
        sync: 'Syncing timing...',
        mixing: 'Mixing audio...',
        muxing: 'Finalizing video...',
        done: '✅ Done! Check your Videos/Dubbed folder',
        failed: '❌ Failed — check the app for details'
      };

      const label = stageLabels[status.stage] || status.stage;
      showOverlay(label, status.progress);

      if (status.completed || status.failed) {
        clearInterval(interval);
        hideOverlay();
      }
    } catch (err) {
      clearInterval(interval);
    }
  }, 2000);
}

// Observe YouTube's SPA navigation
const observer = new MutationObserver(() => injectDubButton());
observer.observe(document.body, { childList: true, subtree: true });

injectDubButton();
