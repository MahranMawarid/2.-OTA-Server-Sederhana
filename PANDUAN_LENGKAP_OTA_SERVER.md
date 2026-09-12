# PANDUAN LENGKAP: SISTEM OTA SERVER & CARA ESP32 MENGAMBIL DATA BIN

Buku panduan ini menjelaskan secara mendalam bagaimana **Web Server OTA** dibangun dari awal, bagaimana antarmuka dashboard bekerja, serta **mekanisme teknis lengkap bagaimana ESP32 berkomunikasi dengan web server untuk mengunduh dan mengeksekusi file `.bin` secara aman tanpa looping**.

---

## 📑 DAFTAR ISI
1. [Arsitektur & Diagram Alur Komunikasi](#1-arsitektur--diagram-alur-komunikasi)
2. [Cara Pembuatan Web Server dari Awal (Node.js & Dashboard)](#2-cara-pembuatan-web-server-dari-awal)
3. [Mekanisme Teknis: Bagaimana ESP32 Mengambil Data .bin](#3-mekanisme-teknis-bagaimana-esp32-mengambil-data-bin)
4. [Penjelasan Struktur Program Sketch ESP32](#4-penjelasan-struktur-program-sketch-esp32)
5. [Panduan Langkah Praktik dari Nol ke Sukses OTA](#5-panduan-langkah-praktik-dari-nol-ke-sukses-ota)
6. [Troubleshooting & Solusi Kendala Umum](#6-troubleshooting--solusi-kendala-umum)

---

## 1. ARSITEKTUR & DIAGRAM ALUR KOMUNIKASI

Sistem ini terdiri dari 3 komponen utama:
1. **Web Dashboard (Browser Pengguna)**: Tempat Anda mengunggah file `.bin`, mengatur target versi, memantau status perangkat (*Live Heartbeat*), dan memilih mode *Auto-Deploy* / *Manual Trigger*.
2. **OTA Web Server (Node.js Express di PC/Laptop)**: Menyimpan file binary `.bin`, menghitung sidik jari hash MD5, mengelola antrean pembaruan, dan mengalirkan (*streaming*) file ke ESP32.
3. **ESP32 (Klien IoT)**: Terhubung ke WiFi yang sama, melapor status berkala, meminta informasi firmware, mengunduh *stream* binary ke partisi cadangan (`app1`), lalu berpindah partisi saat reboot.

### Diagram Alur Komunikasi (Sequence Flow)

```
[Browser Admin]              [Web Server PC]                 [ESP32 Device]
      |                             |                              |
      |-- 1. Upload .bin & Versi -->| (Simpan di /uploads &        |
      |   (Target: v1.0.1)          |  Hitung MD5 Hash)            |
      |                             |                              |
      |                             |<-- 2. POST /api/ota/heartbeat| (Tiap 5-10s: Kirim IP,
      |                             |    (Versi Saat Ini: v1.0.0)  |  RAM, RSSI, & MD5)
      |                             |-- Response: Ada update! ---->|
      |                             |                              |
      |                             |<-- 3. GET /api/ota/check ----| (Mengecek update detail)
      |                             |-- Response JSON: ----------->|
      |                             |   { update: true,            |
      |                             |     version: "1.0.1",        |
      |                             |     md5: "a1b2c3...",        |
      |                             |     download_url: "..." }    |
      |                             |                              |
      |                             |<-- 4. GET /download/firmware | (Memulai HTTPUpdate)
      |                             |== Stream Byte Binary (.bin)==> (Tulis chunk ke Partisi
      |                             |   (Chunk per Chunk 4-64KB)   |  Flash app1)
      |                             |                              |
      |                             |                              |-- 5. Flashing Sukses!
      |                             |                              |   - Simpan v1.0.1 ke NVS
      |                             |                              |   - Switch boot pointer
      |                             |                              |   - Restart Chip (Reboot)
      |                             |                              |
      |                             |<-- 6. Heartbeat Baru --------| (ESP32 Boot di app1:
      |                             |    (Versi Aktif: v1.0.1)     |  Melapor v1.0.1)
      |<- 7. Tampilkan Status Baru -|                              |
```

---

## 2. CARA PEMBUATAN WEB SERVER DARI AWAL

Berikut adalah langkah-langkah membuat Web Server OTA ini dari nol:

### Langkah 2.1: Inisialisasi Proyek Node.js
Buat folder proyek baru dan buat file `package.json`:
```json
{
  "name": "esp32-ota-server",
  "version": "1.0.0",
  "description": "Server Manajemen OTA ESP32 Dinamis",
  "main": "server.js",
  "scripts": {
    "start": "node server.js"
  },
  "dependencies": {
    "cors": "^2.8.5",
    "express": "^4.19.2",
    "multer": "^1.4.5-lts.1"
  }
}
```
Jalankan perintah instalasi dependensi:
```bash
npm install
```

### Langkah 2.2: Backend Server (`server.js`)
Backend dibangun menggunakan Express.js dengan peran:
1. **`POST /api/upload`**: Menerima file `.bin` menggunakan `multer`, menyimpan ke folder `uploads/`, dan menghitung hash MD5 menggunakan modul bawaan Node.js `crypto`.
2. **`GET /api/ota/check`**: Menerima query parameter dari ESP32 (`?version=x.x.x&mac=XXXX&md5=YYYY`). Server membandingkan versi dan MD5. Jika ada versi baru dan MD5 berbeda, server mengembalikan data URL download.
3. **`GET /api/ota/download/:type`**: Mengalirkan file binary (`fs.createReadStream`) dengan header `Content-Type: application/octet-stream` dan header `x-MD5`.
4. **`POST /api/ota/heartbeat`**: Menerima laporan telemetri perangkat (MAC, IP, Free RAM, RSSI WiFi, Partisi aktif, Uptime) untuk ditampilkan di dashboard.
5. **`POST /api/devices/trigger-ota`**: Menangani perintah pembaruan manual saat admin menekan tombol "Deploy Update" di web.

### Langkah 2.3: Frontend Dashboard (`public/index.html`, `style.css`, `script.js`)
1. **Tampilan Modern (Glassmorphism Dark Theme)**: Dirancang responsif dengan CSS grid dan kartu metrik yang nyaman dilihat di desktop maupun smartphone.
2. **Drag-and-Drop Uploader**: Memungkinkan pengguna menyeret file `.bin` langsung dari file explorer komputer.
3. **Kontrol Dinamis**:
   - Sakelar **Auto-Deploy**: Menentukan apakah ESP32 langsung di-update begitu file diupload, atau menunggu konfirmasi manual.
   - Dropdown **Polling Interval**: Mengatur seberapa sering ESP32 memeriksa pembaruan (10s, 15s, 30s, 60s).
4. **Tabel Live Heartbeat**: Memperbarui status perangkat setiap 3 detik secara otomatis via AJAX (`fetch('/api/devices')`).

---

## 3. MEKANISME TEKNIS: BAGAIMANA ESP32 MENGAMBIL DATA .BIN

Banyak pengembang pemula mengira ESP32 mendownload seluruh file `.bin` (yang berukuran ~1 MB) ke dalam memori RAM sekaligus. **Hal itu salah dan mustahil**, karena total RAM bebas ESP32 hanya sekitar 300 KB.

Berikut adalah mekanisme sebenarnya yang terjadi di dalam chip ESP32:

### A. Aliran Data Secara "Chunked Streaming" (Bongkahan Kecil)
Ketika fungsi `httpUpdate.update(client, downloadUrl)` dipanggil:
1. ESP32 membuka koneksi TCP/HTTP ke server pada port `3000`.
2. ESP32 mengirimkan HTTP Request:
   ```http
   GET /api/ota/download/firmware HTTP/1.1
   Host: 192.168.1.15:3000
   User-Agent: ESP32-HTTP-Update
   ```
3. Server membalas dengan Header:
   ```http
   HTTP/1.1 200 OK
   Content-Type: application/octet-stream
   Content-Length: 1048576
   x-MD5: e2fc714c4727ee9395f324cd2e7f331f
   ```
4. **Proses Streaming ke Flash Memory**:
   - ESP32 hanya menyediakan buffer kecil di RAM (sekitar **4 KB hingga 64 KB**).
   - ESP32 membaca 4 KB data dari paket jaringan WiFi $\rightarrow$ langsung menulis dan membakar (*flashing*) 4 KB tersebut ke partisi flash target (`app1`).
   - Setelah 4 KB tertulis, buffer RAM dikosongkan untuk membaca 4 KB berikutnya dari jaringan WiFi.
   - Proses ini diulang terus hingga seluruh total byte (`Content-Length`) selesai ditulis 100%.

### B. Pergantian Partisi (*Partition Switching*)
ESP32 Anda memiliki tabel partisi Dual-Bank:
- `app0` (1280 KB)
- `app1` (1280 KB)
- `otadata` (8 KB)

```
Kondisi Awal:
ESP32 sedang aktif berjalan di partisi [app0].

Saat Proses OTA Berlangsung:
Data binary baru ditulis ke partisi cadangan [app1]. Partisi [app0] yang sedang berjalan TIDAK DIGANGGU.

Saat Selesai 100%:
Fungsi `esp_ota_set_boot_partition(app1)` menulis catatan ke partisi `otadata`.

Saat Reboot:
Bootloader membaca `otadata`, melihat bahwa [app1] adalah firmware terbaru yang valid, lalu mengeksekusi [app1].
```

---

## 4. PENJELASAN STRUKTUR PROGRAM SKETCH ESP32

Program sketch Arduino [**`ESP32_OTA_Client.ino`**](file:///C:/Users/mahra/Downloads/Web_Pribadi/2.%20OTA%20Server%20Sederhana/esp32_sketch/ESP32_OTA_Client/ESP32_OTA_Client.ino) memiliki 4 pilar utama:

### 1. Pengelolaan Versi Dinamis via NVS (`Preferences.h`)
```cpp
// Membaca versi saat boot dari memori flash NVS internal
void loadDynamicVersionFromNVS() {
  prefs.begin("ota_mgr", false);
  runningFirmwareVersion = prefs.getString("fw_ver", "1.0.0");
  currentFlashedMD5 = prefs.getString("fw_md5", "");
  prefs.end();
}
```
* **Manfaat**: Anda tidak perlu mengubah kode string versi di Arduino IDE saat kompilasi. Versi yang Anda ketikkan di Web Dashboard akan otomatis diserap dan disimpan permanen ke NVS ESP32.

### 2. Proteksi Anti-Looping (Pemeriksaan MD5 Checksum)
```cpp
// Jika MD5 binary di server sama dengan MD5 yang sedang berjalan di flash:
if (serverMD5.length() > 0 && serverMD5 == currentFlashedMD5) {
  Serial.println("[ANTI-LOOP] Binary di server identik (MD5 cocok). Melewati update.");
  return; // Batalkan unduhan!
}
```
* **Manfaat**: Mencegah ESP32 mengunduh ulang file yang sama secara terus-menerus.

### 3. Validasi Rollback Bootloader
```cpp
// Di dalam setup():
esp_ota_mark_app_valid_cancel_rollback();
```
* **Manfaat**: Memberitahu bootloader ESP-IDF bahwa firmware yang baru berjalan ini stabil dan sukses, sehingga bootloader tidak melakukan *rollback* (kembali) ke firmware lama.

### 4. Eksekusi Unduhan OTA
```cpp
// Mendaftarkan callback monitor persentase
httpUpdate.onProgress(updateProgress);
httpUpdate.onStart(updateStarted);
httpUpdate.onEnd(updateFinished);

// Eksekusi stream unduh dan flash
t_httpUpdate_return ret = httpUpdate.update(client, downloadUrl);
```

---

## 5. PANDUAN LANGKAH PRAKTIK DARI NOL KE SUKSES OTA

### Langkah 1: Menjalankan Server di Komputer
1. Buka folder `C:\Users\mahra\Downloads\Web_Pribadi\2. OTA Server Sederhana\`.
2. Klik ganda file **`start_server.bat`**.
3. Jendela terminal hitam akan terbuka dan menampilkan IP komputer Anda, contoh:
   ```
   ======================================================
          ESP32 OTA MANAGEMENT WEB SERVER BERJALAN        
   ======================================================
   Port Server    : 3000
   Akses Lokal    : http://localhost:3000
   Alamat IP untuk dimasukkan di sketch ESP32:
     -> http://192.168.1.15:3000
   ======================================================
   ```
4. Buka browser (Chrome / Edge / Firefox) dan buka: **`http://localhost:3000`**.

---

### Langkah 2: Mengunggah Sketch Pertama Kali ke ESP32 (Via Kabel USB)
1. Buka Arduino IDE.
2. Buka file: `esp32_sketch/ESP32_OTA_Client/ESP32_OTA_Client.ino`.
3. Buka tab **`config.h`** dan sesuaikan:
   - `WIFI_SSID` : Nama WiFi router Anda.
   - `WIFI_PASSWORD` : Password WiFi Anda.
   - `OTA_SERVER_HOST` : Alamat IP komputer Anda yang muncul di terminal (misal: `"http://192.168.1.15:3000"`).
4. Di menu **Tools** Arduino IDE:
   - **Board**: `ESP32 Dev Module`
   - **Partition Scheme**: `Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)`
   - **Flash Frequency**: `80MHz`
   - **Port**: Pilih port COM ESP32 Anda.
5. Klik **Upload** via kabel USB.
6. Buka **Serial Monitor** (Baudrate `115200`). ESP32 akan terhubung ke WiFi dan mengirim *heartbeat* ke server.
7. Lihat Web Dashboard di browser Anda $\rightarrow$ ESP32 Anda (MAC: `549738124B00`) sudah berstatus **ONLINE** di tabel!

---

### Langkah 3: Melakukan Pembaruan OTA Tanpa Kabel USB
1. Di Arduino IDE, tambahkan fitur atau ubah kode program Anda (misalnya ubah jeda kedipan LED atau tambahkan pembacaan sensor).
2. Di Arduino IDE, klik menu:
   **`Sketch` $\rightarrow$ `Export Compiled Binary`** (atau tekan shortcut **`Ctrl + Alt + S`**).
3. Arduino IDE akan meng-compile dan menghasilkan file binary berekstensi **`.bin`** (misal `ESP32_OTA_Client.ino.bin`) di dalam folder sketch.
4. Buka Web Dashboard di browser:
   - Masukkan **Target Nomor Versi**: misal **`1.0.1`**.
   - Tulis catatan perubahan pada kolom **Catatan Rilis**.
   - Seret file **`.bin`** hasil export tadi ke kotak dropzone upload.
   - Klik tombol **Publikasikan Firmware ke Server**.
5. **Amati Proses OTA**:
   - Di Web Dashboard: Status ESP32 akan berubah menjadi *Sedang Flashing...*.
   - Di Serial Monitor ESP32: Muncul log progres `[OTA PROGRESS] Terunduh: 10% ... 50% ... 100%`.
   - ESP32 otomatis reboot dan berpindah partisi (`app0` $\rightarrow$ `app1`).
   - Dalam hitungan detik, ESP32 kembali online di Web Dashboard dengan versi baru **`v1.0.1`**!

---

## 6. TROUBLESHOOTING & SOLUSI KENDALA UMUM

### 1. ESP32 Gagal Menghubungi Server (`HTTP Code: -1` atau `Connection Refused`)
* **Penyebab**: Windows Firewall memblokir port 3000 atau IP komputer berubah.
* **Solusi**:
  - Pastikan ESP32 dan Komputer terhubung ke **satu jaringan WiFi / Router yang sama**.
  - Izinkan aplikasi Node.js pada Windows Defender Firewall (pilih *Allow on Private Networks*).
  - Cek kembali IP komputer Anda di terminal `start_server.bat` dan pastikan sama dengan `OTA_SERVER_HOST` di `config.h`.

### 2. Ukuran File Binary Melebihi Batas Partisi (`Sketch too big`)
* **Penyebab**: Ukuran file `.bin` melebihi alokasi partisi `app0`/`app1` (1280 KB / 1.25 MB).
* **Solusi**: Web server dan ESP32 telah dilengkapi proteksi batas ukuran maksimal 1.28 MB. Pastikan Anda tidak menyertakan library yang memakan flash terlalu besar, atau pilih skema partisi *Minimal SPIFFS (1.9MB APP)* jika program Anda membutuhkan ruang lebih dari 1.25 MB.

### 3. ESP32 Mengalami Looping Re-flash Berulang
* **Solusi**: Fitur **MD5 Anti-Looping** dan **NVS Dynamic Versioning** pada versi terbaru ini telah secara otomatis mencegah masalah tersebut. Pastikan Anda menggunakan file sketch `ESP32_OTA_Client.ino` dan `server.js` terbaru yang telah disediakan.

---

*Dokumen ini dibuat otomatis sebagai panduan operasional standar untuk proyek ESP32 OTA Web Server.*
