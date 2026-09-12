/*
  ==============================================================================
  PROJECT : Web Server OTA ESP32 Dev Module (On-Device Web Server)
  AUTHOR  : Mahra
  DATE    : 2026
  BOARD   : ESP32 Dev Module (Xtensa Dual-Core 240MHz, 4MB Flash)
  PARTISI : Dual App (app0: 1280KB, app1: 1280KB, spiffs: 1408KB)
  ==============================================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include "config.h"
#include "webpage.h"

// Web Server instance pada port yang ditentukan di config.h (default 80)
WebServer server(WEB_SERVER_PORT);

// State variable untuk proses reboot tertunda
bool shouldReboot = false;
unsigned long rebootTimer = 0;
const unsigned long REBOOT_DELAY_MS = 2000;

// Variabel status OTA
int currentOtaCommand = U_FLASH;
size_t lastUploadSize = 0;
String otaErrorMessage = "";

// ==============================================================================
// HELPER FUNCTIONS: INFORMASI SISTEM & IDENTITAS CHIP
// ==============================================================================
String getMacAddressFormatted() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

uint32_t getChipId32() {
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  return chipId;
}

String getCurrentPartitionLabel() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running != NULL) {
    return String(running->label);
  }
  return "unknown";
}

String getNextPartitionLabel() {
  const esp_partition_t* next = esp_ota_get_next_update_partition(NULL);
  if (next != NULL) {
    return String(next->label);
  }
  return "none";
}

// ==============================================================================
// AUTHENTICATION CHECK
// ==============================================================================
bool isUserAuthenticated() {
  if (!server.authenticate(WWW_USER, WWW_PASS)) {
    server.requestAuthentication(BASIC_AUTH, "ESP32_OTA_Login", "Akses Ditolak: Masukkan kredensial admin.");
    return false;
  }
  return true;
}

// ==============================================================================
// ROUTE HANDLERS
// ==============================================================================

// 1. Root / Dashboard Handler
void handleRoot() {
  if (!isUserAuthenticated()) return;
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", INDEX_HTML);
}

// 2. API Status & Telemetri JSON
void handleApiStatus() {
  if (!isUserAuthenticated()) return;

  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t totalHeap = ESP.getHeapSize();
  uint32_t minFreeHeap = ESP.getMinFreeHeap();
  uint32_t maxAllocHeap = ESP.getMaxAllocHeap();

  uint32_t flashSize = ESP.getFlashChipSize();
  uint32_t sketchSize = ESP.getSketchSize();
  uint32_t freeSketchSpace = ESP.getFreeSketchSpace();

  int rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  String ipAddr = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  String ssidName = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String(AP_SSID) + " (AP Mode)";

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  String json = "{";
  json += "\"firmware_name\":\"" + String(FIRMWARE_NAME) + "\",";
  json += "\"firmware_version\":\"" + String(FIRMWARE_VERSION) + "\",";
  json += "\"firmware_author\":\"" + String(FIRMWARE_AUTHOR) + "\",";
  json += "\"mac_address\":\"" + getMacAddressFormatted() + "\",";
  json += "\"chip_id\":\"" + String(getChipId32()) + "\",";
  json += "\"chip_model\":\"ESP32\",";
  json += "\"chip_rev\":" + String(chip_info.revision) + ",";
  json += "\"chip_cores\":" + String(chip_info.cores) + ",";
  json += "\"cpu_freq_mhz\":" + String(ESP.getCpuFreqMHz()) + ",";
  json += "\"free_heap\":" + String(freeHeap) + ",";
  json += "\"total_heap\":" + String(totalHeap) + ",";
  json += "\"min_free_heap\":" + String(minFreeHeap) + ",";
  json += "\"max_alloc_heap\":" + String(maxAllocHeap) + ",";
  json += "\"flash_size\":" + String(flashSize) + ",";
  json += "\"sketch_size\":" + String(sketchSize) + ",";
  json += "\"free_sketch_space\":" + String(freeSketchSpace) + ",";
  json += "\"current_partition\":\"" + getCurrentPartitionLabel() + "\",";
  json += "\"next_partition\":\"" + getNextPartitionLabel() + "\",";
  json += "\"wifi_rssi\":" + String(rssi) + ",";
  json += "\"wifi_ssid\":\"" + ssidName + "\",";
  json += "\"ip_address\":\"" + ipAddr + "\",";
  json += "\"uptime_seconds\":" + String(millis() / 1000);
  json += "}";

  server.sendHeader("Content-Type", "application/json");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// 3. API Version
void handleApiVersion() {
  String json = "{\"version\":\"" + String(FIRMWARE_VERSION) + "\",\"name\":\"" + String(FIRMWARE_NAME) + "\",\"mac\":\"" + getMacAddressFormatted() + "\"}";
  server.send(200, "application/json", json);
}

// 4. API Restart Device
void handleApiRestart() {
  if (!isUserAuthenticated()) return;

  server.send(200, "application/json", "{\"success\":true,\"message\":\"ESP32 akan melakukan reboot dalam 2 detik...\"}");
  shouldReboot = true;
  rebootTimer = millis();
}

// 5. OTA Upload Final Response
void handleOtaUpdateResponse() {
  if (!isUserAuthenticated()) return;

  bool success = !Update.hasError();
  String message = success ? "Pembaruan Berhasil! Perangkat sedang memulai ulang..." : otaErrorMessage;

  String json = "{\"success\":" + String(success ? "true" : "false") + ",\"message\":\"" + message + "\"}";
  server.send(200, "application/json", json);

  if (success) {
    shouldReboot = true;
    rebootTimer = millis();
  }
}

// 6. OTA Upload Streaming Handler (Chunked File Stream)
void handleOtaUploadStream() {
  if (!isUserAuthenticated()) return;

  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    otaErrorMessage = "";
    lastUploadSize = 0;

    // Tentukan tipe partisi: Firmware (Flash) atau Filesystem (SPIFFS)
    String targetType = server.arg("type");
    if (targetType == "spiffs") {
      currentOtaCommand = U_SPIFFS;
      Serial.println("\n[OTA] Memulai upload filesystem (SPIFFS)...");
    } else {
      currentOtaCommand = U_FLASH;
      Serial.println("\n[OTA] Memulai upload firmware aplikasi (FLASH)...");
    }

    Serial.printf("[OTA] Nama File: %s\n", upload.filename.c_str());

    // Alokasi update buffer sesuai tipe
    size_t maxSpace = (currentOtaCommand == U_SPIFFS) ? 0x160000 : (ESP.getFreeSketchSpace() - 0x1000) & ~0xFFF;
    
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, currentOtaCommand)) {
      otaErrorMessage = "Update.begin Gagal: " + String(Update.errorString());
      Serial.printf("[OTA ERROR] %s\n", otaErrorMessage.c_str());
      Update.printError(Serial);
    }
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      otaErrorMessage = "Update.write Gagal: " + String(Update.errorString());
      Serial.printf("[OTA ERROR] %s\n", otaErrorMessage.c_str());
      Update.printError(Serial);
    } else {
      lastUploadSize += upload.currentSize;
      if (lastUploadSize % (64 * 1024) == 0 || upload.currentSize == 0) {
        Serial.printf("[OTA PROGRESS] Tertulis: %u KB\n", (unsigned int)(lastUploadSize / 1024));
      }
    }
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[OTA SUCCESS] Total byte berhasil ditulis: %u bytes (%u KB)\n", (unsigned int)upload.totalSize, (unsigned int)(upload.totalSize / 1024));
      Serial.println("[OTA] Firmware / filesystem baru telah terverifikasi.");
    } else {
      otaErrorMessage = "Update.end Gagal: " + String(Update.errorString());
      Serial.printf("[OTA ERROR] %s\n", otaErrorMessage.c_str());
      Update.printError(Serial);
    }
  } 
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.end();
    otaErrorMessage = "Proses upload dibatalkan oleh pengguna atau jaringan.";
    Serial.println("[OTA ABORT] Upload dibatalkan.");
  }
}

// 7. Not Found 404 Handler
void handleNotFound() {
  if (server.method() == HTTP_OPTIONS) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "X-Requested-With, content-type");
    server.send(204);
    return;
  }
  server.send(404, "text/plain", "404: Not Found on ESP32 OTA Web Server");
}

// ==============================================================================
// SETUP: INISIALISASI HARDWARE & JARINGAN
// ==============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==================================================");
  Serial.printf("  %s v%s\n", FIRMWARE_NAME, FIRMWARE_VERSION);
  Serial.println("==================================================");
  Serial.printf("Chip Model       : ESP32 Dual Core @ %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("MAC Address      : %s\n", getMacAddressFormatted().c_str());
  Serial.printf("Chip ID          : %u\n", getChipId32());
  Serial.printf("Active Partition : %s\n", getCurrentPartitionLabel().c_str());
  Serial.printf("Next OTA Part    : %s\n", getNextPartitionLabel().c_str());
  Serial.printf("Free Sketch Spc  : %u KB\n", (unsigned int)(ESP.getFreeSketchSpace() / 1024));
  Serial.printf("Free Heap RAM    : %u bytes\n", ESP.getFreeHeap());
  Serial.println("--------------------------------------------------");

  // Koneksi ke WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("[WIFI] Menghubungkan ke SSID: %s ", WIFI_SSID);
  
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
    delay(500);
    Serial.print(".");
  }

  // Jika gagal terhubung ke router, aktifkan mode Access Point (AP) darurat
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[WIFI] Gagal terhubung ke WiFi router! Mengaktifkan mode Access Point (AP)...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("[WIFI AP] AP SSID: %s | Password: %s\n", AP_SSID, AP_PASSWORD);
    Serial.printf("[WIFI AP] Akses Dashboard di: http://%s\n", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("\n[WIFI] Terhubung dengan sukses!");
    Serial.printf("[WIFI] IP Address  : http://%s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] Sinyal RSSI : %d dBm\n", WiFi.RSSI());

    // Inisialisasi mDNS Responder
    if (MDNS.begin(DEVICE_HOSTNAME)) {
      MDNS.addService("http", "tcp", WEB_SERVER_PORT);
      Serial.printf("[mDNS] Anda juga dapat mengakses via: http://%s.local\n", DEVICE_HOSTNAME);
    }
  }

  // Setup Routing Web Server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/version", HTTP_GET, handleApiVersion);
  server.on("/api/restart", HTTP_POST, handleApiRestart);

  // Endpoint OTA Upload (Handler response + Handler upload stream)
  server.on("/update", HTTP_POST, handleOtaUpdateResponse, handleOtaUploadStream);

  server.onNotFound(handleNotFound);

  // Start Web Server
  server.begin();
  Serial.printf("[HTTP] Web Server aktif pada port %d.\n", WEB_SERVER_PORT);
  Serial.printf("[AUTH] Login Web: Username='%s', Password='%s'\n", WWW_USER, WWW_PASS);
  Serial.println("==================================================\n");
}

// ==============================================================================
// MAIN LOOP
// ==============================================================================
void loop() {
  server.handleClient();

  // Eksekusi restart yang tertunda (agar respons HTTP berhasil dikirim ke browser terlebih dahulu)
  if (shouldReboot && (millis() - rebootTimer >= REBOOT_DELAY_MS)) {
    Serial.println("\n[SYSTEM] Memulai proses restart sistem ESP32...");
    delay(100);
    ESP.restart();
  }
}
