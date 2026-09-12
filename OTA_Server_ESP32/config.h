#ifndef CONFIG_H
#define CONFIG_H

// ==============================================================================
// 1. PENGATURAN KONEKSI WIFI
// ==============================================================================
// Ganti dengan nama WiFi (SSID) dan Password router / hotspot Anda
#define WIFI_SSID     "NAMA_WIFI_ANDA"
#define WIFI_PASSWORD "PASSWORD_WIFI_ANDA"

// Hostname mDNS (Anda dapat mengakses web via http://esp32-ota.local)
#define DEVICE_HOSTNAME "esp32-ota"

// Konfigurasi Fallback Access Point (AP) jika gagal terhubung ke WiFi utama
#define AP_SSID       "ESP32-OTA-Setup"
#define AP_PASSWORD   "12345678"

// ==============================================================================
// 2. KREDENSIAL KEAMANAN WEB (LOGIN)
// ==============================================================================
// Username & Password untuk mengakses Dashboard Web dan melakukan Update OTA
#define WWW_USER      "admin"
#define WWW_PASS      "admin123"

// ==============================================================================
// 3. INFORMASI FIRMWARE & PERANGKAT
// ==============================================================================
#define FIRMWARE_NAME       "ESP32 Web Server OTA"
#define FIRMWARE_VERSION    "1.0.0"
#define FIRMWARE_AUTHOR     "Mahra"
#define FIRMWARE_CHANGELOG  "Initial Release with Dual OTA (Flash & SPIFFS), Live Metrics & Auth"

// Port Web Server (Default: 80 untuk HTTP)
#define WEB_SERVER_PORT     80

#endif // CONFIG_H
