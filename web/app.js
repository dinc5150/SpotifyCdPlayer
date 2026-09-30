// Spotify CD Player portal: shared helpers plus one init function per page
// (chosen by <body data-page>). Plain JS, no build step.
'use strict';

const $ = (sel) => document.querySelector(sel);

function show(el, visible) {
  (typeof el === 'string' ? $(el) : el).classList.toggle('hidden', !visible);
}

function setText(sel, text) {
  $(sel).textContent = text == null ? '' : text;
}

// A result line under a button: red for errors, green for success.
function result(sel, text, ok) {
  const el = $(sel);
  el.textContent = text || '';
  el.classList.toggle('error', ok === false);
  el.classList.toggle('ok', ok === true);
  el.classList.toggle('muted', ok === undefined);
}

const kMinAdminPassword = 6;  // Matches the device's check

// Checks the two admin password fields; returns an error message or ''.
function adminPasswordProblem(password, repeat) {
  if (!password) return '';
  if (password.length < kMinAdminPassword) return `The admin password needs at least ${kMinAdminPassword} characters`;
  if (password !== repeat) return "The admin passwords don't match";
  return '';
}

async function getJson(path) {
  const res = await fetch(path, { cache: 'no-store' });
  if (!res.ok) throw new Error((await res.json().catch(() => ({}))).error || res.statusText);
  return res.json();
}

async function post(path, fields) {
  const res = await fetch(path, { method: 'POST', body: new URLSearchParams(fields) });
  const body = await res.json().catch(() => ({}));
  if (!res.ok || body.ok === false) throw new Error(body.error || res.statusText);
  return body;
}

function signal(rssi) {
  if (!rssi) return '';
  const bars = rssi > -55 ? 'Excellent' : rssi > -65 ? 'Good' : rssi > -75 ? 'Fair' : 'Weak';
  return `${bars} (${rssi} dBm)`;
}

function uptime(s) {
  const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60);
  return d ? `${d}d ${h}h` : h ? `${h}h ${m}m` : `${m}m`;
}

function deviceUrls(wifi) {
  return `http://${wifi.host}.local/ or http://${wifi.ip}/`;
}

// A list of scanned networks; clicking one fills `ssidInput`.
function renderScan(list, networks, ssidInput) {
  list.innerHTML = '';
  if (!networks.length) {
    list.innerHTML = '<li class="muted">Searching…</li>';
    return;
  }
  for (const n of networks) {
    const li = document.createElement('li');
    li.innerHTML = `<span></span><span class="muted"></span>`;
    li.firstChild.textContent = (n.secure ? '🔒 ' : '') + n.ssid;
    li.lastChild.textContent = signal(n.rssi).split(' ')[0];
    if (n.ssid === ssidInput.value) li.classList.add('selected');
    li.onclick = () => {
      ssidInput.value = n.ssid;
      list.querySelectorAll('li').forEach((x) => x.classList.remove('selected'));
      li.classList.add('selected');
      ssidInput.dispatchEvent(new Event('input'));
    };
    list.appendChild(li);
  }
}

// Keeps the scan list fresh while `active()` is true.
function pollScan(list, ssidInput, active) {
  const tick = async () => {
    if (!active()) return;
    try {
      const w = await getJson('/api/wifi');
      renderScan(list, w.scan, ssidInput);
      setTimeout(tick, w.scanning ? 1500 : 8000);
    } catch (e) {
      setTimeout(tick, 3000);
    }
  };
  tick();
}

// Waits for the result of a submitted network. The phone may briefly lose the
// setup Wi-Fi while the device switches channel, so fetch errors are expected.
function watchConnect(ssid, onDone) {
  const started = Date.now();
  let sawConnecting = false;
  const tick = async () => {
    try {
      const s = await getJson('/api/status');
      if (s.wifi.connecting) sawConnecting = true;
      if (s.wifi.connected && s.wifi.ssid === ssid) return onDone(true, s);
      if (sawConnecting && !s.wifi.connecting && s.wifi.error) return onDone(false, s);
    } catch (e) { /* Keep waiting */ }
    if (Date.now() - started > 45000) return onDone(false, null);
    setTimeout(tick, 1000);
  };
  setTimeout(tick, 800);
}

// ---------------------------------------------------------------- Status page
async function initIndex() {
  const refresh = async () => {
    try {
      const s = await getJson('/api/status');
      document.title = s.name;
      setText('#name', s.name);
      setText('#state', s.state.replace(/_/g, ' '));
      setText('#wifi', s.wifi.connected ? `${s.wifi.ssid} · ${signal(s.wifi.rssi)}` : (s.wifi.connecting ? `Connecting to ${s.wifi.connecting_ssid}…` : 'Not connected'));
      setText('#address', s.wifi.connected ? deviceUrls(s.wifi) : '—');
      setText('#spotify', s.spotify.linked ? 'Linked' : 'Not linked yet');
      setText('#fw', s.fw);
      setText('#uptime', uptime(s.uptime_s));
      setText('#reset', s.last_crash ? `${s.reset_reason}: ${s.last_crash}` : s.reset_reason);
      show('#rolledback', s.ota.rolled_back);
      show('#setup-link', s.setup_ap);
      show('#admin-warning', !s.admin_set);
    } catch (e) { /* Offline for a moment */ }
  };
  refresh();
  setInterval(refresh, 5000);
}

// ---------------------------------------------------------------- Setup wizard
async function initSetup() {
  const steps = ['#step-wifi', '#step-device', '#step-spotify', '#step-connect'];
  let current = 0;
  const go = (i) => {
    current = i;
    steps.forEach((s, j) => show(s, j === i));
    document.querySelectorAll('.steps span').forEach((s, j) => s.classList.toggle('done', j <= i));
    window.scrollTo(0, 0);
  };

  const ssid = $('#ssid'), password = $('#password');
  pollScan($('#scan'), ssid, () => current === 0);
  ssid.oninput = () => { $('#wifi-next').disabled = !ssid.value.trim(); };

  try {
    const d = await getJson('/api/device');
    $('#name').value = d.name;
    $('#host').value = d.host;
    $('#client-id').value = d.client_id;
    $('#relay-url').value = d.relay_url;
  } catch (e) { /* Defaults */ }

  $('#wifi-next').onclick = () => { setText('#wifi-error', ''); go(1); };
  $('#device-back').onclick = () => go(0);
  $('#device-next').onclick = async () => {
    setText('#device-error', '');
    const problem = adminPasswordProblem($('#admin').value, $('#admin2').value);
    if (problem) return setText('#device-error', problem);
    try {
      const fields = { name: $('#name').value, host: $('#host').value };
      if ($('#admin').value) fields.admin_password = $('#admin').value;
      const saved = await post('/api/device', fields);
      $('#host').value = saved.host;  // As cleaned up by the device
      go(2);
    } catch (e) { setText('#device-error', e.message); }
  };
  $('#spotify-back').onclick = () => go(1);
  $('#spotify-next').onclick = async () => {
    setText('#spotify-error', '');
    try {
      await post('/api/device', { client_id: $('#client-id').value, relay_url: $('#relay-url').value });
      go(3);
      connect();
    } catch (e) { setText('#spotify-error', e.message); }
  };
  $('#spotify-skip').onclick = () => { go(3); connect(); };
  $('#relay-url').oninput = () => setText('#redirect-uri', $('#relay-url').value.trim() || 'https://<you>.github.io/<repo>/');

  const connect = async () => {
    show('#connect-progress', true);
    show('#connect-done', false);
    show('#connect-failed', false);
    setText('#connect-ssid', ssid.value);
    try {
      await post('/api/wifi', { ssid: ssid.value.trim(), password: password.value });
    } catch (e) {
      show('#connect-progress', false);
      show('#connect-failed', true);
      return setText('#connect-error', e.message);
    }
    watchConnect(ssid.value.trim(), (ok, s) => {
      show('#connect-progress', false);
      if (ok) {
        show('#connect-done', true);
        setText('#home-ssid', s.wifi.ssid);
        setText('#device-url', deviceUrls(s.wifi));
      } else {
        show('#connect-failed', true);
        setText('#connect-error', (s && s.wifi.error) || 'No answer. Check the network name and password.');
      }
    });
  };
  $('#connect-retry').onclick = () => { setText('#wifi-error', $('#connect-error').textContent); go(0); };

  go(0);
}

// ---------------------------------------------------------------- Manage page
async function initManage() {
  const refreshWifi = async () => {
    const w = await getJson('/api/wifi');
    const saved = $('#saved');
    saved.innerHTML = w.saved.length ? '' : '<li class="muted">None</li>';
    for (const name of w.saved) {
      const li = document.createElement('li');
      li.innerHTML = '<span></span><button class="secondary">Forget</button>';
      li.firstChild.textContent = name;
      li.lastChild.onclick = async (ev) => {
        ev.stopPropagation();
        if (!confirm(`Forget "${name}"?`)) return;
        await post('/api/wifi/forget', { ssid: name });
        setTimeout(refreshWifi, 500);
      };
      saved.appendChild(li);
    }
  };
  refreshWifi();
  pollScan($('#scan'), $('#ssid'), () => true);

  $('#add-network').onclick = async () => {
    const ssid = $('#ssid').value.trim();
    setText('#wifi-result', `Connecting to ${ssid}…`);
    try {
      await post('/api/wifi', { ssid, password: $('#password').value });
      watchConnect(ssid, (ok, s) => {
        setText('#wifi-result', ok ? `Connected to ${ssid}.` : `Couldn't join ${ssid}: ${(s && s.wifi.error) || 'no answer'}`);
        refreshWifi();
      });
    } catch (e) { setText('#wifi-result', e.message); }
  };

  const d = await getJson('/api/device');
  $('#name').value = d.name;
  $('#host').value = d.host;
  setText('#admin-state', d.admin_set ? 'An admin password is set.' : 'No admin password: anyone on your network can change settings.');
  $('#client-id').value = d.client_id;
  $('#relay-url').value = d.relay_url;

  $('#save-device').onclick = async () => {
    result('#device-result', '');
    const fields = { name: $('#name').value, host: $('#host').value, client_id: $('#client-id').value, relay_url: $('#relay-url').value };
    if ($('#admin-clear').checked) {
      fields.admin_password = '';
    } else if ($('#admin').value) {
      const problem = adminPasswordProblem($('#admin').value, $('#admin2').value);
      if (problem) return result('#device-result', problem, false);
      fields.admin_password = $('#admin').value;
    }
    try {
      const saved = await post('/api/device', fields);
      $('#host').value = saved.host;  // As cleaned up by the device
      $('#admin').value = $('#admin2').value = '';
      $('#admin-clear').checked = false;
      result('#device-result', `Saved. Web address: http://${saved.host}.local/`, true);
      if ('admin_password' in fields) {
        setText('#admin-state', fields.admin_password ? 'An admin password is set. Your browser will ask for it next time.' : 'No admin password: anyone on your network can change settings.');
      }
    } catch (e) { result('#device-result', e.message, false); }
  };

  const s = await getJson('/api/status');
  setText('#fw', s.fw);
  show('#rolledback', s.ota.rolled_back);

  $('#upload').onclick = () => {
    const file = $('#firmware').files[0];
    if (!file) return setText('#upload-result', 'Choose a firmware .bin file first.');
    const form = new FormData();
    form.append('firmware', file, file.name);
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/update');
    show('#upload-progress', true);
    xhr.upload.onprogress = (e) => { if (e.lengthComputable) $('#upload-progress').value = e.loaded / e.total; };
    xhr.onload = () => {
      const body = JSON.parse(xhr.responseText || '{}');
      if (xhr.status === 200 && body.ok) {
        setText('#upload-result', 'Installed. The device is restarting; this page reloads in 20 s.');
        setTimeout(() => location.reload(), 20000);
      } else {
        setText('#upload-result', `Update failed: ${body.error || xhr.statusText}`);
      }
    };
    xhr.onerror = () => setText('#upload-result', 'Upload interrupted.');
    setText('#upload-result', 'Uploading…');
    xhr.send(form);
  };

  $('#load-logs').onclick = async () => {
    const res = await fetch('/api/logs', { cache: 'no-store' });
    setText('#logs', await res.text());
    show('#logs', true);
  };
  $('#reboot').onclick = async () => {
    if (!confirm('Restart the device?')) return;
    await post('/api/reboot', {});
    setText('#maintenance-result', 'Restarting…');
  };
  $('#factory-reset').onclick = async () => {
    if (!confirm('Erase Wi-Fi, the Spotify link and all settings?')) return;
    if (!confirm('Really erase everything? The device restarts in setup mode.')) return;
    await post('/api/factory-reset', {});
    setText('#maintenance-result', 'Erased. The device restarts in setup mode.');
  };
}

document.addEventListener('DOMContentLoaded', () => {
  const page = document.body.dataset.page;
  if (page === 'index') initIndex();
  if (page === 'setup') initSetup();
  if (page === 'manage') initManage();
});
