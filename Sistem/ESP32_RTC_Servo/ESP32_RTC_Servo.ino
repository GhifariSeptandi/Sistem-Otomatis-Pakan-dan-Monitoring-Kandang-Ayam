#include <Wire.h>
#include <RTClib.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <time.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID "Tenda03"
#define WIFI_PASSWORD "BRHtenda68"

#define API_KEY "AIzaSyAiAN7Vu0ShWGy-VowU1GX54aKAOV22p2s"
#define DATABASE_URL "https://tugas-akhir-c0452-default-rtdb.asia-southeast1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

RTC_DS3231 rtc;
Servo servoMotor;

const int servoPin = 18;

// Status agar servo tidak bergerak berulang dalam menit yang sama
bool sudahJam7 = false;
bool sudahJam12 = false;
bool sudahJam18 = false;

void setup() {
  Serial.begin(115200);

WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

while (WiFi.status() != WL_CONNECTED) {
  Serial.print(".");
  delay(500);
}

Serial.println("WiFi Connected");

config.api_key = API_KEY;
config.database_url = DATABASE_URL;

if (Firebase.signUp(&config, &auth, "", "")) {
  Serial.println("Firebase SignUp OK");
} else {
  Serial.printf("SignUp Error: %s\n",
                config.signer.signupError.message.c_str());
}

config.token_status_callback = tokenStatusCallback;

Firebase.begin(&config, &auth);
Firebase.reconnectWiFi(true);

Serial.println("Menunggu Firebase Ready...");

unsigned long timeout = millis();

while (!Firebase.ready()) {
  if (millis() - timeout > 10000) {
    Serial.println("Firebase timeout!");
    break;
  }
  delay(100);
}

Serial.println("Firebase Ready!");

if (Firebase.RTDB.setString(&fbdo, "/test", "ESP32 Connected")) {
  Serial.println("Firebase Write Success");
} else {
  Serial.print("Firebase Error: ");
  Serial.println(fbdo.errorReason());
}

Serial.println("Database URL:");
Serial.println(config.database_url.c_str());

Serial.println("API Key:");
Serial.println(config.api_key.c_str());

Serial.println("Firebase Connected");

if (Firebase.RTDB.setString(&fbdo, "/test", "ESP32 Connected")) {
  Serial.println("Firebase Write Success");
} else {
  Serial.println(fbdo.errorReason());
}

Wire.begin(21, 22);

if (!rtc.begin()) {
  Serial.println("RTC tidak terdeteksi!");
  while (1);
}

// Sinkronisasi waktu internet (WIB)
configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");

if (rtc.lostPower()) {

  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");


  struct tm timeinfo;

  if (getLocalTime(&timeinfo)) {

    rtc.adjust(DateTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec
    ));

    Serial.println("RTC berhasil disinkronkan");
  } else {
    Serial.println("Gagal mendapatkan waktu dari NTP");
  }
}

servoMotor.attach(servoPin);
servoMotor.write(0);

Serial.println("Sistem Pakan Otomatis Siap");
}

void loop() {
  DateTime now = rtc.now();

  int jam = now.hour();
  int menit = now.minute();
  int detik = now.second();

  Serial.printf(
    "%02d:%02d:%02d\n",
    now.hour(),
    now.minute(),
    now.second()
  );

  // Jadwal 07:00
  if (jam == 7 && menit == 0 && !sudahJam7) {
    gerakServo();
    sudahJam7 = true;
  }

  // Jadwal 12:00
  if (jam == 12 && menit == 0 && !sudahJam12) {
    gerakServo();
    sudahJam12 = true;
  }

  // Jadwal 18:00
  if (jam == 18 && menit == 00 && !sudahJam18) {
    gerakServo();
    sudahJam18 = true;
  }

  if (jam == 0 && menit == 0) {
    sudahJam7 = false;
    sudahJam12 = false;
    sudahJam18 = false;
  }

  delay(1000);
}

void gerakServo() {

  Serial.println("Waktu pemberian pakan!");

  Serial.print("WiFi Status sebelum Firebase: ");
  Serial.println(WiFi.status());

  if (Firebase.RTDB.setString(&fbdo, "/servo/status", "MENYALA")) {
    Serial.println("Status MENYALA terkirim");
  } else {
    Serial.println(fbdo.errorReason());
  }

  servoMotor.write(0);
  delay(1000);

  servoMotor.write(29);
  delay(1000);

  servoMotor.write(67);
  delay(1000);

  servoMotor.write(108);
  delay(1000);

  servoMotor.write(144);
  delay(1000);

  servoMotor.write(0);
  delay(1000);

  Serial.print("WiFi Status setelah servo: ");
  Serial.println(WiFi.status());

  if (Firebase.RTDB.setString(&fbdo, "/servo/status", "MATI")) {
    Serial.println("Status MATI terkirim");
  } else {
    Serial.println(fbdo.errorReason());
  }

  Serial.println("Pakan selesai diberikan");
}