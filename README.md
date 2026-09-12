# ESP32 OTA Management Web Server & Client Sketch

Solusi lengkap sistem pembaruan firmware Over-The-Air (OTA) untuk ESP32. Terdiri dari **Web Server Pengelola Firmware (Node.js & Dashboard Modern)** yang berjalan di PC/Laptop dan **Contoh Sketch ESP32 (Arduino IDE)** yang secara otomatis mengunduh pembaruan firmware dari server.

---

## 📁 Struktur Direktori

```
2. OTA Server Sederhana/
├── server/                         # WEB SERVER OTA (PC / LAPTOP)
│   ├── public/
│   │   ├── index.html              # Antarmuka Dashboard Web Modern
│   │   ├── style.css               # Styling Glassmorphism Dark Theme
│   │   └── script.js               # Logika Upload Drag-and-Drop & Polling Heartbeat
│   ├── uploads/                    # Folder penyimpanan file binary (.bin)
│   ├── data/
│   │   └── ota_config.json         # Metadata versi & target MAC
│   ├── package.json                # Dependensi (Express, Multer, Cors)
│   └── server.js                   # REST API OTA Backend
│
├── esp32_sketch/                   # CONTOH PROGRAM ESP32 (ARDUINO IDE)
│   └── ESP32_OTA_Client/
│       ├── ESP32_OTA_Client.ino    # Sketch Arduino (WiFi, HTTPUpdate OTA, Auto-check, Heartbeat)
│       └── config.h                # Konfigurasi WiFi & Alamat IP Web Server PC
│
├── start_server.bat                # Shortcut 1-Click untuk menjalankan server di Windows
└── README.md                       # Panduan lengkap penggunaan
```

---

## 🖥️ BAGIAN 1: Menjalankan Web Server di PC/Laptop

### 1. Menjalankan Server
Anda dapat menjalankan server dengan salah satu cara berikut:
- **Cara 1 (Paling Mudah)**: Klik ganda file **`start_server.bat`**.
- **Cara 2 (Via Terminal/PowerShell)**:
  ```powershell
  cd "C:\Users\mahra\Downloads\Web_Pribadi\2. OTA Server Sederhana\server"
  node server.js
  ```

### 2. Membuka Dashboard Web
Saat server berjalan, terminal akan menampilkan alamat IP lokal PC Anda, contoh:
```
Port Server    : 3000
Akses Lokal    : http://localhost:3000
Alamat IP untuk dimasukkan di sketch ESP32:
  -> http://192.168.1.15:3000
```
Buka browser dan akses **`http://localhost:3000`**.

### 3. Mengunggah Firmware Baru ke Server
1. Buka Web Dashboard di browser.
2. Masukkan **Target Nomor Versi** baru (misal `1.0.1`).
3. *(Opsional)* Masukkan **Target MAC Address** (misal `549738124B00`) jika hanya ingin device tertentu yang mengupdate, atau kosongkan agar berlaku untuk semua ESP32.
4. Tulis catatan rilis (*Changelog*).
5. Drag & drop file binary **`.bin`** ke area upload, lalu klik **Publikasikan Firmware ke Server**.

---

## 📟 BAGIAN 2: Menyiapkan Program Sketch ESP32 (Arduino IDE)

### 1. Buka Sketch di Arduino IDE
Buka folder `esp32_sketch/ESP32_OTA_Client/` dan buka file **`ESP32_OTA_Client.ino`**.

### 2. Sesuaikan Konfigurasi di `config.h`
Buka tab **`config.h`** dan isi:
```cpp
// 1. Nama WiFi & Password
#define WIFI_SSID        "NAMA_WIFI_ANDA"
#define WIFI_PASSWORD    "PASSWORD_WIFI_ANDA"

// 2. Alamat IP Server PC yang didapat dari console terminal server
#define OTA_SERVER_HOST  "http://192.168.1.15:3000"

// 3. Versi Firmware saat ini pada sketch ini
#define CURRENT_FIRMWARE_VERSION "1.0.0"
```

### 3. Pengaturan Board di Arduino IDE
Di menu **Tools** Arduino IDE:
- **Board**: `ESP32 Dev Module`
- **Partition Scheme**: `Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)` *(sesuai tabel partisi Anda)*
- **Flash Frequency**: `80MHz`

### 4. Upload Pertama Kali via Kabel USB
Hubungkan ESP32 ke komputer dan upload sketch tersebut. Buka **Serial Monitor** pada baudrate `115200`.

---

## 🔄 BAGIAN 3: Cara Kerja Alur OTA Otomatis

1. **ESP32 Terhubung & Melapor (Heartbeat)**:
   - ESP32 terhubung ke WiFi dan mengirimkan status ke server setiap 10 detik.
   - Anda dapat melihat perangkat Anda langsung muncul di tabel **"Daftar ESP32 Terhubung (Live Heartbeat)"** pada web dashboard beserta MAC `549738124B00`, sisa RAM, dan sinyal WiFi.
2. **Membuat File Binary Firmware Baru**:
   - Di Arduino IDE, ubah versi di `config.h` menjadi `1.0.1` (atau ubah kode program Anda).
   - Klik menu **Sketch** $\rightarrow$ **Export Compiled Binary** (`Ctrl + Alt + S`).
   - File `.bin` akan tersimpan di dalam folder sketch.
3. **Mendistribusikan Update**:
   - Unggah file `.bin` tadi melalui Web Dashboard dengan nomor versi `1.0.1`.
   - Dalam maksimal 30 detik (interval auto-check), ESP32 akan mendeteksi bahwa versi di server (`1.0.1`) lebih tinggi dari versinya (`1.0.0`), lalu otomatis mengunduh stream binary dan melakukan reboot ke partisi baru (`app0` $\rightarrow$ `app1`).
4. **Status Berubah di Dashboard**:
   - Setelah reboot, ESP32 akan melapor ke dashboard dengan versi terbaru `v1.0.1`!

---

## 📡 REST API Endpoints

| Endpoint | Method | Keterangan |
| :--- | :---: | :--- |
| `/api/config` | `GET` | Mendapatkan metadata firmware aktif di server |
| `/api/upload` | `POST` | Upload file `.bin` baru via multipart form-data |
| `/api/ota/check` | `GET` | Endpoint pengecekan versi (`?version=x.x.x&mac=XXXX`) |
| `/api/ota/download/:type` | `GET` | Stream unduhan binary `.bin` ke ESP32 |
| `/api/ota/heartbeat` | `POST` | Penerima telemetri berkala dari ESP32 |
| `/api/devices` | `GET` | Daftar seluruh ESP32 yang terhubung & online |
| `/api/firmware/:type` | `DELETE` | Menghapus binary firmware dari server |
