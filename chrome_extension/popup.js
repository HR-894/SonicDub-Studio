const API_BASE = 'http://localhost:7070/api/v1';

// Load saved settings
chrome.storage.sync.get({
  target_lang: 'hi', tts_backend: 'gemini', mix_mode: 'overlay'
}, (s) => {
  document.getElementById('target_lang').value = s.target_lang;
  document.getElementById('tts_backend').value = s.tts_backend;
  document.getElementById('mix_mode').value    = s.mix_mode;
});

// Save on change
['target_lang','tts_backend','mix_mode'].forEach(id => {
  document.getElementById(id).addEventListener('change', saveSettings);
});

function saveSettings() {
  chrome.storage.sync.set({
    target_lang: document.getElementById('target_lang').value,
    tts_backend: document.getElementById('tts_backend').value,
    mix_mode:    document.getElementById('mix_mode').value,
  });
}

document.getElementById('dub_btn').addEventListener('click', async () => {
  const status = document.getElementById('status');
  
  // Get current tab URL
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  
  if (!tab.url.includes('youtube.com/watch')) {
    status.textContent = '❌ Please open a YouTube video first';
    status.style.color = '#f38ba8';
    return;
  }

  // Check if app is running
  try {
    await fetch(API_BASE + '/job/ping/status', { signal: AbortSignal.timeout(2000) });
  } catch {
    status.textContent = '❌ VideoDubber app not running on port 7070';
    status.style.color = '#f38ba8';
    return;
  }

  status.textContent = '📤 Sending to VideoDubber...';
  status.style.color = '#89b4fa';

  try {
    const resp = await fetch(`${API_BASE}/dub`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        url: tab.url,
        source_lang: 'auto',
        target_lang: document.getElementById('target_lang').value,
        tts_backend: document.getElementById('tts_backend').value,
        mix_mode: document.getElementById('mix_mode').value,
      })
    });

    const { job_id } = await resp.json();
    status.textContent = `✅ Job started! ID: ${job_id.substring(0,8)}...`;
    status.style.color = '#a6e3a1';

    // Poll status
    const poll = setInterval(async () => {
      try {
        const s = await (await fetch(`${API_BASE}/job/${job_id}/status`)).json();
        status.textContent = `${s.stage}: ${s.progress}%`;
        if (s.completed) {
          status.textContent = '✅ Done! Check Videos/Dubbed folder';
          clearInterval(poll);
        }
        if (s.failed) {
          status.textContent = '❌ ' + s.error;
          status.style.color = '#f38ba8';
          clearInterval(poll);
        }
      } catch { clearInterval(poll); }
    }, 3000);

  } catch(e) {
    status.textContent = '❌ ' + e.message;
    status.style.color = '#f38ba8';
  }
});
