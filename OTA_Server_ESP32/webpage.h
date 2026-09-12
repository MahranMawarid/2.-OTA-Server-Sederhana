#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 OTA Web Server Dashboard</title>
  <style>
    :root {
      --bg-primary: #0f172a;
      --bg-secondary: #1e293b;
      --bg-card: rgba(30, 41, 59, 0.7);
      --border-color: rgba(255, 255, 255, 0.1);
      --accent-primary: #38bdf8;
      --accent-hover: #0284c7;
      --accent-success: #10b981;
      --accent-warning: #f59e0b;
      --accent-danger: #ef4444;
      --text-main: #f8fafc;
      --text-muted: #94a3b8;
    }

    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
    }

    body {
      background: radial-gradient(circle at top, #1e293b 0%, #0f172a 100%);
      color: var(--text-main);
      min-height: 100vh;
      padding: 24px 16px;
      display: flex;
      justify-content: center;
      align-items: flex-start;
    }

    .container {
      max-width: 960px;
      width: 100%;
      display: flex;
      flex-direction: column;
      gap: 20px;
    }

    /* Glassmorphism Card */
    .card {
      background: var(--bg-card);
      backdrop-filter: blur(12px);
      -webkit-backdrop-filter: blur(12px);
      border: 1px solid var(--border-color);
      border-radius: 16px;
      padding: 24px;
      box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.3), 0 8px 10px -6px rgba(0, 0, 0, 0.3);
    }

    /* Header */
    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 16px;
    }

    .title-group h1 {
      font-size: 1.6rem;
      font-weight: 700;
      background: linear-gradient(to right, #38bdf8, #818cf8);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      display: flex;
      align-items: center;
      gap: 10px;
    }

    .title-group p {
      color: var(--text-muted);
      font-size: 0.88rem;
      margin-top: 4px;
    }

    .badge {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      padding: 6px 14px;
      border-radius: 9999px;
      font-size: 0.82rem;
      font-weight: 600;
      background: rgba(16, 185, 129, 0.15);
      color: var(--accent-success);
      border: 1px solid rgba(16, 185, 129, 0.3);
    }

    .badge-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--accent-success);
      box-shadow: 0 0 8px var(--accent-success);
      animation: pulse 2s infinite;
    }

    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.4; }
    }

    /* Grid Metrik */
    .metrics-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(210px, 1fr));
      gap: 16px;
    }

    .metric-card {
      background: rgba(15, 23, 42, 0.6);
      border: 1px solid var(--border-color);
      border-radius: 12px;
      padding: 16px;
    }

    .metric-title {
      font-size: 0.78rem;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      color: var(--text-muted);
      margin-bottom: 8px;
    }

    .metric-value {
      font-size: 1.35rem;
      font-weight: 700;
      color: var(--text-main);
      display: flex;
      align-items: baseline;
      gap: 6px;
    }

    .metric-sub {
      font-size: 0.78rem;
      color: var(--text-muted);
      margin-top: 6px;
    }

    .progress-bar-wrap {
      width: 100%;
      height: 6px;
      background: rgba(255, 255, 255, 0.1);
      border-radius: 4px;
      overflow: hidden;
      margin-top: 8px;
    }

    .progress-bar-fill {
      height: 100%;
      background: linear-gradient(90deg, #38bdf8, #818cf8);
      border-radius: 4px;
      transition: width 0.4s ease;
    }

    /* Section Title */
    .section-title {
      font-size: 1.15rem;
      font-weight: 600;
      margin-bottom: 16px;
      display: flex;
      align-items: center;
      gap: 8px;
    }

    /* Radio / Toggle Selector */
    .type-selector {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      margin-bottom: 20px;
    }

    .type-option {
      position: relative;
    }

    .type-option input[type="radio"] {
      position: absolute;
      opacity: 0;
    }

    .type-box {
      border: 1px solid var(--border-color);
      border-radius: 12px;
      padding: 14px 16px;
      cursor: pointer;
      display: flex;
      flex-direction: column;
      gap: 4px;
      transition: all 0.2s ease;
      background: rgba(15, 23, 42, 0.4);
    }

    .type-option input[type="radio"]:checked + .type-box {
      border-color: var(--accent-primary);
      background: rgba(56, 189, 248, 0.1);
      box-shadow: 0 0 12px rgba(56, 189, 248, 0.2);
    }

    .type-name {
      font-weight: 600;
      font-size: 0.95rem;
      color: var(--text-main);
    }

    .type-desc {
      font-size: 0.78rem;
      color: var(--text-muted);
    }

    /* Dropzone Upload */
    .dropzone {
      border: 2px dashed rgba(56, 189, 248, 0.4);
      border-radius: 14px;
      padding: 36px 20px;
      text-align: center;
      background: rgba(15, 23, 42, 0.4);
      cursor: pointer;
      transition: all 0.2s ease;
    }

    .dropzone:hover, .dropzone.dragover {
      border-color: var(--accent-primary);
      background: rgba(56, 189, 248, 0.08);
      transform: translateY(-2px);
    }

    .dropzone-icon {
      font-size: 2.5rem;
      margin-bottom: 12px;
      color: var(--accent-primary);
    }

    .dropzone-text {
      font-size: 0.95rem;
      font-weight: 500;
      color: var(--text-main);
    }

    .dropzone-hint {
      font-size: 0.8rem;
      color: var(--text-muted);
      margin-top: 6px;
    }

    .file-info-box {
      display: none;
      background: rgba(15, 23, 42, 0.8);
      border: 1px solid var(--border-color);
      border-radius: 10px;
      padding: 12px 16px;
      margin-top: 16px;
      align-items: center;
      justify-content: space-between;
    }

    .file-details {
      display: flex;
      flex-direction: column;
      gap: 2px;
    }

    .file-name {
      font-weight: 600;
      font-size: 0.9rem;
      color: var(--accent-primary);
    }

    .file-size {
      font-size: 0.78rem;
      color: var(--text-muted);
    }

    /* Buttons */
    .btn {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      padding: 12px 24px;
      border-radius: 10px;
      font-size: 0.92rem;
      font-weight: 600;
      border: none;
      cursor: pointer;
      transition: all 0.2s ease;
      text-decoration: none;
    }

    .btn-primary {
      background: linear-gradient(135deg, #0284c7, #2563eb);
      color: white;
      box-shadow: 0 4px 14px rgba(37, 99, 235, 0.3);
    }

    .btn-primary:hover:not(:disabled) {
      background: linear-gradient(135deg, #0369a1, #1d4ed8);
      transform: translateY(-1px);
    }

    .btn-danger {
      background: rgba(239, 68, 68, 0.15);
      color: var(--accent-danger);
      border: 1px solid rgba(239, 68, 68, 0.3);
    }

    .btn-danger:hover {
      background: rgba(239, 68, 68, 0.25);
    }

    .btn:disabled {
      opacity: 0.5;
      cursor: not-allowed;
      transform: none !important;
    }

    .btn-group {
      display: flex;
      gap: 12px;
      margin-top: 20px;
      flex-wrap: wrap;
    }

    /* Upload Progress Box */
    .upload-progress-box {
      display: none;
      margin-top: 20px;
      background: rgba(15, 23, 42, 0.8);
      border: 1px solid var(--border-color);
      border-radius: 12px;
      padding: 16px;
    }

    .upload-header {
      display: flex;
      justify-content: space-between;
      font-size: 0.88rem;
      font-weight: 600;
      margin-bottom: 8px;
    }

    .progress-track {
      width: 100%;
      height: 12px;
      background: rgba(255, 255, 255, 0.1);
      border-radius: 6px;
      overflow: hidden;
    }

    .progress-fill {
      width: 0%;
      height: 100%;
      background: linear-gradient(90deg, #38bdf8, #10b981);
      transition: width 0.2s ease;
      border-radius: 6px;
    }

    .upload-status-text {
      font-size: 0.82rem;
      color: var(--text-muted);
      margin-top: 8px;
      text-align: center;
    }

    /* Alert Message */
    .alert {
      display: none;
      padding: 12px 16px;
      border-radius: 10px;
      font-size: 0.88rem;
      margin-top: 16px;
    }

    .alert-success {
      background: rgba(16, 185, 129, 0.15);
      border: 1px solid rgba(16, 185, 129, 0.3);
      color: #34d399;
    }

    .alert-error {
      background: rgba(239, 68, 68, 0.15);
      border: 1px solid rgba(239, 68, 68, 0.3);
      color: #f87171;
    }

    /* Modal */
    .modal-overlay {
      display: none;
      position: fixed;
      inset: 0;
      background: rgba(0, 0, 0, 0.7);
      backdrop-filter: blur(8px);
      z-index: 100;
      justify-content: center;
      align-items: center;
      padding: 16px;
    }

    .modal {
      background: var(--bg-secondary);
      border: 1px solid var(--border-color);
      border-radius: 16px;
      max-width: 440px;
      width: 100%;
      padding: 24px;
      text-align: center;
    }

    .modal h3 {
      font-size: 1.25rem;
      margin-bottom: 8px;
    }

    .modal p {
      color: var(--text-muted);
      font-size: 0.88rem;
      margin-bottom: 20px;
      line-height: 1.4;
    }

    .countdown-number {
      font-size: 2.8rem;
      font-weight: 800;
      color: var(--accent-primary);
      margin: 12px 0;
    }

    @media (max-width: 640px) {
      .type-selector {
        grid-template-columns: 1fr;
      }
      .header {
        flex-direction: column;
        align-items: flex-start;
      }
    }
  </style>
</head>
<body>

  <div class="container">
    <!-- Header Card -->
    <div class="card header">
      <div class="title-group">
        <h1>
          <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="color:#38bdf8"><path d="M4 14.899A7 7 0 1 1 15.71 8h1.79a4.5 4.5 0 0 1 2.5 8.242"/><path d="M12 12v9"/><path d="m16 16-4-4-4 4"/></svg>
          ESP32 OTA Manager
        </h1>
        <p id="fw-info">Firmware: Loading... | Dual OTA Partition (1280 KB)</p>
      </div>
      <div class="badge">
        <div class="badge-dot"></div>
        <span id="conn-status">ONLINE</span>
      </div>
    </div>

    <!-- Live Telemetry Grid -->
    <div class="metrics-grid">
      <div class="metric-card">
        <div class="metric-title">Memori Heap (RAM)</div>
        <div class="metric-value"><span id="free-heap">-</span> <small style="font-size:0.8rem;font-weight:400">KB Free</small></div>
        <div class="progress-bar-wrap">
          <div id="heap-bar" class="progress-bar-fill" style="width: 0%"></div>
        </div>
        <div class="metric-sub" id="heap-detail">Min Free: - KB</div>
      </div>

      <div class="metric-card">
        <div class="metric-title">Sinyal WiFi (RSSI)</div>
        <div class="metric-value"><span id="wifi-rssi">-</span> <small style="font-size:0.8rem;font-weight:400">dBm</small></div>
        <div class="progress-bar-wrap">
          <div id="wifi-bar" class="progress-bar-fill" style="width: 0%"></div>
        </div>
        <div class="metric-sub" id="wifi-ssid">SSID: -</div>
      </div>

      <div class="metric-card">
        <div class="metric-title">Partisi & Flash</div>
        <div class="metric-value" style="font-size:1.15rem"><span id="active-partition" style="color:var(--accent-primary)">-</span></div>
        <div class="metric-sub" id="sketch-space" style="margin-top:14px">Free Sketch: - KB</div>
        <div class="metric-sub" id="chip-identity">MAC: 549738124B00</div>
      </div>

      <div class="metric-card">
        <div class="metric-title">Sistem & Uptime</div>
        <div class="metric-value" style="font-size:1.15rem"><span id="sys-uptime">-</span></div>
        <div class="metric-sub" id="cpu-freq" style="margin-top:14px">CPU: 240 MHz Dual-Core</div>
        <div class="metric-sub" id="ip-address">IP: -</div>
      </div>
    </div>

    <!-- OTA Upload Card -->
    <div class="card">
      <div class="section-title">
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="color:#38bdf8"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
        Pusat Pembaruan Over-The-Air (OTA)
      </div>

      <!-- Target Selection -->
      <div class="type-selector">
        <label class="type-option">
          <input type="radio" name="ota_type" value="firmware" checked>
          <div class="type-box">
            <span class="type-name">&#128187; Firmware Aplikasi</span>
            <span class="type-desc">Flash ke partisi app (app0/app1 - Max 1.28 MB)</span>
          </div>
        </label>
        <label class="type-option">
          <input type="radio" name="ota_type" value="spiffs">
          <div class="type-box">
            <span class="type-name">&#128193; Filesystem (SPIFFS)</span>
            <span class="type-desc">Flash data ke partisi spiffs (Max 1.40 MB)</span>
          </div>
        </label>
      </div>

      <!-- Dropzone -->
      <div id="dropzone" class="dropzone">
        <div class="dropzone-icon">&#9729;&#65039;</div>
        <div class="dropzone-text">Seret & Lepaskan file binary (.bin) di sini</div>
        <div class="dropzone-hint">atau klik untuk memilih file dari komputer</div>
        <input type="file" id="file-input" accept=".bin" style="display: none">
      </div>

      <!-- Selected File Info -->
      <div id="file-info" class="file-info-box">
        <div class="file-details">
          <span id="file-name" class="file-name">firmware.bin</span>
          <span id="file-size" class="file-size">0 KB</span>
        </div>
        <button type="button" id="btn-cancel-file" class="btn" style="padding:4px 8px;font-size:0.75rem;background:rgba(255,255,255,0.1);color:var(--text-muted)">Ganti</button>
      </div>

      <!-- Upload Progress -->
      <div id="progress-box" class="upload-progress-box">
        <div class="upload-header">
          <span id="upload-status-label">Mengunggah Binary...</span>
          <span id="upload-percent">0%</span>
        </div>
        <div class="progress-track">
          <div id="progress-bar" class="progress-fill"></div>
        </div>
        <div id="upload-details" class="upload-status-text">0 KB / 0 KB (0 KB/s)</div>
      </div>

      <!-- Alert Notification -->
      <div id="alert-msg" class="alert"></div>

      <!-- Actions -->
      <div class="btn-group">
        <button type="button" id="btn-upload" class="btn btn-primary" disabled>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
          Mulai Proses OTA Update
        </button>
        <button type="button" id="btn-reboot" class="btn btn-danger">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M18.36 6.64a9 9 0 1 1-12.73 0"/><line x1="12" y1="2" x2="12" y2="12"/></svg>
          Restart ESP32
        </button>
      </div>
    </div>
  </div>

  <!-- Modal Restart/Countdown -->
  <div id="modal-reboot" class="modal-overlay">
    <div class="modal">
      <h3 id="modal-title">ESP32 Sedang Reboot...</h3>
      <p id="modal-desc">Pembaruan firmware berhasil ditulis ke flash memory. Perangkat sedang memulai ulang.</p>
      <div id="countdown" class="countdown-number">10</div>
      <p style="font-size:0.8rem;color:var(--text-muted)">Halaman ini akan otomatis memuat ulang saat perangkat telah online.</p>
    </div>
  </div>

  <script>
    // State
    let selectedFile = null;
    let isUploading = false;
    let totalHeapSize = 377760;

    // DOM Elements
    const dropzone = document.getElementById('dropzone');
    const fileInput = document.getElementById('file-input');
    const fileInfo = document.getElementById('file-info');
    const fileName = document.getElementById('file-name');
    const fileSize = document.getElementById('file-size');
    const btnCancelFile = document.getElementById('btn-cancel-file');
    const btnUpload = document.getElementById('btn-upload');
    const btnReboot = document.getElementById('btn-reboot');
    const progressBox = document.getElementById('progress-box');
    const progressBar = document.getElementById('progress-bar');
    const uploadPercent = document.getElementById('upload-percent');
    const uploadStatusLabel = document.getElementById('upload-status-label');
    const uploadDetails = document.getElementById('upload-details');
    const alertMsg = document.getElementById('alert-msg');
    const modalReboot = document.getElementById('modal-reboot');
    const countdownEl = document.getElementById('countdown');

    // Drag and drop event listeners
    dropzone.addEventListener('click', () => fileInput.click());
    dropzone.addEventListener('dragover', (e) => {
      e.preventDefault();
      dropzone.classList.add('dragover');
    });
    dropzone.addEventListener('dragleave', () => dropzone.classList.remove('dragover'));
    dropzone.addEventListener('drop', (e) => {
      e.preventDefault();
      dropzone.classList.remove('dragover');
      if (e.dataTransfer.files.length > 0) {
        handleFileSelect(e.dataTransfer.files[0]);
      }
    });

    fileInput.addEventListener('change', (e) => {
      if (e.target.files.length > 0) {
        handleFileSelect(e.target.files[0]);
      }
    });

    btnCancelFile.addEventListener('click', (e) => {
      e.stopPropagation();
      resetFileSelection();
    });

    function handleFileSelect(file) {
      if (!file.name.endsWith('.bin')) {
        showAlert('Hanya file berekstensi .bin yang diperbolehkan!', 'error');
        return;
      }
      selectedFile = file;
      fileName.textContent = file.name;
      fileSize.textContent = (file.size / 1024).toFixed(1) + ' KB (' + (file.size / (1024 * 1024)).toFixed(2) + ' MB)';
      dropzone.style.display = 'none';
      fileInfo.style.display = 'flex';
      btnUpload.disabled = false;
      hideAlert();
    }

    function resetFileSelection() {
      selectedFile = null;
      fileInput.value = '';
      dropzone.style.display = 'block';
      fileInfo.style.display = 'none';
      btnUpload.disabled = true;
    }

    function showAlert(text, type) {
      alertMsg.textContent = text;
      alertMsg.className = 'alert ' + (type === 'success' ? 'alert-success' : 'alert-error');
      alertMsg.style.display = 'block';
    }

    function hideAlert() {
      alertMsg.style.display = 'none';
    }

    // Live Telemetry Poller
    function fetchTelemetry() {
      if (isUploading) return;
      fetch('/api/status')
        .then(res => res.json())
        .then(data => {
          document.getElementById('conn-status').textContent = 'ONLINE';
          document.getElementById('fw-info').textContent = 'Firmware: v' + data.firmware_version + ' (' + data.firmware_name + ')';
          
          // Heap
          const freeHeapKB = (data.free_heap / 1024).toFixed(1);
          const totalHeapKB = (data.total_heap / 1024).toFixed(1);
          document.getElementById('free-heap').textContent = freeHeapKB;
          const heapUsagePct = Math.round(((data.total_heap - data.free_heap) / data.total_heap) * 100);
          document.getElementById('heap-bar').style.width = (100 - heapUsagePct) + '%';
          document.getElementById('heap-detail').textContent = 'Min Free: ' + (data.min_free_heap / 1024).toFixed(1) + ' KB | Max Alloc: ' + (data.max_alloc_heap / 1024).toFixed(1) + ' KB';
          
          // WiFi
          document.getElementById('wifi-rssi').textContent = data.wifi_rssi;
          let wifiPct = Math.min(Math.max(2 * (data.wifi_rssi + 100), 0), 100);
          document.getElementById('wifi-bar').style.width = wifiPct + '%';
          document.getElementById('wifi-ssid').textContent = 'SSID: ' + data.wifi_ssid;

          // Partition & Flash
          document.getElementById('active-partition').textContent = data.current_partition + ' (Target: ' + data.next_partition + ')';
          document.getElementById('sketch-space').textContent = 'Free Sketch: ' + (data.free_sketch_space / 1024).toFixed(0) + ' KB';
          document.getElementById('chip-identity').textContent = 'MAC: ' + data.mac_address + ' | ID: ' + data.chip_id;

          // System & Uptime
          document.getElementById('sys-uptime').textContent = formatUptime(data.uptime_seconds);
          document.getElementById('ip-address').textContent = 'IP: ' + data.ip_address;
          document.getElementById('cpu-freq').textContent = 'CPU: ' + data.cpu_freq_mhz + ' MHz (' + data.chip_cores + ' Cores)';
        })
        .catch(err => {
          document.getElementById('conn-status').textContent = 'DISCONNECTED';
        });
    }

    function formatUptime(sec) {
      const d = Math.floor(sec / 86400);
      const h = Math.floor((sec % 86400) / 3600);
      const m = Math.floor((sec % 3600) / 60);
      const s = sec % 60;
      if (d > 0) return d + 'h ' + h + 'j ' + m + 'm';
      if (h > 0) return h + 'j ' + m + 'm ' + s + 'd';
      return m + 'm ' + s + 'd';
    }

    // OTA Upload Action
    btnUpload.addEventListener('click', () => {
      if (!selectedFile) return;
      
      const otaType = document.querySelector('input[name="ota_type"]:checked').value;
      const confirmMsg = otaType === 'firmware' 
        ? 'Apakah Anda yakin ingin memperbarui FIRMWARE dengan file ' + selectedFile.name + '?' 
        : 'Apakah Anda yakin ingin memperbarui FILESYSTEM (SPIFFS) dengan file ' + selectedFile.name + '?';

      if (!confirm(confirmMsg)) return;

      isUploading = true;
      btnUpload.disabled = true;
      btnReboot.disabled = true;
      btnCancelFile.style.display = 'none';
      progressBox.style.display = 'block';
      hideAlert();

      const formData = new FormData();
      formData.append('type', otaType);
      formData.append('update', selectedFile);

      const xhr = new XMLHttpRequest();
      const startTime = Date.now();

      xhr.upload.addEventListener('progress', (e) => {
        if (e.lengthComputable) {
          const pct = Math.round((e.loaded / e.total) * 100);
          progressBar.style.width = pct + '%';
          uploadPercent.textContent = pct + '%';
          
          const elapsedSec = (Date.now() - startTime) / 1000;
          const speedKBps = elapsedSec > 0 ? ((e.loaded / 1024) / elapsedSec).toFixed(1) : 0;
          uploadDetails.textContent = (e.loaded / 1024).toFixed(0) + ' KB / ' + (e.total / 1024).toFixed(0) + ' KB (' + speedKBps + ' KB/s)';

          if (pct === 100) {
            uploadStatusLabel.textContent = 'Menulis ke Flash Memory... Mohon Tunggu.';
          }
        }
      });

      xhr.onload = function() {
        isUploading = false;
        if (xhr.status === 200) {
          try {
            const resp = JSON.parse(xhr.responseText);
            if (resp.success) {
              showAlert(resp.message || 'OTA Update Berhasil! ESP32 akan reboot...', 'success');
              startRebootCountdown(12, 'Pembaruan Sukses!', 'Firmware baru telah tersimpan. Memulai ulang sistem ESP32...');
            } else {
              showAlert('Gagal: ' + (resp.message || 'Error tidak diketahui'), 'error');
              btnUpload.disabled = false;
              btnReboot.disabled = false;
              btnCancelFile.style.display = 'inline-block';
            }
          } catch(e) {
            showAlert('Pembaruan selesai: ' + xhr.responseText, 'success');
            startRebootCountdown(12, 'Pembaruan Sukses!', 'Memulai ulang sistem ESP32...');
          }
        } else {
          showAlert('Upload Gagal (HTTP ' + xhr.status + '): ' + xhr.responseText, 'error');
          btnUpload.disabled = false;
          btnReboot.disabled = false;
          btnCancelFile.style.display = 'inline-block';
        }
      };

      xhr.onerror = function() {
        isUploading = false;
        showAlert('Koneksi terputus saat upload berlangsung.', 'error');
        btnUpload.disabled = false;
        btnReboot.disabled = false;
        btnCancelFile.style.display = 'inline-block';
      };

      xhr.open('POST', '/update', true);
      xhr.send(formData);
    });

    // Reboot Button Action
    btnReboot.addEventListener('click', () => {
      if (!confirm('Apakah Anda yakin ingin me-restart ESP32 sekarang?')) return;
      fetch('/api/restart', { method: 'POST' })
        .then(res => res.json())
        .then(data => {
          startRebootCountdown(8, 'ESP32 Sedang Restart', 'Perangkat sedang melakukan booting ulang.');
        })
        .catch(err => {
          startRebootCountdown(8, 'ESP32 Sedang Restart', 'Perangkat sedang melakukan booting ulang.');
        });
    });

    function startRebootCountdown(seconds, title, desc) {
      document.getElementById('modal-title').textContent = title;
      document.getElementById('modal-desc').textContent = desc;
      modalReboot.style.display = 'flex';
      let remaining = seconds;
      countdownEl.textContent = remaining;

      const timer = setInterval(() => {
        remaining--;
        countdownEl.textContent = remaining;
        if (remaining <= 0) {
          clearInterval(timer);
          window.location.reload();
        }
      }, 1000);
    }

    // Initial load and polling every 2.5s
    fetchTelemetry();
    setInterval(fetchTelemetry, 2500);
  </script>
</body>
</html>
)rawliteral";

#endif // WEBPAGE_H
