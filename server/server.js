const express = require('express');
const cors = require('cors');
const multer = require('multer');
const path = require('path');
const fs = require('fs');
const os = require('os');
const crypto = require('crypto');

const app = express();
const PORT = process.env.PORT || 3000;

// Path direktori
const UPLOADS_DIR = path.join(__dirname, 'uploads');
const DATA_FILE = path.join(__dirname, 'data', 'ota_config.json');

// Inisialisasi direktori uploads jika belum ada
if (!fs.existsSync(UPLOADS_DIR)) {
  fs.mkdirSync(UPLOADS_DIR, { recursive: true });
}

// Memory store untuk melacak ESP32 yang terhubung (Heartbeat)
const connectedDevices = new Map();

// Helper untuk menghitung MD5 file
function getFileMD5(filePath) {
  try {
    if (fs.existsSync(filePath)) {
      const fileBuffer = fs.readFileSync(filePath);
      return crypto.createHash('md5').update(fileBuffer).digest('hex');
    }
  } catch (err) {
    console.error('Error calculating MD5:', err);
  }
  return '';
}

// Server Settings
let serverSettings = {
  autoDeploy: true,          // Jika true, ESP32 yang versinya lebih lama langsung di-update
  pollIntervalSec: 15        // Interval polling ESP32 dinamis (detik)
};

// Antrian perintah OTA per-device (MAC -> { command: 'ota_update', timestamp })
const pendingDeviceCommands = new Map();

// Helper untuk membaca metadata config
function readConfig() {
  try {
    if (fs.existsSync(DATA_FILE)) {
      return JSON.parse(fs.readFileSync(DATA_FILE, 'utf8'));
    }
  } catch (err) {
    console.error('Error reading config file:', err);
  }
  return {
    firmware: { version: "1.0.0", filename: "", originalName: "", size: 0, md5: "", uploadDate: "", changelog: "", targetMac: "" },
    spiffs: { version: "1.0.0", filename: "", originalName: "", size: 0, md5: "", uploadDate: "", changelog: "" }
  };
}

// Helper untuk menyimpan metadata config
function saveConfig(config) {
  try {
    fs.writeFileSync(DATA_FILE, JSON.stringify(config, null, 2), 'utf8');
    return true;
  } catch (err) {
    console.error('Error saving config file:', err);
    return false;
  }
}

// Helper perbandingan versi (Semantic Versioning: misal '1.0.1' > '1.0.0')
function isNewerVersion(serverVer, clientVer) {
  if (!serverVer || !clientVer) return false;
  if (serverVer === clientVer) return false;

  const parse = (v) => v.replace(/^v/i, '').split('.').map(n => parseInt(n, 10) || 0);
  const s = parse(serverVer);
  const c = parse(clientVer);

  for (let i = 0; i < Math.max(s.length, c.length); i++) {
    const sv = s[i] || 0;
    const cv = c[i] || 0;
    if (sv > cv) return true;
    if (sv < cv) return false;
  }
  return false;
}

// Konfigurasi Multer untuk Upload Binary (.bin)
const storage = multer.diskStorage({
  destination: (req, file, cb) => cb(null, UPLOADS_DIR),
  filename: (req, file, cb) => {
    const type = req.body.type || 'firmware';
    const timestamp = Date.now();
    cb(null, `${type}_${timestamp}.bin`);
  }
});

const upload = multer({
  storage: storage,
  limits: { fileSize: 4 * 1024 * 1024 }, // Maksimal 4 MB (sesuai flash ESP32)
  fileFilter: (req, file, cb) => {
    if (path.extname(file.originalname).toLowerCase() === '.bin') {
      cb(null, true);
    } else {
      cb(new Error('Hanya file dengan format .bin yang diperbolehkan!'));
    }
  }
});

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.urlencoded({ extended: true }));
app.use(express.static(path.join(__dirname, 'public')));

// ==============================================================================
// 1. API: STATUS & CONFIG
// ==============================================================================
app.get('/api/config', (req, res) => {
  const config = readConfig();
  res.json({
    ...config,
    settings: serverSettings
  });
});

app.post('/api/settings', (req, res) => {
  if (req.body.autoDeploy !== undefined) {
    serverSettings.autoDeploy = Boolean(req.body.autoDeploy);
  }
  if (req.body.pollIntervalSec !== undefined) {
    serverSettings.pollIntervalSec = Math.max(5, parseInt(req.body.pollIntervalSec, 10) || 15);
  }
  res.json({ success: true, settings: serverSettings });
});

// ==============================================================================
// 2. API: TRIGGER MANUAL OTA PER DEVICE DARI DASHBOARD
// ==============================================================================
app.post('/api/devices/trigger-ota', (req, res) => {
  const mac = (req.body.mac || '').toUpperCase();
  if (!mac) {
    return res.status(400).json({ success: false, message: 'MAC Address wajib diisi.' });
  }

  const config = readConfig();
  if (!config.firmware || !config.firmware.filename) {
    return res.status(400).json({ success: false, message: 'Belum ada firmware yang diunggah ke server.' });
  }

  pendingDeviceCommands.set(mac, {
    command: 'ota_update',
    type: req.body.type || 'firmware',
    timestamp: Date.now()
  });

  console.log(`[MANUAL TRIGGER] Perintah OTA dijadwalkan untuk ESP32 MAC=${mac}`);
  res.json({ success: true, message: `Perintah update untuk device ${mac} telah diantrikan!` });
});

// ==============================================================================
// 3. API: UPLOAD FIRMWARE / SPIFFS (.BIN)
// ==============================================================================
app.post('/api/upload', upload.single('binary'), (req, res) => {
  if (!req.file) {
    return res.status(400).json({ success: false, message: 'Tidak ada file yang diunggah atau ekstensi bukan .bin' });
  }

  const type = req.body.type === 'spiffs' ? 'spiffs' : 'firmware';
  const newVersion = req.body.version ? req.body.version.trim() : '1.0.0';
  const changelog = req.body.changelog ? req.body.changelog.trim() : '';
  const targetMac = req.body.targetMac ? req.body.targetMac.trim().toUpperCase() : '';

  const config = readConfig();

  // Hapus file lama jika ada
  if (config[type] && config[type].filename) {
    const oldPath = path.join(UPLOADS_DIR, config[type].filename);
    if (fs.existsSync(oldPath)) {
      try { fs.unlinkSync(oldPath); } catch (e) {}
    }
  }

  // Hitung MD5 hash dari file yang baru diunggah
  const filePath = path.join(UPLOADS_DIR, req.file.filename);
  const fileMD5 = getFileMD5(filePath);

  // Update data config
  config[type] = {
    version: newVersion,
    filename: req.file.filename,
    originalName: req.file.originalname,
    size: req.file.size,
    md5: fileMD5,
    uploadDate: new Date().toISOString(),
    changelog: changelog,
    targetMac: targetMac
  };

  saveConfig(config);

  console.log(`[UPLOAD] Sukses mengunggah ${type.toUpperCase()} v${newVersion} (${(req.file.size / 1024).toFixed(1)} KB) | MD5: ${fileMD5}`);

  res.json({
    success: true,
    message: `File ${type.toUpperCase()} v${newVersion} berhasil diunggah!`,
    data: config[type]
  });
});

// ==============================================================================
// 4. API: OTA CHECK (Dipanggil oleh ESP32 secara berkala)
// ==============================================================================
app.get('/api/ota/check', (req, res) => {
  const clientVersion = req.query.version || '0.0.0';
  const clientMac = (req.query.mac || '').toUpperCase();
  const clientMD5 = (req.query.md5 || '').toLowerCase();
  const type = req.query.type === 'spiffs' ? 'spiffs' : 'firmware';

  const config = readConfig();
  const targetConfig = config[type];

  // Pastikan ada file binary tersedia
  if (!targetConfig || !targetConfig.filename) {
    return res.json({
      update_available: false,
      poll_interval_sec: serverSettings.pollIntervalSec,
      message: 'Belum ada file binary yang diunggah ke server.'
    });
  }

  const filePath = path.join(UPLOADS_DIR, targetConfig.filename);
  if (!fs.existsSync(filePath)) {
    return res.json({
      update_available: false,
      poll_interval_sec: serverSettings.pollIntervalSec,
      message: 'File binary tidak ditemukan pada penyimpanan server.'
    });
  }

  if (!targetConfig.md5) {
    targetConfig.md5 = getFileMD5(filePath);
    saveConfig(config);
  }

  // Cek apakah ada filter MAC address
  if (targetConfig.targetMac && targetConfig.targetMac !== '') {
    if (clientMac !== targetConfig.targetMac) {
      return res.json({
        update_available: false,
        poll_interval_sec: serverSettings.pollIntervalSec,
        message: `Firmware ini dikhususkan untuk MAC: ${targetConfig.targetMac}. MAC Anda: ${clientMac}`
      });
    }
  }

  // Cek apakah ESP32 sudah memiliki binary dengan MD5 yang sama persis
  if (clientMD5 && targetConfig.md5 && clientMD5 === targetConfig.md5.toLowerCase()) {
    return res.json({
      update_available: false,
      current_client_version: clientVersion,
      server_version: targetConfig.version,
      md5: targetConfig.md5,
      poll_interval_sec: serverSettings.pollIntervalSec,
      message: 'Firmware pada ESP32 sudah identik (MD5 Checksum sama).'
    });
  }

  // Cek apakah ada manual trigger untuk device ini
  const hasManualTrigger = pendingDeviceCommands.has(clientMac);
  if (hasManualTrigger) {
    pendingDeviceCommands.delete(clientMac); // Clear trigger setelah dikirim
  }

  // Cek versi baru jika autoDeploy aktif ATAU jika ada manual trigger
  const hasNewerVersion = isNewerVersion(targetConfig.version, clientVersion);
  const shouldUpdate = (serverSettings.autoDeploy && hasNewerVersion) || hasManualTrigger;

  if (shouldUpdate) {
    const protocol = req.protocol;
    const host = req.get('host');
    const downloadUrl = `${protocol}://${host}/api/ota/download/${type}`;

    console.log(`[OTA CHECK] ESP32 MAC=${clientMac || 'UNKNOWN'} (v${clientVersion}) -> Siap Update: v${targetConfig.version} (Trigger: ${hasManualTrigger ? 'MANUAL' : 'AUTO'})`);

    return res.json({
      update_available: true,
      current_client_version: clientVersion,
      new_version: targetConfig.version,
      filename: targetConfig.originalName,
      size: targetConfig.size,
      md5: targetConfig.md5,
      download_url: downloadUrl,
      changelog: targetConfig.changelog,
      poll_interval_sec: serverSettings.pollIntervalSec
    });
  } else {
    return res.json({
      update_available: false,
      current_client_version: clientVersion,
      server_version: targetConfig.version,
      md5: targetConfig.md5,
      poll_interval_sec: serverSettings.pollIntervalSec,
      message: 'Firmware ESP32 Anda sudah menggunakan versi terbaru.'
    });
  }
});

// ==============================================================================
// 5. API: OTA DOWNLOAD (ESP32 mendownload stream file .bin)
// ==============================================================================
app.get('/api/ota/download/:type', (req, res) => {
  const type = req.params.type === 'spiffs' ? 'spiffs' : 'firmware';
  const config = readConfig();
  const targetConfig = config[type];

  if (!targetConfig || !targetConfig.filename) {
    return res.status(404).send('File binary belum diunggah.');
  }

  const filePath = path.join(UPLOADS_DIR, targetConfig.filename);
  if (!fs.existsSync(filePath)) {
    return res.status(404).send('File binary tidak ditemukan di server.');
  }

  const fileMD5 = targetConfig.md5 || getFileMD5(filePath);

  console.log(`[OTA STREAM] Mengirim binary ${type.toUpperCase()} ke client... (MD5: ${fileMD5})`);
  res.setHeader('Content-Type', 'application/octet-stream');
  res.setHeader('Content-Disposition', `attachment; filename="${targetConfig.filename}"`);
  res.setHeader('Content-Length', targetConfig.size);
  res.setHeader('x-MD5', fileMD5);
  res.setHeader('ETag', `"${fileMD5}"`);

  const fileStream = fs.createReadStream(filePath);
  fileStream.pipe(res);
});

// ==============================================================================
// 6. API: HEARTBEAT / DEVICE STATUS (ESP32 melapor status)
// ==============================================================================
app.post('/api/ota/heartbeat', (req, res) => {
  const { mac, ip, version, md5, free_heap, rssi, uptime, partition, status } = req.body;

  if (!mac) {
    return res.status(400).json({ error: 'MAC address wajib disertakan.' });
  }

  const formattedMac = mac.toUpperCase();
  const hasPendingOta = pendingDeviceCommands.has(formattedMac);

  const deviceData = {
    mac: formattedMac,
    ip: ip || req.ip,
    version: version || '1.0.0',
    md5: md5 || '',
    free_heap: free_heap || 0,
    rssi: rssi || 0,
    uptime: uptime || 0,
    partition: partition || 'app0',
    status: status || 'idle',
    hasPendingOta: hasPendingOta,
    lastSeen: new Date().toISOString()
  };

  connectedDevices.set(formattedMac, deviceData);

  // Respons ke ESP32 dengan instruksi dinamis
  res.json({
    success: true,
    poll_interval_sec: serverSettings.pollIntervalSec,
    has_pending_ota: hasPendingOta
  });
});

// ==============================================================================
// 7. API: DAFTAR PERANGKAT AKTIF (Untuk Dashboard)
// ==============================================================================
app.get('/api/devices', (req, res) => {
  const now = Date.now();
  const devices = [];

  for (const [mac, dev] of connectedDevices.entries()) {
    const lastSeenTime = new Date(dev.lastSeen).getTime();
    const diffSec = Math.floor((now - lastSeenTime) / 1000);
    const isOnline = diffSec < 60; // Online jika ada heartbeat dalam 60 detik terakhir

    devices.push({
      ...dev,
      isOnline: isOnline,
      lastSeenSecondsAgo: diffSec
    });
  }

  res.json(devices);
});

// ==============================================================================
// 7. API: HAPUS FIRMWARE
// ==============================================================================
app.delete('/api/firmware/:type', (req, res) => {
  const type = req.params.type === 'spiffs' ? 'spiffs' : 'firmware';
  const config = readConfig();

  if (config[type] && config[type].filename) {
    const filePath = path.join(UPLOADS_DIR, config[type].filename);
    if (fs.existsSync(filePath)) {
      try { fs.unlinkSync(filePath); } catch (e) {}
    }
  }

  config[type] = { version: "1.0.0", filename: "", originalName: "", size: 0, uploadDate: "", changelog: "", targetMac: "" };
  saveConfig(config);

  res.json({ success: true, message: `Firmware ${type.toUpperCase()} berhasil dihapus dari server.` });
});

// Helper untuk menampilkan IP Lokal PC di console
function getLocalIPs() {
  const interfaces = os.networkInterfaces();
  const ips = [];
  for (const name of Object.keys(interfaces)) {
    for (const iface of interfaces[name]) {
      if (iface.family === 'IPv4' && !iface.internal) {
        ips.push(iface.address);
      }
    }
  }
  return ips;
}

// Start Server
app.listen(PORT, '0.0.0.0', () => {
  const localIps = getLocalIPs();
  console.log('\n======================================================');
  console.log('       ESP32 OTA MANAGEMENT WEB SERVER BERJALAN        ');
  console.log('======================================================');
  console.log(`Port Server    : ${PORT}`);
  console.log(`Akses Lokal    : http://localhost:${PORT}`);
  if (localIps.length > 0) {
    console.log('Alamat IP untuk dimasukkan di sketch ESP32:');
    localIps.forEach(ip => {
      console.log(`  -> http://${ip}:${PORT}`);
    });
  }
  console.log('======================================================\n');
});
