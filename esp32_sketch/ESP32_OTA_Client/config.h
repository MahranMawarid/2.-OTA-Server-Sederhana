#ifndef CONFIG_H
#define CONFIG_H

// ==============================================================================
// 1. PENGATURAN KONEKSI WIFI
// ==============================================================================
// Masukkan nama WiFi (SSID) dan Password router / hotspot Anda
#define WIFI_SSID     "Mahran Mawarid"
#define WIFI_PASSWORD "QD8j-qNiP-Fhzh-4ihK"

// ==============================================================================
// 2. PILIHAN TIPE SERVER & ALAMAT HOST
// ==============================================================================
// Set 'true' jika menggunakan LARAGON (Apache / PHP)
// Set 'false' jika menggunakan Node.js (start_server.bat port 3000)
#define USE_PHP_LARAGON  true

// Contoh Alamat IP Host:
// - Jika menggunakan Laragon : "http://172.20.10.2/ota-server" (atau nama folder di C:\laragon\www\)
// - Jika menggunakan Node.js : "http://172.20.10.2:3000"
#define OTA_SERVER_HOST "http://172.20.10.2/ota-server"

// ==============================================================================
// 3. INFORMASI DASAR FIRMWARE (Hanya Default Awal Flash Pertama)
// ==============================================================================
// CATATAN: Versi akan dikelola SECARA DINAMIS di memori NVS internal ESP32.
// Anda TIDAK PERLU mengubah file ini setiap kali membuat file .bin baru!
#define DEFAULT_FACTORY_VERSION  "1.0.0"
#define FIRMWARE_PROJECT_NAME    "ESP32 Dynamic OTA Node"

// Pin LED status (Default: GPIO 2 - LED bawaan ESP32 Dev Module)
#define LED_PIN 2

#endif // CONFIG_H
