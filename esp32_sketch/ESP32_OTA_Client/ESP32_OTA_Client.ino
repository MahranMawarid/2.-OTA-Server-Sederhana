/*
  ==============================================================================
  PROJECT : ESP32 OTA Auto-Update Client (Dinamis & Anti-Looping)
  AUTHOR  : Mahra
  DATE    : 2026
  BOARD   : ESP32 Dev Module (Xtensa Dual-Core 240MHz, 4MB Flash)
  PARTISI : Dual App (app0: 1280KB, app1: 1280KB, spiffs: 1408KB)
  
  FITUR DINAMIS:
  1. Versi Firmware Otomatis & Dinamis tersimpan di NVS (Preferences) 
     -> Anda TIDAK PERLU mengubah kode config.h setiap kali membuat .bin baru!
  2. Interval polling dapat diatur secara dinamis dari Web Dashboard.
  3. Proteksi MD5 Checksum ganda mencegah loop unduhan berulang.
  4. Mendukung Auto-Deploy maupun Trigger Manual (Tombol Deploy di Web).
  ==============================================================================
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include "config.h"

// Preferences (NVS) untuk menyimpan Versi dan MD5 secara dinamis
Preferences prefs;
String runningFirmwareVersion = "1.0.0";
String currentFlashedMD5 = "";

// Timer interval (Dinamis diselaraskan dengan server)
unsigned long otaCheckIntervalMs = 15000;  // Default 15 detik
unsigned long heartbeatIntervalMs = 5000;  // Lapor status tiap 5 detik
unsigned long lastOtaCheckTime = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastBlinkTime = 0;
bool ledState = false;

// Cooldown setelah boot sebelum pengecekan OTA pertama
bool initialOtaCheckDone = false;

// ==============================================================================
// HELPER FUNCTIONS: IDENTITAS CHIP & PARTISI
// ==============================================================================
String getMacAddressFormatted() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

String getCurrentPartitionLabel() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running != NULL) {
    return String(running->label);
  }
  return "unknown";
}

// Inisialisasi versi dinamis dari NVS Flash
void loadDynamicVersionFromNVS() {
  prefs.begin("ota_mgr", false);
  
  // Baca versi yang tersimpan, jika belum ada gunakan default dari config.h
  if (prefs.isKey("fw_ver")) {
    runningFirmwareVersion = prefs.getString("fw_ver", DEFAULT_FACTORY_VERSION);
  } else {
    runningFirmwareVersion = DEFAULT_FACTORY_VERSION;
    prefs.putString("fw_ver", runningFirmwareVersion);
  }

  // Baca MD5 hash terakhir yang tersimpan
  currentFlashedMD5 = prefs.getString("fw_md5", "");
  
  prefs.end();
}

// Simpan versi baru dan MD5 secara dinamis ke NVS Flash
void saveDynamicVersionToNVS(const String& newVer, const String& newMD5) {
  prefs.begin("ota_mgr", false);
  prefs.putString("fw_ver", newVer);
  if (newMD5.length() > 0) {
    prefs.putString("fw_md5", newMD5);
  }
  prefs.end();
  runningFirmwareVersion = newVer;
  currentFlashedMD5 = newMD5;
}

// ==============================================================================
// HTTPUPDATE CALLBACKS (MONITORING PROSES DOWNLOAD OTA)
// ==============================================================================
void updateStarted() {
  Serial.println("\n[OTA CLIENT] >>> Memulai Download & Flashing ke Partisi Baru...");
  digitalWrite(LED_PIN, HIGH);
}

void updateFinished() {
  Serial.println("\n[OTA CLIENT] >>> Flashing Sukses! Menyimpan versi baru & Rebooting...");
  digitalWrite(LED_PIN, LOW);
}

void updateProgress(int current, int total) {
  static int lastPercent = -1;
  if (total > 0) {
    int percent = (int)(((float)current / (float)total) * 100);
    if (percent % 10 == 0 && percent != lastPercent) {
      Serial.printf("[OTA PROGRESS] Terunduh: %d%% (%d / %d bytes)\n", percent, current, total);
      lastPercent = percent;
    }
  }
}

void updateError(int err) {
  Serial.printf("\n[OTA ERROR] Gagal mengunduh firmware, kode error: %d\n", err);
}

// ==============================================================================
// FUNGSI: KIRIM HEARTBEAT KE WEB SERVER PC (DAN TERIMA INSTRUKSI DINAMIS)
// ==============================================================================
void sendHeartbeatToServer(String statusMessage = "idle") {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;

  String url = String(OTA_SERVER_HOST) + "/api/ota/heartbeat";
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  // Format payload JSON telemetri dengan versi dinamis
  String payload = "{";
  payload += "\"mac\":\"" + getMacAddressFormatted() + "\",";
  payload += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  payload += "\"version\":\"" + runningFirmwareVersion + "\",";
  payload += "\"md5\":\"" + currentFlashedMD5 + "\",";
  payload += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  payload += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  payload += "\"uptime\":" + String(millis() / 1000) + ",";
  payload += "\"partition\":\"" + getCurrentPartitionLabel() + "\",";
  payload += "\"status\":\"" + statusMessage + "\"";
  payload += "}";

  int httpCode = http.POST(payload);
  if (httpCode == HTTP_CODE_OK) {
    String resp = http.getString();
    // Parse interval dinamis dari server
    int intervalIdx = resp.indexOf("\"poll_interval_sec\":");
    if (intervalIdx >= 0) {
      int secVal = resp.substring(intervalIdx + 20).toInt();
      if (secVal >= 5) {
        otaCheckIntervalMs = secVal * 1000UL;
      }
    }

    // Jika server memiliki perintah manual trigger yang tertunda, langsung cek OTA
    if (resp.indexOf("\"has_pending_ota\":true") >= 0) {
      Serial.println("[HEARTBEAT] Server mengirim sinyal pembaruan manual (Deploy Trigger)!");
      checkAndExecuteOtaUpdate();
    }
  }

  http.end();
}

// ==============================================================================
// FUNGSI: CEK & EKSEKUSI OTA UPDATE DINAMIS
// ==============================================================================
void checkAndExecuteOtaUpdate() {
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.println("\n[OTA CHECK] Menghubungi server (Versi Terpasang: v" + runningFirmwareVersion + ")...");
  
  WiFiClient client;
  HTTPClient http;

  // Endpoint: GET /api/ota/check?version=x.x.x&mac=XXXX&md5=YYYY
  String url = String(OTA_SERVER_HOST) + "/api/ota/check?version=" + runningFirmwareVersion 
               + "&mac=" + getMacAddressFormatted() 
               + "&md5=" + currentFlashedMD5;
  
  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();

    // Cek apakah ada update tersedia
    if (response.indexOf("\"update_available\":true") >= 0) {
      // Ambil MD5 dari server jika ada
      String serverMD5 = "";
      int md5Start = response.indexOf("\"md5\":\"");
      if (md5Start >= 0) {
        md5Start += 7;
        int md5End = response.indexOf("\"", md5Start);
        serverMD5 = response.substring(md5Start, md5End);
      }

      // Proteksi Anti-Looping: Jika MD5 sama persis dengan yang sedang berjalan, tolak
      if (serverMD5.length() > 0 && serverMD5 == currentFlashedMD5) {
        Serial.println("[ANTI-LOOP] Binary di server identik dengan yang sedang berjalan (MD5 cocok). Melewati update.");
        http.end();
        return;
      }

      // Ambil download URL
      int urlStart = response.indexOf("\"download_url\":\"") + 16;
      int urlEnd = response.indexOf("\"", urlStart);
      String downloadUrl = response.substring(urlStart, urlEnd);

      // Ambil nomor versi baru yang ditetapkan server
      int verStart = response.indexOf("\"new_version\":\"") + 15;
      int verEnd = response.indexOf("\"", verStart);
      String newVersion = response.substring(verStart, verEnd);

      Serial.println("\n==================================================");
      Serial.printf(">>> PEMBARUAN DINAMIS TERSEDIA: v%s <<<\n", newVersion.c_str());
      Serial.println("==================================================");
      Serial.printf("[OTA] Download URL : %s\n", downloadUrl.c_str());
      if (serverMD5.length() > 0) {
        Serial.printf("[OTA] Binary MD5   : %s\n", serverMD5.c_str());
      }
      
      http.end(); // Tutup koneksi check sebelum streaming download

      // Simpan Versi Baru dan MD5 ke NVS Flash SEBELUM rebooting
      saveDynamicVersionToNVS(newVersion, serverMD5);

      // Beritahu server bahwa perangkat sedang melakukan proses update
      sendHeartbeatToServer("updating_to_v" + newVersion);

      // Daftarkan callbacks HTTPUpdate
      httpUpdate.onStart(updateStarted);
      httpUpdate.onEnd(updateFinished);
      httpUpdate.onProgress(updateProgress);
      httpUpdate.onError(updateError);

      // Eksekusi HTTPUpdate OTA Stream
      Serial.println("[OTA] Sedang mengunduh dan mem-flash partisi baru...");
      t_httpUpdate_return ret = httpUpdate.update(client, downloadUrl);

      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("[OTA FAILED] Update Gagal! Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
          sendHeartbeatToServer("update_failed");
          break;
        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("[OTA] Tidak ada update.");
          break;
        case HTTP_UPDATE_OK:
          Serial.println("[OTA] Flashing Selesai! ESP32 akan reboot...");
          break;
      }
      return;
    } else {
      Serial.println("[OTA CHECK] Firmware saat ini sudah versi terbaru (v" + runningFirmwareVersion + ").");
    }
  } else {
    Serial.printf("[OTA CHECK] Gagal menghubungi server. HTTP Code: %d\n", httpCode);
  }

  http.end();
}

// ==============================================================================
// SETUP
// ==============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. Validasi partisi aktif untuk membatalkan rollback otomatis
  esp_ota_mark_app_valid_cancel_rollback();

  // 2. Muat versi firmware dan MD5 secara dinamis dari NVS Flash
  loadDynamicVersionFromNVS();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("\n==================================================");
  Serial.printf("  %s\n", FIRMWARE_PROJECT_NAME);
  Serial.printf("  Versi Dinamis NVS: v%s\n", runningFirmwareVersion.c_str());
  Serial.println("==================================================");
  Serial.printf("Chip Model       : ESP32 Dual-Core @ %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("MAC Address      : %s\n", getMacAddressFormatted().c_str());
  Serial.printf("Active Partition : %s\n", getCurrentPartitionLabel().c_str());
  Serial.printf("Current MD5      : %s\n", currentFlashedMD5.length() > 0 ? currentFlashedMD5.c_str() : "Initial Flash");
  Serial.printf("Target OTA Server: %s\n", OTA_SERVER_HOST);
  Serial.println("--------------------------------------------------");

  // Koneksi WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WIFI] Menghubungkan ke: %s ", WIFI_SSID);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  digitalWrite(LED_PIN, LOW);
  Serial.println("\n[WIFI] Terhubung!");
  Serial.printf("[WIFI] IP Address  : %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("[WIFI] Sinyal RSSI : %d dBm\n", WiFi.RSSI());
  Serial.println("==================================================\n");

  // Kirim heartbeat pertama kali
  sendHeartbeatToServer("online");
  lastOtaCheckTime = millis();
  
  // --------------------------------------------------------------------------
  // Masukkan kode aplikasi IoT Anda di bawah ini
  // --------------------------------------------------------------------------

  pinMode(14, OUTPUT);
  pinMode(12, OUTPUT);
}

// ==============================================================================
// LOOP
// ==============================================================================
void loop() {
  unsigned long currentMillis = millis();

  // 1. Indikator LED Blink Heartbeat
  if (currentMillis - lastBlinkTime >= 1000) {
    lastBlinkTime = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }

  // 2. Kirim Laporan Heartbeat ke Server PC secara berkala
  if (currentMillis - lastHeartbeatTime >= heartbeatIntervalMs) {
    lastHeartbeatTime = currentMillis;
    sendHeartbeatToServer("online");
  }

  // 3. Pengecekan OTA Dinamis (Dengan jeda stabil 8 detik setelah boot)
  if (!initialOtaCheckDone && currentMillis >= 8000) {
    initialOtaCheckDone = true;
    lastOtaCheckTime = currentMillis;
    checkAndExecuteOtaUpdate();
  } else if (initialOtaCheckDone && (currentMillis - lastOtaCheckTime >= otaCheckIntervalMs)) {
    lastOtaCheckTime = currentMillis;
    checkAndExecuteOtaUpdate();
  }

  // --------------------------------------------------------------------------
  // Masukkan kode aplikasi IoT Anda di bawah ini
  // --------------------------------------------------------------------------
  digitalWrite(14, HIGH);
  delay(1000);
  digitalWrite(14, LOW);
  delay(1000);
  digitalWrite(12, HIGH);
  delay(1000);
  digitalWrite(12, LOW);
  delay(1000);
}
