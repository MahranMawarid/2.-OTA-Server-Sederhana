<?php
/**
 * ESP32 OTA Management Web Server Dashboard (PHP Native untuk Laragon)
 */

function getServerLocalIP() {
    // Coba deteksi IP lokal
    $ip = gethostbyname(gethostname());
    if ($ip && $ip !== '127.0.0.1') return $ip;
    return $_SERVER['SERVER_ADDR'] ?? '127.0.0.1';
}

$localIP = getServerLocalIP();
$serverPort = $_SERVER['SERVER_PORT'] == '80' ? '' : ':' . $_SERVER['SERVER_PORT'];
$projectFolder = basename(__DIR__);
$fullLocalUrl = "http://{$localIP}{$serverPort}/{$projectFolder}";
?>
<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 OTA Server Dashboard (Laragon / PHP)</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>
  <div class="container">
    <!-- Header -->
    <header class="card header-card">
      <div class="header-content">
        <div class="header-icon">
          <svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 14.899A7 7 0 1 1 15.71 8h1.79a4.5 4.5 0 0 1 2.5 8.242"/><path d="M12 12v9"/><path d="m16 16-4-4-4 4"/></svg>
        </div>
        <div>
          <h1>ESP32 OTA Server (Laragon / PHP)</h1>
          <p class="subtitle">Pusat Distribusi Firmware Over-The-Air & Monitoring Real-Time</p>
        </div>
      </div>
      <div class="header-meta">
        <div class="badge badge-success">
          <span class="pulse-dot"></span>
          <span>LARAGON ACTIVE</span>
        </div>
      </div>
    </header>

    <!-- Dynamic Settings Bar -->
    <section class="card settings-bar">
      <div class="settings-group">
        <label class="switch-container">
          <input type="checkbox" id="toggle-autodeploy" checked>
          <span class="switch-slider"></span>
          <span class="switch-label"><strong>Mode Auto-Deploy</strong> (Otomatis perbarui saat versi baru diunggah)</span>
        </label>
      </div>
      <div class="settings-group">
        <label for="poll-interval-select" class="form-label" style="margin-bottom:0">Interval Cek ESP32:</label>
        <select id="poll-interval-select" class="form-input" style="width: auto; padding: 6px 12px;">
          <option value="10">10 Detik (Cepat)</option>
          <option value="15" selected>15 Detik (Optimal)</option>
          <option value="30">30 Detik</option>
          <option value="60">60 Detik (Hemat Bandwidth)</option>
        </select>
      </div>
    </section>

    <!-- Main Grid -->
    <div class="main-grid">
      <!-- Left Column: Upload Center -->
      <section class="card upload-section">
        <h2 class="section-title">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
          Upload Firmware Baru (.bin)
        </h2>

        <form id="upload-form">
          <!-- Tipe Binary -->
          <div class="form-group">
            <label class="form-label">Tipe Binary:</label>
            <div class="segmented-control">
              <label class="segment-option">
                <input type="radio" name="ota_type" value="firmware" checked>
                <span>&#128187; Firmware (Flash App)</span>
              </label>
              <label class="segment-option">
                <input type="radio" name="ota_type" value="spiffs">
                <span>&#128193; Filesystem (SPIFFS)</span>
              </label>
            </div>
          </div>

          <!-- Nomor Versi Baru -->
          <div class="form-group">
            <label class="form-label" for="version-input">Target Nomor Versi:</label>
            <input type="text" id="version-input" class="form-input" placeholder="contoh: 1.0.1" required>
            <small class="form-hint">ESP32 akan secara dinamis menyelaraskan versinya dengan versi ini setelah ter-update.</small>
          </div>

          <!-- Filter Target MAC (Opsional) -->
          <div class="form-group">
            <label class="form-label" for="mac-input">Target MAC Address (Opsional):</label>
            <input type="text" id="mac-input" class="form-input" placeholder="Biarkan kosong untuk SEMUA ESP32 (atau misal: 549738124B00)">
          </div>

          <!-- Changelog -->
          <div class="form-group">
            <label class="form-label" for="changelog-input">Catatan Rilis (Changelog):</label>
            <textarea id="changelog-input" class="form-input textarea" rows="2" placeholder="Catatan perubahan fitur, fix bug, dll..."></textarea>
          </div>

          <!-- Drag and Drop Dropzone -->
          <div class="form-group">
            <label class="form-label">File Binary (.bin):</label>
            <div id="dropzone" class="dropzone">
              <div class="dropzone-icon">&#9729;&#65039;</div>
              <div class="dropzone-text">Tarik & letakkan file <strong>.bin</strong> di sini</div>
              <div class="dropzone-hint">atau klik untuk memilih file dari komputer</div>
              <input type="file" id="binary-file" accept=".bin" style="display:none">
            </div>

            <!-- Selected File Info -->
            <div id="file-info" class="file-info-box" style="display:none">
              <div class="file-meta">
                <span id="file-name" class="file-name-text">firmware.bin</span>
                <span id="file-size" class="file-size-text">0 KB</span>
              </div>
              <button type="button" id="btn-cancel-file" class="btn-sm btn-ghost">Ganti File</button>
            </div>
          </div>

          <!-- Progress Bar -->
          <div id="progress-container" class="progress-box" style="display:none">
            <div class="progress-header">
              <span id="upload-status-text">Mengunggah ke Server...</span>
              <span id="upload-percent">0%</span>
            </div>
            <div class="progress-track">
              <div id="progress-bar" class="progress-fill"></div>
            </div>
          </div>

          <!-- Alert -->
          <div id="alert-box" class="alert" style="display:none"></div>

          <!-- Submit Button -->
          <button type="submit" id="btn-submit" class="btn btn-primary btn-block" disabled>
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
            Publikasikan Firmware ke Server
          </button>
        </form>
      </section>

      <!-- Right Column: Active Firmware Status & Info -->
      <section class="card-column">
        <!-- Current Active Firmware Card -->
        <div class="card">
          <h2 class="section-title">
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>
            Firmware Aktif di Server
          </h2>

          <div class="active-fw-container">
            <div class="fw-item">
              <div class="fw-header">
                <div>
                  <span class="badge badge-indigo">FIRMWARE FLASH</span>
                  <h3 id="srv-fw-ver" class="fw-version">v1.0.0</h3>
                </div>
                <button type="button" id="btn-delete-fw" class="btn-icon btn-danger-icon" title="Hapus Firmware">
                  <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg>
                </button>
              </div>
              <div class="fw-details">
                <p><strong>File:</strong> <span id="srv-fw-file">-</span></p>
                <p><strong>Ukuran:</strong> <span id="srv-fw-size">-</span></p>
                <p><strong>MD5 Hash:</strong> <code id="srv-fw-md5" style="color:var(--accent-primary);font-size:0.8rem">-</code></p>
                <p><strong>Tanggal Upload:</strong> <span id="srv-fw-date">-</span></p>
                <p><strong>Target MAC:</strong> <span id="srv-fw-mac">Semua Device (Global)</span></p>
                <p><strong>Changelog:</strong> <span id="srv-fw-changelog">-</span></p>
              </div>
            </div>
          </div>
        </div>

        <!-- Integration Help Card -->
        <div class="card">
          <h2 class="section-title">
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="16 18 22 12 16 6"/><polyline points="8 6 2 12 8 18"/></svg>
            URL Host untuk Sketch ESP32
          </h2>
          <div class="endpoint-box">
            <div class="endpoint-label">Masukkan URL ini di tab <code>config.h</code> sketch ESP32:</div>
            <code class="code-block" style="font-weight:700"><?php echo $fullLocalUrl; ?></code>
          </div>
          <div class="endpoint-box">
            <div class="endpoint-label">Endpoint API Lengkap:</div>
            <code class="code-block" style="font-size:0.75rem"><?php echo $fullLocalUrl; ?>/api.php?action=check</code>
          </div>
        </div>
      </section>
    </div>

    <!-- Bottom Section: Device Telemetry & Heartbeat Monitor -->
    <section class="card">
      <div class="section-header-flex">
        <h2 class="section-title" style="margin-bottom:0">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="2" width="20" height="8" rx="2" ry="2"/><rect x="2" y="14" width="20" height="8" rx="2" ry="2"/><line x1="6" y1="6" x2="6.01" y2="6"/><line x1="6" y1="18" x2="6.01" y2="18"/></svg>
          Daftar ESP32 Terhubung (Live Heartbeat & Kontrol Aksi)
        </h2>
        <span class="refresh-indicator" id="refresh-badge">Pembaruan otomatis tiap 3s</span>
      </div>

      <div class="table-responsive">
        <table class="device-table">
          <thead>
            <tr>
              <th>Status</th>
              <th>MAC Address</th>
              <th>IP Address</th>
              <th>Versi Terpasang</th>
              <th>Free Heap</th>
              <th>WiFi RSSI</th>
              <th>Partisi</th>
              <th>Uptime</th>
              <th>Terakhir Terlihat</th>
              <th>Aksi Manual</th>
            </tr>
          </thead>
          <tbody id="device-table-body">
            <tr>
              <td colspan="10" class="empty-state">Menunggu ESP32 mengirimkan heartbeat pertama...</td>
            </tr>
          </tbody>
        </table>
      </div>
    </section>
  </div>

  <script src="script.js"></script>
</body>
</html>
