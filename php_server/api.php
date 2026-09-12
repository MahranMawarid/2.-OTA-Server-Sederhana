<?php
/**
 * ESP32 OTA Management Server Backend (PHP Native untuk Laragon / Apache / Nginx)
 * Author : Mahra
 * Year   : 2026
 */

header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$uploadsDir = __DIR__ . '/uploads';
$dataDir = __DIR__ . '/data';
$configFile = $dataDir . '/ota_config.json';
$devicesFile = $dataDir . '/devices.json';
$settingsFile = $dataDir . '/settings.json';

// Inisialisasi folder jika belum ada
if (!file_exists($uploadsDir)) mkdir($uploadsDir, 0777, true);
if (!file_exists($dataDir)) mkdir($dataDir, 0777, true);

// Helper membaca JSON
function readJson($filePath, $default = []) {
    if (file_exists($filePath)) {
        $content = file_get_contents($filePath);
        $data = json_decode($content, true);
        if (json_last_error() === JSON_ERROR_NONE) return $data;
    }
    return $default;
}

// Helper menyimpan JSON
function saveJson($filePath, $data) {
    file_put_contents($filePath, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
}

// Helper perbandingan versi SemVer
function isNewerVersion($serverVer, $clientVer) {
    if (empty($serverVer) || empty($clientVer)) return false;
    $s = ltrim($serverVer, 'vV');
    $c = ltrim($clientVer, 'vV');
    return version_compare($s, $c, '>');
}

// Baca konfigurasi dasar
$config = readJson($configFile, [
    'firmware' => ['version' => '1.0.0', 'filename' => '', 'originalName' => '', 'size' => 0, 'md5' => '', 'uploadDate' => '', 'changelog' => '', 'targetMac' => ''],
    'spiffs'   => ['version' => '1.0.0', 'filename' => '', 'originalName' => '', 'size' => 0, 'md5' => '', 'uploadDate' => '', 'changelog' => '']
]);

$settings = readJson($settingsFile, [
    'autoDeploy' => true,
    'pollIntervalSec' => 15
]);

// Helper URL dasar server
$protocol = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off' || $_SERVER['SERVER_PORT'] == 443) ? "https://" : "http://";
$baseUrl = $protocol . $_SERVER['HTTP_HOST'] . dirname($_SERVER['SCRIPT_NAME']);
$baseUrl = rtrim($baseUrl, '/\\');

$action = $_GET['action'] ?? '';

// ==============================================================================
// 1. ACTION: CONFIG (STATUS FIRMWARE & SETTINGS)
// ==============================================================================
if ($action === 'config') {
    header('Content-Type: application/json');
    echo json_encode(array_merge($config, ['settings' => $settings]));
    exit;
}

// ==============================================================================
// 2. ACTION: SETTINGS (UBAH PENGATURAN DARI WEB)
// ==============================================================================
if ($action === 'settings' && $_SERVER['REQUEST_METHOD'] === 'POST') {
    $input = json_decode(file_get_contents('php://input'), true);
    if (isset($input['autoDeploy'])) $settings['autoDeploy'] = (bool)$input['autoDeploy'];
    if (isset($input['pollIntervalSec'])) $settings['pollIntervalSec'] = max(5, intval($input['pollIntervalSec']));
    saveJson($settingsFile, $settings);
    header('Content-Type: application/json');
    echo json_encode(['success' => true, 'settings' => $settings]);
    exit;
}

// ==============================================================================
// 3. ACTION: UPLOAD FIRMWARE / SPIFFS (.BIN)
// ==============================================================================
if ($action === 'upload' && $_SERVER['REQUEST_METHOD'] === 'POST') {
    header('Content-Type: application/json');

    if (!isset($_FILES['binary']) || $_FILES['binary']['error'] !== UPLOAD_ERR_OK) {
        http_response_code(400);
        echo json_encode(['success' => false, 'message' => 'Tidak ada file binary yang diunggah atau terjadi error.']);
        exit;
    }

    $type = ($_POST['type'] ?? 'firmware') === 'spiffs' ? 'spiffs' : 'firmware';
    $version = trim($_POST['version'] ?? '1.0.0');
    $changelog = trim($_POST['changelog'] ?? '');
    $targetMac = strtoupper(trim($_POST['targetMac'] ?? ''));

    $originalName = $_FILES['binary']['name'];
    $ext = strtolower(pathinfo($originalName, PATHINFO_EXTENSION));

    if ($ext !== 'bin') {
        http_response_code(400);
        echo json_encode(['success' => false, 'message' => 'Hanya file dengan ekstensi .bin yang diperbolehkan!']);
        exit;
    }

    // Hapus file lama
    if (!empty($config[$type]['filename'])) {
        $oldFile = $uploadsDir . '/' . $config[$type]['filename'];
        if (file_exists($oldFile)) @unlink($oldFile);
    }

    $newFilename = $type . '_' . time() . '.bin';
    $targetPath = $uploadsDir . '/' . $newFilename;

    if (move_uploaded_file($_FILES['binary']['tmp_name'], $targetPath)) {
        $fileSize = filesize($targetPath);
        $fileMD5 = md5_file($targetPath);

        $config[$type] = [
            'version'      => $version,
            'filename'     => $newFilename,
            'originalName' => $originalName,
            'size'         => $fileSize,
            'md5'          => $fileMD5,
            'uploadDate'   => date('c'),
            'changelog'    => $changelog,
            'targetMac'    => $targetMac
        ];

        saveJson($configFile, $config);

        echo json_encode([
            'success' => true,
            'message' => "File " . strtoupper($type) . " v{$version} berhasil diunggah!",
            'data'    => $config[$type]
        ]);
    } else {
        http_response_code(500);
        echo json_encode(['success' => false, 'message' => 'Gagal memindahkan file yang diunggah ke folder uploads.']);
    }
    exit;
}

// ==============================================================================
// 4. ACTION: CHECK (DIPANGGIL OLEH ESP32 UNTUK CEK UPDATE)
// ==============================================================================
if ($action === 'check') {
    header('Content-Type: application/json');

    $clientVer = $_GET['version'] ?? '0.0.0';
    $clientMac = strtoupper($_GET['mac'] ?? '');
    $clientMD5 = strtolower($_GET['md5'] ?? '');
    $type      = ($_GET['type'] ?? 'firmware') === 'spiffs' ? 'spiffs' : 'firmware';

    $targetConfig = $config[$type] ?? null;

    if (!$targetConfig || empty($targetConfig['filename'])) {
        echo json_encode([
            'update_available'  => false,
            'poll_interval_sec' => $settings['pollIntervalSec'],
            'message'           => 'Belum ada file binary di server.'
        ]);
        exit;
    }

    $filePath = $uploadsDir . '/' . $targetConfig['filename'];
    if (!file_exists($filePath)) {
        echo json_encode([
            'update_available'  => false,
            'poll_interval_sec' => $settings['pollIntervalSec'],
            'message'           => 'File binary tidak ditemukan di server.'
        ]);
        exit;
    }

    // Cek filter MAC
    if (!empty($targetConfig['targetMac']) && $targetConfig['targetMac'] !== $clientMac) {
        echo json_encode([
            'update_available'  => false,
            'poll_interval_sec' => $settings['pollIntervalSec'],
            'message'           => 'Firmware ini dikhususkan untuk MAC: ' . $targetConfig['targetMac']
        ]);
        exit;
    }

    // Proteksi Anti-Looping MD5
    if (!empty($clientMD5) && !empty($targetConfig['md5']) && $clientMD5 === strtolower($targetConfig['md5'])) {
        echo json_encode([
            'update_available'  => false,
            'current_client_version' => $clientVer,
            'server_version'    => $targetConfig['version'],
            'md5'               => $targetConfig['md5'],
            'poll_interval_sec' => $settings['pollIntervalSec'],
            'message'           => 'Firmware ESP32 sudah identik (MD5 Hash cocok).'
        ]);
        exit;
    }

    // Cek apakah ada antrean manual trigger
    $devices = readJson($devicesFile, []);
    $hasManualTrigger = !empty($devices[$clientMac]['hasPendingOta']);
    if ($hasManualTrigger) {
        $devices[$clientMac]['hasPendingOta'] = false;
        saveJson($devicesFile, $devices);
    }

    $hasNewer = isNewerVersion($targetConfig['version'], $clientVer);
    $shouldUpdate = ($settings['autoDeploy'] && $hasNewer) || $hasManualTrigger;

    if ($shouldUpdate) {
        $downloadUrl = $baseUrl . '/api.php?action=download&type=' . $type;
        echo json_encode([
            'update_available'       => true,
            'current_client_version' => $clientVer,
            'new_version'            => $targetConfig['version'],
            'filename'               => $targetConfig['originalName'],
            'size'                   => $targetConfig['size'],
            'md5'                    => $targetConfig['md5'],
            'download_url'           => $downloadUrl,
            'changelog'              => $targetConfig['changelog'],
            'poll_interval_sec'      => $settings['pollIntervalSec']
        ]);
    } else {
        echo json_encode([
            'update_available'       => false,
            'current_client_version' => $clientVer,
            'server_version'         => $targetConfig['version'],
            'md5'                    => $targetConfig['md5'],
            'poll_interval_sec'      => $settings['pollIntervalSec'],
            'message'                => 'Firmware sudah menggunakan versi terbaru.'
        ]);
    }
    exit;
}

// ==============================================================================
// 5. ACTION: DOWNLOAD (ESP32 MENDOWNLOAD STREAM .BIN)
// ==============================================================================
if ($action === 'download') {
    $type = ($_GET['type'] ?? 'firmware') === 'spiffs' ? 'spiffs' : 'firmware';
    $targetConfig = $config[$type] ?? null;

    if (!$targetConfig || empty($targetConfig['filename'])) {
        http_response_code(404);
        exit('File binary belum diunggah.');
    }

    $filePath = $uploadsDir . '/' . $targetConfig['filename'];
    if (!file_exists($filePath)) {
        http_response_code(404);
        exit('File binary tidak ditemukan.');
    }

    $fileSize = filesize($filePath);
    $fileMD5 = $targetConfig['md5'] ?: md5_file($filePath);

    header('Content-Type: application/octet-stream');
    header('Content-Disposition: attachment; filename="' . $targetConfig['filename'] . '"');
    header('Content-Length: ' . $fileSize);
    header('x-MD5: ' . $fileMD5);
    header('ETag: "' . $fileMD5 . '"');

    readfile($filePath);
    exit;
}

// ==============================================================================
// 6. ACTION: HEARTBEAT (ESP32 MELAPOR STATUS TELEMETRI)
// ==============================================================================
if ($action === 'heartbeat' && $_SERVER['REQUEST_METHOD'] === 'POST') {
    header('Content-Type: application/json');
    $input = json_decode(file_get_contents('php://input'), true);

    if (empty($input['mac'])) {
        http_response_code(400);
        echo json_encode(['error' => 'MAC address wajib disertakan.']);
        exit;
    }

    $mac = strtoupper($input['mac']);
    $devices = readJson($devicesFile, []);
    $hasPendingOta = !empty($devices[$mac]['hasPendingOta']);

    $devices[$mac] = [
        'mac'           => $mac,
        'ip'            => $input['ip'] ?? $_SERVER['REMOTE_ADDR'],
        'version'       => $input['version'] ?? '1.0.0',
        'md5'           => $input['md5'] ?? '',
        'free_heap'     => $input['free_heap'] ?? 0,
        'rssi'          => $input['rssi'] ?? 0,
        'uptime'        => $input['uptime'] ?? 0,
        'partition'     => $input['partition'] ?? 'app0',
        'status'        => $input['status'] ?? 'idle',
        'hasPendingOta' => $hasPendingOta,
        'lastSeen'      => date('c')
    ];

    saveJson($devicesFile, $devices);

    echo json_encode([
        'success'           => true,
        'poll_interval_sec' => $settings['pollIntervalSec'],
        'has_pending_ota'   => $hasPendingOta
    ]);
    exit;
}

// ==============================================================================
// 7. ACTION: DEVICES (UNTUK TABEL MONITOR DASHBOARD)
// ==============================================================================
if ($action === 'devices') {
    header('Content-Type: application/json');
    $devices = readJson($devicesFile, []);
    $now = time();
    $list = [];

    foreach ($devices as $mac => $dev) {
        $lastSeenTime = strtotime($dev['lastSeen']);
        $diffSec = $now - $lastSeenTime;
        $isOnline = $diffSec < 60;

        $list[] = array_merge($dev, [
            'isOnline'           => $isOnline,
            'lastSeenSecondsAgo' => $diffSec
        ]);
    }

    echo json_encode($list);
    exit;
}

// ==============================================================================
// 8. ACTION: TRIGGER_OTA (MANUAL DEPLOY DARI WEB)
// ==============================================================================
if ($action === 'trigger_ota' && $_SERVER['REQUEST_METHOD'] === 'POST') {
    header('Content-Type: application/json');
    $input = json_decode(file_get_contents('php://input'), true);
    $mac = strtoupper($input['mac'] ?? '');

    if (empty($mac)) {
        http_response_code(400);
        echo json_encode(['success' => false, 'message' => 'MAC address wajib diisi.']);
        exit;
    }

    $devices = readJson($devicesFile, []);
    if (!isset($devices[$mac])) {
        $devices[$mac] = ['mac' => $mac];
    }
    $devices[$mac]['hasPendingOta'] = true;
    saveJson($devicesFile, $devices);

    echo json_encode(['success' => true, 'message' => "Perintah OTA untuk device {$mac} telah diantrikan!"]);
    exit;
}

// ==============================================================================
// 9. ACTION: DELETE_FIRMWARE
// ==============================================================================
if ($action === 'delete_firmware') {
    header('Content-Type: application/json');
    $type = ($_GET['type'] ?? 'firmware') === 'spiffs' ? 'spiffs' : 'firmware';

    if (!empty($config[$type]['filename'])) {
        $filePath = $uploadsDir . '/' . $config[$type]['filename'];
        if (file_exists($filePath)) @unlink($filePath);
    }

    $config[$type] = ['version' => '1.0.0', 'filename' => '', 'originalName' => '', 'size' => 0, 'md5' => '', 'uploadDate' => '', 'changelog' => '', 'targetMac' => ''];
    saveJson($configFile, $config);

    echo json_encode(['success' => true, 'message' => "Firmware " . strtoupper($type) . " berhasil dihapus."]);
    exit;
}

// Fallback 404
http_response_code(404);
echo json_encode(['error' => 'Action tidak dikenali pada API OTA.']);
