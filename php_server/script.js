// State
let selectedFile = null;

// DOM Elements
const dropzone = document.getElementById('dropzone');
const binaryFileInput = document.getElementById('binary-file');
const fileInfo = document.getElementById('file-info');
const fileName = document.getElementById('file-name');
const fileSize = document.getElementById('file-size');
const btnCancelFile = document.getElementById('btn-cancel-file');
const btnSubmit = document.getElementById('btn-submit');
const uploadForm = document.getElementById('upload-form');
const versionInput = document.getElementById('version-input');
const macInput = document.getElementById('mac-input');
const changelogInput = document.getElementById('changelog-input');
const progressContainer = document.getElementById('progress-container');
const progressBar = document.getElementById('progress-bar');
const uploadPercent = document.getElementById('upload-percent');
const uploadStatusText = document.getElementById('upload-status-text');
const alertBox = document.getElementById('alert-box');
const deviceTableBody = document.getElementById('device-table-body');
const btnDeleteFw = document.getElementById('btn-delete-fw');
const toggleAutoDeploy = document.getElementById('toggle-autodeploy');
const pollIntervalSelect = document.getElementById('poll-interval-select');

// Base API Path relative
const API_URL = 'api.php';

// Drag and drop events
dropzone.addEventListener('click', () => binaryFileInput.click());
dropzone.addEventListener('dragover', (e) => {
  e.preventDefault();
  dropzone.classList.add('dragover');
});
dropzone.addEventListener('dragleave', () => dropzone.classList.remove('dragover'));
dropzone.addEventListener('drop', (e) => {
  e.preventDefault();
  dropzone.classList.remove('dragover');
  if (e.dataTransfer.files.length > 0) {
    handleFile(e.dataTransfer.files[0]);
  }
});

binaryFileInput.addEventListener('change', (e) => {
  if (e.target.files.length > 0) {
    handleFile(e.target.files[0]);
  }
});

btnCancelFile.addEventListener('click', () => {
  resetFileInput();
});

function handleFile(file) {
  if (!file.name.toLowerCase().endsWith('.bin')) {
    showAlert('Hanya file dengan format .bin yang diperbolehkan!', 'error');
    return;
  }
  selectedFile = file;
  fileName.textContent = file.name;
  fileSize.textContent = (file.size / 1024).toFixed(1) + ' KB (' + (file.size / (1024 * 1024)).toFixed(2) + ' MB)';
  dropzone.style.display = 'none';
  fileInfo.style.display = 'flex';
  btnSubmit.disabled = false;
  hideAlert();
}

function resetFileInput() {
  selectedFile = null;
  binaryFileInput.value = '';
  dropzone.style.display = 'block';
  fileInfo.style.display = 'none';
  btnSubmit.disabled = true;
}

function showAlert(text, type) {
  alertBox.textContent = text;
  alertBox.className = 'alert ' + (type === 'success' ? 'alert-success' : 'alert-error');
  alertBox.style.display = 'block';
}

function hideAlert() {
  alertBox.style.display = 'none';
}

// Upload Form Submit Handler
uploadForm.addEventListener('submit', (e) => {
  e.preventDefault();
  if (!selectedFile) return;

  const type = document.querySelector('input[name="ota_type"]:checked').value;
  const version = versionInput.value.trim();
  const mac = macInput.value.trim();
  const changelog = changelogInput.value.trim();

  if (!version) {
    showAlert('Nomor versi harus diisi!', 'error');
    return;
  }

  btnSubmit.disabled = true;
  progressContainer.style.display = 'block';
  hideAlert();

  const formData = new FormData();
  formData.append('type', type);
  formData.append('version', version);
  formData.append('targetMac', mac);
  formData.append('changelog', changelog);
  formData.append('binary', selectedFile);

  const xhr = new XMLHttpRequest();
  xhr.upload.addEventListener('progress', (ev) => {
    if (ev.lengthComputable) {
      const pct = Math.round((ev.loaded / ev.total) * 100);
      progressBar.style.width = pct + '%';
      uploadPercent.textContent = pct + '%';
      uploadStatusText.textContent = `Mengunggah: ${(ev.loaded / 1024).toFixed(0)} KB / ${(ev.total / 1024).toFixed(0)} KB`;
    }
  });

  xhr.onload = function() {
    btnSubmit.disabled = false;
    progressContainer.style.display = 'none';
    if (xhr.status === 200) {
      try {
        const resp = JSON.parse(xhr.responseText);
        if (resp.success) {
          showAlert(resp.message, 'success');
          resetFileInput();
          changelogInput.value = '';
          fetchServerConfig();
        } else {
          showAlert(resp.message || 'Gagal mengunggah firmware.', 'error');
        }
      } catch (err) {
        showAlert('Berhasil: ' + xhr.responseText, 'success');
        fetchServerConfig();
      }
    } else {
      showAlert(`Error ${xhr.status}: ${xhr.responseText}`, 'error');
    }
  };

  xhr.onerror = function() {
    btnSubmit.disabled = false;
    progressContainer.style.display = 'none';
    showAlert('Koneksi ke server terputus saat upload.', 'error');
  };

  xhr.open('POST', `${API_URL}?action=upload`, true);
  xhr.send(formData);
});

// Delete Firmware
btnDeleteFw.addEventListener('click', () => {
  if (!confirm('Apakah Anda yakin ingin menghapus binary firmware aktif di server?')) return;
  fetch(`${API_URL}?action=delete_firmware&type=firmware`)
    .then(r => r.json())
    .then(data => {
      showAlert(data.message, 'success');
      fetchServerConfig();
    })
    .catch(e => showAlert('Gagal menghapus firmware: ' + e, 'error'));
});

// Settings Handlers
toggleAutoDeploy.addEventListener('change', () => {
  updateSettings({ autoDeploy: toggleAutoDeploy.checked });
});

pollIntervalSelect.addEventListener('change', () => {
  updateSettings({ pollIntervalSec: parseInt(pollIntervalSelect.value, 10) });
});

function updateSettings(newSettings) {
  fetch(`${API_URL}?action=settings`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(newSettings)
  })
  .then(r => r.json())
  .then(data => {
    console.log('Settings updated:', data.settings);
  })
  .catch(err => console.error('Error updating settings:', err));
}

// Manual OTA Trigger
window.triggerDeviceOta = function(mac) {
  if (!confirm(`Kirim perintah update OTA sekarang ke perangkat ${mac}?`)) return;

  fetch(`${API_URL}?action=trigger_ota`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ mac: mac, type: 'firmware' })
  })
  .then(r => r.json())
  .then(data => {
    if (data.success) {
      showAlert(data.message, 'success');
      fetchDevices();
    } else {
      showAlert(data.message, 'error');
    }
  })
  .catch(err => showAlert('Gagal mengirim perintah OTA: ' + err, 'error'));
};

// Fetch Server Config (Firmware Status & Settings)
function fetchServerConfig() {
  fetch(`${API_URL}?action=config`)
    .then(r => r.json())
    .then(cfg => {
      // Sync Settings
      if (cfg.settings) {
        toggleAutoDeploy.checked = Boolean(cfg.settings.autoDeploy);
        if (cfg.settings.pollIntervalSec) {
          pollIntervalSelect.value = String(cfg.settings.pollIntervalSec);
        }
      }

      const fw = cfg.firmware;
      if (fw && fw.filename) {
        document.getElementById('srv-fw-ver').textContent = 'v' + fw.version;
        document.getElementById('srv-fw-file').textContent = fw.originalName || fw.filename;
        document.getElementById('srv-fw-size').textContent = (fw.size / 1024).toFixed(1) + ' KB (' + (fw.size / (1024 * 1024)).toFixed(2) + ' MB)';
        document.getElementById('srv-fw-md5').textContent = fw.md5 || '-';
        document.getElementById('srv-fw-date').textContent = new Date(fw.uploadDate).toLocaleString('id-ID');
        document.getElementById('srv-fw-mac').textContent = fw.targetMac ? fw.targetMac : 'Semua Device (Global)';
        document.getElementById('srv-fw-changelog').textContent = fw.changelog || '-';
        btnDeleteFw.style.display = 'block';

        // Suggest next version in form
        const parts = fw.version.split('.');
        if (parts.length === 3 && !versionInput.value) {
          versionInput.value = `${parts[0]}.${parts[1]}.${parseInt(parts[2], 10) + 1}`;
        }
      } else {
        document.getElementById('srv-fw-ver').textContent = 'Belum Ada';
        document.getElementById('srv-fw-file').textContent = 'Belum ada file .bin yang diunggah';
        document.getElementById('srv-fw-size').textContent = '-';
        document.getElementById('srv-fw-md5').textContent = '-';
        document.getElementById('srv-fw-date').textContent = '-';
        document.getElementById('srv-fw-mac').textContent = '-';
        document.getElementById('srv-fw-changelog').textContent = '-';
        btnDeleteFw.style.display = 'none';
      }
    })
    .catch(err => console.error('Error fetching config:', err));
}

// Fetch Devices (Heartbeat Table with Action Column)
function fetchDevices() {
  fetch(`${API_URL}?action=devices`)
    .then(r => r.json())
    .then(devices => {
      if (!devices || devices.length === 0) {
        deviceTableBody.innerHTML = '<tr><td colspan="10" class="empty-state">Menunggu ESP32 mengirimkan heartbeat pertama...</td></tr>';
        return;
      }

      let html = '';
      devices.forEach(dev => {
        const statusBadge = dev.isOnline
          ? '<span class="badge badge-success"><span class="pulse-dot"></span> ONLINE</span>'
          : '<span class="badge badge-offline">OFFLINE</span>';

        const lastSeenText = dev.lastSeenSecondsAgo < 60
          ? `${dev.lastSeenSecondsAgo} detik lalu`
          : `${Math.floor(dev.lastSeenSecondsAgo / 60)} menit lalu`;

        let actionBtn = '';
        if (dev.hasPendingOta) {
          actionBtn = '<span class="badge badge-warning">⏳ Antrean Update</span>';
        } else if (dev.status && dev.status.startsWith('updating')) {
          actionBtn = '<span class="badge badge-indigo">🚀 Sedang Flashing...</span>';
        } else {
          actionBtn = `<button type="button" class="btn-sm btn-primary" onclick="triggerDeviceOta('${dev.mac}')">🚀 Deploy Update</button>`;
        }

        html += `
          <tr>
            <td>${statusBadge}</td>
            <td><strong><code>${dev.mac}</code></strong></td>
            <td>${dev.ip}</td>
            <td><span class="badge badge-indigo">v${dev.version}</span></td>
            <td>${(dev.free_heap / 1024).toFixed(1)} KB</td>
            <td>${dev.rssi} dBm</td>
            <td><code>${dev.partition}</code></td>
            <td>${formatUptime(dev.uptime)}</td>
            <td>${lastSeenText}</td>
            <td>${actionBtn}</td>
          </tr>
        `;
      });
      deviceTableBody.innerHTML = html;
    })
    .catch(err => console.error('Error fetching devices:', err));
}

function formatUptime(sec) {
  if (!sec) return '0s';
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = sec % 60;
  if (h > 0) return `${h}j ${m}m ${s}d`;
  return `${m}m ${s}d`;
}

// Initial fetch and periodic polling
fetchServerConfig();
fetchDevices();
setInterval(fetchDevices, 3000);
