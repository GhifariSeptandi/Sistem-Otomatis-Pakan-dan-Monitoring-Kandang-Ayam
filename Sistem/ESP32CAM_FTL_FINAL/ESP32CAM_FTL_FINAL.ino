// ================================================================
// SISTEM PAKAN OTOMATIS & MONITORING KANDANG AYAM
// Platform  : ESP32-CAM AI-Thinker
// Fitur     : Live Stream, Snapshot, Kirim Foto Telegram Otomatis
// Tugas Akhir - Teknik Komputer Telkom University
// ================================================================

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_http_server.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ================================================================
// KONFIGURASI — sesuaikan dengan jaringan & bot kamu
// ================================================================
const char* ssid     = "V-61";
const char* password = "24898454";
#define BOT_TOKEN "8202425520:AAGyGhM1IfvkqrOZzILct2_DQRMt8c_PRD0"
#define CHAT_ID   "1039199332"

// ================================================================
// PIN KAMERA (AI-Thinker ESP32-CAM) — jangan diubah
// ================================================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define FLASH_LED_PIN      4
#define PIR_PIN 13

bool motionDetected = false;

// ================================================================
// STREAMING CONFIG
// ================================================================
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE =
  "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY =
  "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART =
  "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

// ================================================================
// TELEGRAM CONFIG
// ================================================================
WiFiClientSecure clientSecure;
UniversalTelegramBot bot(BOT_TOKEN, clientSecure);

// ================================================================
// CALLBACK BUFFER BINARY FOTO
// ================================================================
uint8_t* _imgBuf = nullptr;
size_t   _imgLen = 0;
size_t   _imgIdx = 0;

bool moreDataAvailable() { return _imgIdx < _imgLen; }
uint8_t getNextByte()    { return _imgBuf[_imgIdx++]; }

// ================================================================
// HTML DASHBOARD
// ================================================================
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Sistem Pakan Otomatis & Monitoring Kandang Ayam</title>
  <style>
    :root {
      --bg:      #0a0e17;
      --surface: #111827;
      --border:  #1f2937;
      --accent:  #22c55e;
      --blue:    #3b82f6;
      --text:    #f1f5f9;
      --muted:   #64748b;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background: var(--bg);
      color: var(--text);
      font-family: 'Segoe UI', system-ui, sans-serif;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 24px 16px 40px;
    }
    .header { text-align: center; margin-bottom: 24px; }
    .header .badge {
      display: inline-block;
      background: rgba(34,197,94,.12);
      color: var(--accent);
      border: 1px solid rgba(34,197,94,.3);
      font-size: .72rem;
      letter-spacing: .12em;
      text-transform: uppercase;
      padding: 4px 12px;
      border-radius: 99px;
      margin-bottom: 10px;
    }
    .header h1 {
      font-size: clamp(1.1rem, 4vw, 1.55rem);
      font-weight: 700;
      color: var(--text);
      line-height: 1.3;
    }
    .header p { color: var(--muted); font-size: .82rem; margin-top: 4px; }
    .statusbar {
      display: flex; gap: 10px;
      flex-wrap: wrap; justify-content: center;
      margin-bottom: 20px;
    }
    .pill {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 6px 14px;
      font-size: .78rem;
      color: var(--muted);
      display: flex; align-items: center; gap: 6px;
    }
    .pill .dot {
      width: 7px; height: 7px;
      border-radius: 50%;
      background: var(--accent);
      box-shadow: 0 0 6px var(--accent);
      animation: pulse 1.6s infinite;
    }
    @keyframes pulse {
      0%,100% { opacity: 1; } 50% { opacity: .35; }
    }
    .stream-wrap {
      width: 100%; max-width: 820px;
      background: #000;
      border: 1px solid var(--border);
      border-radius: 14px;
      overflow: hidden;
      position: relative;
    }
    #stream { width: 100%; display: block; min-height: 200px; object-fit: cover; }
    .stream-label {
      position: absolute; top: 12px; left: 12px;
      background: rgba(0,0,0,.55);
      backdrop-filter: blur(6px);
      color: var(--accent);
      font-size: .72rem; letter-spacing: .1em;
      padding: 3px 10px; border-radius: 99px;
      border: 1px solid rgba(34,197,94,.3);
    }
    .controls {
      display: flex; gap: 10px; margin-top: 18px;
      flex-wrap: wrap; justify-content: center;
      max-width: 820px; width: 100%;
    }
    .btn {
      flex: 1 1 140px; max-width: 200px;
      background: var(--surface);
      border: 1px solid var(--border);
      color: var(--text);
      padding: 11px 16px; border-radius: 10px;
      cursor: pointer; font-size: .88rem; font-weight: 500;
      transition: background .18s, border-color .18s, color .18s, transform .1s;
      display: flex; align-items: center; justify-content: center; gap: 7px;
    }
    .btn:hover  { background: var(--blue); border-color: var(--blue); color: #fff; }
    .btn:active { transform: scale(.96); }
    .btn.green:hover { background: var(--accent); border-color: var(--accent); color: #0a0e17; }
    #toast {
      position: fixed; bottom: 28px; left: 50%;
      transform: translateX(-50%) translateY(80px);
      background: var(--surface); border: 1px solid var(--border);
      color: var(--text); padding: 10px 22px; border-radius: 10px;
      font-size: .85rem; transition: transform .3s ease;
      pointer-events: none; white-space: nowrap; z-index: 99;
    }
    #toast.show { transform: translateX(-50%) translateY(0); }
    .info-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
      gap: 10px; margin-top: 20px;
      max-width: 820px; width: 100%;
    }
    .info-card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 10px; padding: 14px 16px;
    }
    .info-card .label { font-size: .72rem; color: var(--muted); margin-bottom: 4px; }
    .info-card .value { font-size: 1.05rem; font-weight: 600; color: var(--text); }
  </style>
</head>
<body>
  <div class="header">
    <div class="badge">🟢 Online</div>
    <h1>🐔 Sistem Pakan Otomatis<br>&amp; Monitoring Kandang Ayam</h1>
    <p>ESP32-CAM · IoT · Telkom University</p>
  </div>
  <div class="statusbar">
    <div class="pill"><span class="dot"></span> Live Stream Aktif</div>
    <div class="pill">📡 ESP32-CAM AI-Thinker</div>
    <div class="pill">📷 SVGA 800x600</div>
  </div>
  <div class="stream-wrap">
    <span class="stream-label">● LIVE</span>
    <img id="stream" src="" alt="Camera Stream">
  </div>
  <div class="controls">
    <button class="btn green" onclick="startStream()">▶ Mulai Stream</button>
    <button class="btn"       onclick="stopStream()">⏹ Stop Stream</button>
    <button class="btn"       onclick="snapshot()">📸 Simpan Foto</button>
    <button class="btn"       onclick="kirimTelegram()">📨 Kirim Telegram</button>
  </div>
  <div class="info-grid">
    <div class="info-card">
      <div class="label">Uptime</div>
      <div class="value" id="uptime">0 detik</div>
    </div>
    <div class="info-card">
      <div class="label">Status Kamera</div>
      <div class="value" id="camstatus">Memuat...</div>
    </div>
    <div class="info-card">
      <div class="label">Resolusi</div>
      <div class="value">SVGA 800x600</div>
    </div>
    <div class="info-card">
      <div class="label">Telegram Bot</div>
      <div class="value">Sistem Pakan</div>
    </div>
  </div>
  <div id="toast">Siap</div>
  <script>
    const HOST      = location.hostname;
    const streamUrl = `http://${HOST}:81/stream`;
    const img       = document.getElementById('stream');
    let   uptimeSec = 0;

    function toast(msg) {
      const el = document.getElementById('toast');
      el.textContent = msg;
      el.classList.add('show');
      setTimeout(() => el.classList.remove('show'), 2800);
    }
    function startStream() {
      img.src = streamUrl + '?t=' + Date.now();
      document.getElementById('camstatus').textContent = '🟢 Streaming';
      toast('🟢 Stream dimulai');
    }
    function stopStream() {
      img.src = '';
      document.getElementById('camstatus').textContent = '🔴 Dihentikan';
      toast('🔴 Stream dihentikan');
    }
    function snapshot() {
      toast('📸 Mengambil foto...');
      fetch(`http://${HOST}/capture`)
        .then(r => r.blob())
        .then(blob => {
          const a = document.createElement('a');
          a.href = URL.createObjectURL(blob);
          a.download = `kandang_${Date.now()}.jpg`;
          a.click();
          toast('📥 Foto tersimpan!');
        })
        .catch(() => toast('❌ Gagal mengambil foto'));
    }
    function kirimTelegram() {
      toast('📨 Mengirim ke Telegram...');
      fetch(`http://${HOST}/telegram`)
        .then(r => r.text())
        .then(() => toast('✅ Foto terkirim ke Telegram!'))
        .catch(() => toast('❌ Gagal kirim ke Telegram'));
    }
    setInterval(() => {
      uptimeSec++;
      const h = Math.floor(uptimeSec / 3600);
      const m = Math.floor((uptimeSec % 3600) / 60);
      const s = uptimeSec % 60;
      document.getElementById('uptime').textContent =
        (h > 0 ? h + 'j ' : '') + (m > 0 ? m + 'm ' : '') + s + 'd';
    }, 1000);
    window.onload = () => {
      startStream();
      document.getElementById('camstatus').textContent = '🟢 Streaming';
    };
  </script>
</body>
</html>
)rawliteral";

// ================================================================
// HANDLER HTTP
// ================================================================
static esp_err_t index_handler(httpd_req_t* req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, INDEX_HTML, sizeof(INDEX_HTML) - 1);
}

static esp_err_t capture_handler(httpd_req_t* req) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { httpd_resp_send_500(req); return ESP_FAIL; }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t res = httpd_resp_send(req, (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

static esp_err_t stream_handler(httpd_req_t* req) {
  camera_fb_t* fb  = NULL;
  esp_err_t    res = ESP_OK;
  char part_buf[64];

  digitalWrite(FLASH_LED_PIN, HIGH);
  httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) { res = ESP_FAIL; break; }
    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK)
      res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;
  }

  digitalWrite(FLASH_LED_PIN, LOW);
  return res;
}

static esp_err_t telegram_handler(httpd_req_t* req) {
  sendPhotoTelegram();
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_sendstr(req, "OK");
}

// ================================================================
// KIRIM FOTO KE TELEGRAM
// ================================================================
void sendPhotoTelegram() {
  Serial.println("[TELEGRAM] Mengambil foto...");

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[ERROR] Gagal ambil foto!");
    bot.sendMessage(CHAT_ID, "❌ Gagal mengambil foto dari kamera!", "");
    return;
  }

  _imgBuf = fb->buf;
  _imgLen = fb->len;
  _imgIdx = 0;

  // ✅ Parameter ke-7 adalah GetNextBufferLen callback, BUKAN caption
  bool sent = bot.sendPhotoByBinary(
    CHAT_ID,
    "image/jpeg",
    _imgLen,
    moreDataAvailable,
    getNextByte,
    nullptr,   // getNextBuffer    — tidak dipakai
    nullptr    // getNextBufferLen — tidak dipakai
  );

  // ✅ Return frame buffer SEBELUM reset pointer
  esp_camera_fb_return(fb);
  _imgBuf = nullptr;
  _imgLen = 0;
  _imgIdx = 0;

  if (sent) {
    Serial.println("[OK] Foto berhasil terkirim!");
    String cap  = "🐔 *Sistem Pakan Otomatis & Monitoring Kandang Ayam*\n\n";
    cap        += "📸 Foto kandang berhasil diambil\n";
    cap        += "📶 WiFi    : " + String(WiFi.RSSI()) + " dBm\n";
    cap        += "🌐 IP      : " + WiFi.localIP().toString() + "\n";
    cap        += "⚙️  Sistem  : Berjalan Normal";
    bot.sendMessage(CHAT_ID, cap, "Markdown");
  } else {
    Serial.println("[ERROR] Gagal kirim foto!");
    bot.sendMessage(CHAT_ID, "❌ Gagal mengirim foto. Cek koneksi WiFi.", "");
  }
}

// ================================================================
// WARMUP KAMERA — buang frame gelap di awal
// ================================================================
void warmupCamera() {
  Serial.print("[KAMERA] Warmup");
  for (int i = 0; i < 5; i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
      Serial.print(".");
    }
    delay(200);
  }
  Serial.println(" selesai!");
}

// ================================================================
// START WEB SERVER
// ================================================================
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port    = 80;

  httpd_uri_t index_uri    = { "/",         HTTP_GET, index_handler,    NULL };
  httpd_uri_t capture_uri  = { "/capture",  HTTP_GET, capture_handler,  NULL };
  httpd_uri_t telegram_uri = { "/telegram", HTTP_GET, telegram_handler, NULL };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &telegram_uri);
    Serial.println("[OK] HTTP server port 80 aktif");
  }

  config.server_port = 81;
  config.ctrl_port   = 32769;
  httpd_uri_t stream_uri = { "/stream", HTTP_GET, stream_handler, NULL };
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println("[OK] Stream server port 81 aktif");
  }
}

// ================================================================
// SETUP
// ================================================================
void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.println(" Sistem Pakan Otomatis & Monitoring");
  Serial.println(" Kandang Ayam — ESP32-CAM");
  Serial.println("========================================");

  // ✅ Naikkan frekuensi CPU — harus di dalam setup()
  setCpuFrequencyMhz(240);

  // Flash LED
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);
  pinMode(PIR_PIN, INPUT);

  // ── Konfigurasi Kamera ──────────────────────────────────────
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0  = Y2_GPIO_NUM;
  config.pin_d1  = Y3_GPIO_NUM;
  config.pin_d2  = Y4_GPIO_NUM;
  config.pin_d3  = Y5_GPIO_NUM;
  config.pin_d4  = Y6_GPIO_NUM;
  config.pin_d5  = Y7_GPIO_NUM;
  config.pin_d6  = Y8_GPIO_NUM;
  config.pin_d7  = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    Serial.println("[INFO] PSRAM ditemukan — resolusi SVGA aktif");
    config.frame_size   = FRAMESIZE_SVGA;     // 800x600
    config.jpeg_quality = 8;                  // 0=terbaik, 63=terburuk
    config.fb_count     = 2;
    config.grab_mode    = CAMERA_GRAB_LATEST; // selalu frame terbaru
  } else {
    Serial.println("[WARN] PSRAM tidak ada — resolusi QVGA");
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count     = 1;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] Inisialisasi kamera gagal: 0x%x\n", err);
    delay(5000);
    ESP.restart();
    return;
  }
  Serial.println("[OK] Kamera berhasil diinisialisasi!");

  // ── Pengaturan Sensor Gambar ─────────────────────────────────
  sensor_t* s = esp_camera_sensor_get();

  // Kejernihan & Eksposur
  s->set_brightness(s,  1);              // -2 s/d 2
  s->set_contrast(s,    1);              // -2 s/d 2
  s->set_saturation(s,  0);             // -2 s/d 2
  s->set_sharpness(s,   1);             // ketajaman

  // Auto White Balance
  s->set_whitebal(s,    1);             // AWB ON
  s->set_awb_gain(s,    1);             // AWB gain ON
  s->set_wb_mode(s,     0);             // 0=auto

  // Auto Exposure
  s->set_exposure_ctrl(s, 1);           // auto exposure ON
  s->set_aec2(s,          1);           // AEC DSP ON
  s->set_ae_level(s,      0);           // -2 s/d 2
  s->set_aec_value(s,   300);           // 0-1200

  // Auto Gain
  s->set_gain_ctrl(s,   1);             // auto gain ON
  s->set_agc_gain(s,    0);             // manual gain (jika auto OFF)
  s->set_gainceiling(s, (gainceiling_t)2);

  // Koreksi Piksel & Lensa
  s->set_bpc(s,     1);                 // black pixel correction
  s->set_wpc(s,     1);                 // white pixel correction
  s->set_raw_gma(s, 1);                 // gamma correction
  s->set_lenc(s,    1);                 // lens correction
  s->set_dcw(s,     1);                 // noise reduction

  // Orientasi
  s->set_hmirror(s, 0);                 // 1 = mirror horizontal
  s->set_vflip(s,   0);                 // 1 = flip vertikal

  Serial.println("[OK] Sensor kamera dikonfigurasi!");

  // ── Warmup Kamera ────────────────────────────────────────────
  // ✅ Harus dipanggil di dalam setup(), bukan di luar fungsi
  warmupCamera();

  // ── Koneksi WiFi dengan Timeout ──────────────────────────────
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("[WiFi] Menghubungkan ke ");
  Serial.print(ssid);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // ✅ Timeout 20 detik → restart otomatis
    if (millis() - wifiStart >= 20000) {
      Serial.println("\n[ERROR] WiFi timeout! Restart...");
      delay(3000);
      ESP.restart();
    }
  }

  Serial.println("\n[OK] WiFi terhubung!");
  Serial.print("[INFO] IP Address : ");
  Serial.println(WiFi.localIP());
  Serial.print("[INFO] RSSI       : ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  Serial.print("[INFO] Buka browser: http://");
  Serial.println(WiFi.localIP());

  // ── Telegram SSL ─────────────────────────────────────────────
  clientSecure.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  // ── Start Server ─────────────────────────────────────────────
  // ✅ Harus dipanggil di dalam setup(), bukan di luar fungsi
  startCameraServer();

  // ── Notifikasi Boot ke Telegram ──────────────────────────────
  String bootMsg  = "✅ *Sistem Pakan Otomatis & Monitoring Kandang Ayam*\n\n";
  bootMsg        += "🚀 ESP32-CAM berhasil terhubung!\n";
  bootMsg        += "🌐 IP Address : `" + WiFi.localIP().toString() + "`\n";
  bootMsg        += "📶 WiFi RSSI  : " + String(WiFi.RSSI()) + " dBm\n";
  bootMsg        += "📷 Resolusi   : SVGA (800x600)\n";
  bootMsg        += "🚨 Foto akan dikirim saat PIR mendeteksi gerakan\n\n";
  bootMsg        += "🔗 Dashboard  : http://" + WiFi.localIP().toString();
  bot.sendMessage(CHAT_ID, bootMsg, "Markdown");

  Serial.println("[OK] Sistem siap beroperasi!\n");
}


// ================================================================
// LOOP
// ================================================================
void loop()
{
    // ==========================
    // DETEKSI GERAKAN PIR
    // ==========================
    int pirState = digitalRead(PIR_PIN);

    if (pirState == HIGH && !motionDetected)
    {
        motionDetected = true;

        Serial.println("[PIR] Gerakan terdeteksi!");

        bot.sendMessage(
            CHAT_ID,
            "⚠️ Gerakan terdeteksi!\n📷 Mengambil foto...",
            "");

        delay(500);

        sendPhotoTelegram();
    }

    if (pirState == LOW)
    {
        motionDetected = false;
    }

    // ==========================
    // AUTO RECONNECT WIFI
    // ==========================
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[WARN] WiFi terputus! Menghubungkan ulang...");
        WiFi.reconnect();
        delay(5000);
    }

    delay(100);
}