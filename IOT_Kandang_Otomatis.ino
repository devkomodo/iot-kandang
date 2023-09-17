#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <NewPing.h>
#include <Servo.h>
#include <DHT.h>
#include <BlynkSimpleEsp8266.h>

// Definisi konstanta dan variabel global

// Blynk settings
char auth[] = "AUTH_TOKEN";
BlynkTimer timer;

// WiFi
const char* ssid = "SSID";
const char* password = "PASSWORD";

// NTP (Network Time Protocol)
const char* ntpServer = "id.pool.ntp.org";
const int ntpOffset = 7 * 3600; // Offset waktu Indonesia (WIB)
const int ntpUpdateInterval = 3600000; // Sinkronisasi NTP setiap 1 jam (3600 detik * 1000 ms)

// Sensor Ultrasonik
const int ultrasonic1TrigPin = D1;
const int ultrasonic1EchoPin = D2;
const int ultrasonic2TrigPin = D3;
const int ultrasonic2EchoPin = D4;

// Servo untuk pakan
const int servoPin = D5;
const int openPosition = 0; // Sudut untuk posisi terbuka pakan
const int closePosition = 90; // Sudut untuk posisi tertutup pakan
const int feedingDuration = 2000; // Durasi buka pakan dalam milidetik (ms)

// Sensor DHT11 untuk suhu dan kelembaban
const int dhtPin = D6;
DHT dht(dhtPin, DHT11);

// Relay (Lampu dan Blower)
const int Lampu = D0; // Lampu Penghangat
const int Blower = D7; // Blower Pendingin

// Setting Ketinggian Wadah Pakan & Tangki AIR
const int maxWaterLevel = 20; // Tinggi maksimum tangki air dalam cm
const int maxFoodLevel = 10; // Tinggi maksimum level wadah pakan dalam cm
const int notifyFoodLevel = 10;   // Nilai ambang batas untuk mengirimkan notifikasi dalam persen
const int notifyWaterLevel = 10; // Nilai ambang batas untuk mengirimkan notifikasi dalam persen

// Setting Temperatur Kandang
const int minTemp = 30; // Temperatur Minimal
const int normalTemp = 35; // Temperatur Ideal / Normal
const int maxTemp = 40; // Temperatur Maksimal

// Variabel global
WiFiUDP udp;
NTPClient ntpClient(udp, ntpServer, ntpOffset, ntpUpdateInterval);
NewPing ultrasonic1(ultrasonic1TrigPin, ultrasonic1EchoPin, 200);
NewPing ultrasonic2(ultrasonic2TrigPin, ultrasonic2EchoPin, 200);
Servo feederServo;
unsigned long lastFeedingTime = 0;
int stateLampu, stateBlower;
float temperature, humidity;
unsigned long feedingInterval = 60 * 60 * 1000; // Interval 1 jam untuk pemberian pakan (dalam milidetik)
unsigned long lastFeeding = 0;

void setup() {
  Serial.begin(9600);

  // Menghubungkan ke WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Menghubungkan ke WiFi...");
  }
  Serial.println("Terhubung ke WiFi");
  Serial.println("==== INISIALISASI =====");

  // Menginisialisasi NTP client
  ntpClient.begin();
  Serial.println("NTP CLIENT DIMULAI");

  // Menginisialisasi servo
  feederServo.attach(servoPin);
  feederServo.write(closePosition); // Pastikan pakan dalam posisi tertutup saat pertama kali dijalankan
  Serial.println("SERVO TERPASANG");

  // Menginisialisasi sensor DHT
  dht.begin();
  Serial.println("DHT DIMULAI");

  // Menginisialisasi pin relay
  pinMode(Lampu, OUTPUT);
  pinMode(Blower, OUTPUT);

  // Inisialisasi Blynk
  Blynk.begin(auth, ssid, password);
  timer.setInterval(1000L, monitorEnvironment); // Refresh data setiap 1 detik
  timer.setInterval(feedingInterval, checkFeedingSchedule); // Jadwal pemberian pakan
}

void loop() {
  Blynk.run();
  ntpClient.update();
  timer.run();
  checkBlynkConnection(); // Periksa koneksi Blynk
}

// Fungsi untuk menangani tombol Blynk untuk memberi makan secara manual
BLYNK_WRITE(V0) {
  int feedingButton = param.asInt();
  if (feedingButton == 1) {
    feeder();
    Blynk.virtualWrite(V0, 0); // Reset status tombol di aplikasi
  }
}

// Fungsi untuk memeriksa jadwal pemberian pakan
void checkFeedingSchedule() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastFeeding >= feedingInterval) {
    feeder();
    lastFeeding = currentMillis;
  }
}

// Fungsi untuk memberi makan
void feeder() {
  Serial.println("Memberi makan..");
  feederServo.write(openPosition);
  delay(feedingDuration);
  feederServo.write(closePosition);
  lastFeedingTime = millis();
}

// Fungsi untuk memeriksa suhu dan mengontrol lampu/blower
void checkTemperature() {
  if (temperature < minTemp) {
    // Hidupkan lampu & matikan blower
    digitalWrite(Lampu, HIGH);
    digitalWrite(Blower, LOW);
    Blynk.notify("Temperatur Minimum, Menyalakan Lampu."); // Kirim notifikasi ke aplikasi Blynk
    stateLampu = 1;
    stateBlower = 0;
  } else if (temperature > maxTemp) {
    // Hidupkan blower & matikan lampu
    digitalWrite(Lampu, LOW);
    digitalWrite(Blower, HIGH);
    Blynk.notify("Temperatur Maximum, Menyalakan Blower."); // Kirim notifikasi ke aplikasi Blynk
    stateLampu = 0;
    stateBlower = 1;
  } else if (temperature >= minTemp && temperature <= maxTemp) {
    // Matikan lampu & blower
    digitalWrite(Lampu, LOW);
    digitalWrite(Blower, LOW);
    stateLampu = 0;
    stateBlower = 0;
  }
}

// Fungsi untuk memantau lingkungan
void monitorEnvironment() {
  // Baca data dari sensor DHT
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  // Baca data dari sensor ultrasonik untuk level air dan pakan
  int waterLevel = ultrasonic1.ping_cm();
  int foodLevel = ultrasonic2.ping_cm();

  // Hitung persentase level air
  int waterLevelPercentage = map(waterLevel, 0, maxWaterLevel, 0, 100);
  int foodLevelPercentage = map(foodLevel, 0, maxFoodLevel, 0, 100);

  // Tampilkan hasil pembacaan di Serial Monitor
  Serial.print("Suhu: ");
  Serial.print(temperature);
  Serial.print("°C | Kelembaban: ");
  Serial.print(humidity);
  Serial.print("% | Lampu : ");
  Serial.print(stateLampu);
  Serial.print(" | Blower : ");
  Serial.print(stateBlower);
  Serial.print(" | Level Air: ");
  Serial.print(waterLevelPercentage);
  Serial.print("% | Level Pakan: ");
  Serial.print(foodLevelPercentage);
  Serial.println("%");

  // Kirim data ke Blynk
  Blynk.virtualWrite(V1, temperature); // Widget di Blynk
  Blynk.virtualWrite(V2, humidity); // Widget di Blynk
  Blynk.virtualWrite(V3, stateLampu); // Widget di Blynk
  Blynk.virtualWrite(V4, stateBlower); // Widget di Blynk
  Blynk.virtualWrite(V5, waterLevelPercentage); // Widget di Blynk
  Blynk.virtualWrite(V6, foodLevelPercentage); // Widget di Blynk
}

// Fungsi untuk memeriksa koneksi ke server Blynk
void checkBlynkConnection() {
  if (!Blynk.connected()) {
    Serial.println("Blynk Terputus. Menghubungkan kembali...");
    if (Blynk.connect()) {
      Serial.println("Terhubung kembali ke Server Blynk.");
    } else {
      Serial.println("Gagal menghubungkan ulang.");
    }
  }
}
