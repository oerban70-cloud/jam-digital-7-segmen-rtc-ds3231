// JamDigital7SegmenRtcDs3231.ino — Jam digital JJ:MM dengan RTC DS3231
//
// ESP32 DevKit membaca waktu real-time dari modul RTC DS3231 (I2C) dan
// menampilkannya sebagai JJ:MM di modul 7-segmen TM1637. Titik dua di
// tengah layar berkedip tiap detik. Dua tombol mengatur jam dan menit;
// setiap perubahan langsung ditulis kembali ke RTC, jadi jam tetap
// akurat walau listrik padam — RTC jalan terus dari baterai CR2032-nya
// dan hanya melenceng sekitar 2 ppm (kurang dari dua menit setahun).
//
// Library yang dipakai:
//   - RTClib 2.1.4 (Adafruit) — driver RTC DS3231 via I2C.
//     Pasang lewat Library Manager Arduino IDE / arduino-cli:
//     arduino-cli lib install "RTClib"
//     (header yang dipakai: RTClib.h, kelas RTC_DS3231)
//   - TM1637 1.2.0 (Avishorp) — driver modul 4 digit 7-segmen TM1637.
//     Pasang lewat Library Manager Arduino IDE / arduino-cli:
//     arduino-cli lib install "TM1637"
//     (header yang dipakai: TM1637Display.h, kelas TM1637Display)
//
// Perilaku tombol (semua aktif LOW, INPUT_PULLUP):
//   - SET jam   : tiap tekan +1 jam (0..23, lalu kembali ke 0).
//   - SET menit : tiap tekan +1 menit (0..59, lalu kembali ke 0),
//                 detik ikut direset ke 0 supaya menitnya pas.
//   - Tahan tombol = tambah berulang (auto-repeat tiap 150 ms).
//
// Wiring (lihat schematic.png):
//   DS3231 SDA -> GPIO21, SCL -> GPIO22, VCC -> 3V3, GND -> GND
//   TM1637 CLK -> GPIO18, DIO -> GPIO19, VCC -> 3V3, GND -> GND
//   Tombol SET jam   kaki 1 -> GPIO32, kaki 2 -> GND
//   Tombol SET menit kaki 1 -> GPIO33, kaki 2 -> GND
//   ESP32 dicatu USB 5V; DS3231 dan TM1637 dicatu 3V3.

#include <RTClib.h>
#include <TM1637Display.h>
#include <Wire.h>

// ---------------- Pin ----------------
const uint8_t PIN_TM1637_CLK = 18;   // CLK modul TM1637
const uint8_t PIN_TM1637_DIO = 19;   // DIO modul TM1637
const uint8_t PIN_RTC_SDA = 21;      // SDA modul DS3231 (I2C default ESP32)
const uint8_t PIN_RTC_SCL = 22;      // SCL modul DS3231 (I2C default ESP32)
const uint8_t PIN_TOMBOL_JAM = 32;   // SET jam, aktif LOW
const uint8_t PIN_TOMBOL_MENIT = 33; // SET menit, aktif LOW

// ---------------- Konstanta tampilan ----------------
const uint8_t KECERAHAN_LAYAR = 7;  // 0..7, 7 = paling terang
const uint8_t BIT_TITIK_DUA = 0x40; // bit colon (tengah) TM1637
const uint8_t JAM_MAKSIMAL = 23;    // jam tertinggi (format 24 jam)
const uint8_t MENIT_MAKSIMAL = 59;  // menit tertinggi

// ---------------- Konstanta tombol ----------------
const unsigned long DEBOUNCE_TOMBOL_MS = 50;  // abaikan pantulan tombol
const unsigned long TUNDA_REPEAT_MS = 500;    // tahan 0,5 dtk baru repeat
const unsigned long INTERVAL_REPEAT_MS = 150; // repeat tiap 150 ms

// ---------------- Konstanta baca waktu ----------------
const unsigned long INTERVAL_BACA_MS = 200;    // baca RTC tiap 200 ms
const unsigned long INTERVAL_STATUS_MS = 5000; // status Serial tiap 5 detik
const unsigned long BAUD_SERIAL = 115200;

RTC_DS3231 rtc;
TM1637Display layar(PIN_TM1637_CLK, PIN_TM1637_DIO);

// Status tombol (debounce + auto-repeat saat ditahan).
struct StatusTombol {
  uint8_t pin;
  bool ditekanSebelumnya;       // level LOW yang stabil sebelumnya
  unsigned long waktuPerubahan; // millis() saat level stabil terakhir berubah
  unsigned long waktuTekanMulai; // millis() saat tombol mulai ditekan
  unsigned long waktuRepeatTerakhir; // millis() repeat terakhir
};

StatusTombol tombolJam = {PIN_TOMBOL_JAM, false, 0, 0, 0};
StatusTombol tombolMenit = {PIN_TOMBOL_MENIT, false, 0, 0, 0};

unsigned long waktuBacaTerakhir = 0;
unsigned long waktuStatusTerakhir = 0;
uint8_t jamTerakhir = 255;   // jam yang terakhir ditampilkan (255 = belum ada)
uint8_t menitTerakhir = 255; // menit yang terakhir ditampilkan
uint8_t detikTerakhir = 255; // detik yang terakhir dibaca

// ---------------- Tampilan ----------------
// Menampilkan JJ:MM; titik dua menyala saat detik genap, mati saat ganjil
// (berkedip tiap detik).
void tampilkanJam(uint8_t jam, uint8_t menit, uint8_t detik) {
  uint8_t titikDua = (detik % 2 == 0) ? BIT_TITIK_DUA : 0;
  layar.showNumberDecEx(jam * 100 + menit, titikDua, true, 4, 0);
}

// ---------------- Tombol ----------------
// Mengembalikan true bila tombol ini memicu satu langkah tambah
// (tekan baru, atau repeat saat ditahan).
bool tombolMemilikiLangkah(StatusTombol &tombol) {
  bool ditekan = digitalRead(tombol.pin) == LOW;
  unsigned long sekarang = millis();
  if (ditekan != tombol.ditekanSebelumnya) {
    // Level berubah: tunggu sampai stabil (debounce).
    if (sekarang - tombol.waktuPerubahan >= DEBOUNCE_TOMBOL_MS) {
      tombol.ditekanSebelumnya = ditekan;
      tombol.waktuPerubahan = sekarang;
      if (ditekan) {
        tombol.waktuTekanMulai = sekarang;
        tombol.waktuRepeatTerakhir = sekarang;
        return true; // tekan baru = satu langkah
      }
    }
    return false;
  }
  tombol.waktuPerubahan = sekarang;
  if (ditekan && sekarang - tombol.waktuTekanMulai >= TUNDA_REPEAT_MS &&
      sekarang - tombol.waktuRepeatTerakhir >= INTERVAL_REPEAT_MS) {
    tombol.waktuRepeatTerakhir = sekarang;
    return true; // tahan lama = repeat
  }
  return false;
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(BAUD_SERIAL);
  pinMode(PIN_TOMBOL_JAM, INPUT_PULLUP);
  pinMode(PIN_TOMBOL_MENIT, INPUT_PULLUP);
  layar.setBrightness(KECERAHAN_LAYAR);
  Wire.begin(PIN_RTC_SDA, PIN_RTC_SCL);
  if (!rtc.begin()) {
    Serial.println("RTC DS3231 tidak ditemukan! Periksa wiring SDA/SCL.");
    layar.showNumberDecEx(8888, BIT_TITIK_DUA, true); // kode error di layar
    while (true) {
      delay(1000);
    }
  }
  if (rtc.lostPower()) {
    // Baterai RTC habis / pertama kali dipasang: isi dari waktu compile.
    Serial.println("RTC kehilangan daya, isi dari waktu compile.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.println("Jam digital RTC DS3231 siap.");
}

// ---------------- Loop ----------------
void loop() {
  unsigned long sekarang = millis();

  // Tombol SET jam: +1 jam, kembali ke 0 setelah 23.
  if (tombolMemilikiLangkah(tombolJam)) {
    DateTime t = rtc.now();
    uint8_t jamBaru = (t.hour() + 1) % (JAM_MAKSIMAL + 1);
    rtc.adjust(DateTime(t.year(), t.month(), t.day(), jamBaru, t.minute(),
                        t.second()));
    Serial.print("Jam diatur ke ");
    Serial.println(jamBaru);
  }

  // Tombol SET menit: +1 menit, kembali ke 0 setelah 59; detik direset ke 0.
  if (tombolMemilikiLangkah(tombolMenit)) {
    DateTime t = rtc.now();
    uint8_t menitBaru = (t.minute() + 1) % (MENIT_MAKSIMAL + 1);
    rtc.adjust(DateTime(t.year(), t.month(), t.day(), t.hour(), menitBaru, 0));
    Serial.print("Menit diatur ke ");
    Serial.println(menitBaru);
  }

  // Baca RTC dan perbarui layar tiap 200 ms (cukup cepat untuk kedip 1 detik).
  if (sekarang - waktuBacaTerakhir >= INTERVAL_BACA_MS) {
    waktuBacaTerakhir = sekarang;
    DateTime t = rtc.now();
    if (t.hour() != jamTerakhir || t.minute() != menitTerakhir ||
        t.second() != detikTerakhir) {
      jamTerakhir = t.hour();
      menitTerakhir = t.minute();
      detikTerakhir = t.second();
      tampilkanJam(jamTerakhir, menitTerakhir, detikTerakhir);
    }
  }

  // Status berkala ke Serial Monitor (tiap 5 detik).
  if (sekarang - waktuStatusTerakhir >= INTERVAL_STATUS_MS) {
    waktuStatusTerakhir = sekarang;
    DateTime t = rtc.now();
    Serial.print("Waktu sekarang: ");
    if (t.hour() < 10) Serial.print('0');
    Serial.print(t.hour());
    Serial.print(':');
    if (t.minute() < 10) Serial.print('0');
    Serial.print(t.minute());
    Serial.print(':');
    if (t.second() < 10) Serial.print('0');
    Serial.println(t.second());
  }
}
