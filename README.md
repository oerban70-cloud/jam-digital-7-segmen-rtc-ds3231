# Jam Digital 7-Segmen RTC DS3231 — ESP32 + TM1637 + 2 Tombol

Jam dindingmu telat terus, jam ini tetap akurat. Jam digital kecil ini
membaca waktu dari modul RTC DS3231 — sebuah cip jam real-time dengan
baterai CR2032 sendiri — lalu menampilkannya sebagai JJ:MM di modul
7-segmen TM1637. Titik dua di tengah layar berkedip tiap detik, dan dua
tombol mengatur jam dan menitnya. Karena waktunya disimpan di RTC,
bukan di ESP32, listrik padam pun jamnya tetap jalan; saat listrik
menyala lagi, jam langsung tampil benar tanpa diatur ulang. Akurasi
DS3231 sekitar 2 ppm — jam ini melenceng kurang dari dua menit dalam
setahun.

Video Short (60 detik): https://www.youtube.com/shorts/DaE9jqtDS0E

## Cara kerja

1. Modul RTC DS3231 menyimpan waktu (jam, menit, detik, tanggal) dan
   jalan terus dari baterai CR2032-nya sendiri, walau ESP32 mati.
   Akurasinya sekitar 2 ppm dari 0 sampai 40 derajat Celsius — jauh
   lebih akurat daripada jam dinding murah yang cepat melambat.
2. ESP32 membaca waktu dari RTC lewat bus I2C (SDA GPIO21, SCL GPIO22)
   tiap 200 ms memakai library RTClib 2.1.4 (Adafruit). Kalau RTC
   kehilangan daya (misalnya baterai baru pertama kali dipasang), waktu
   diisi dari waktu compile sketch, lalu jalan normal.
3. Waktu ditampilkan di modul TM1637 sebagai JJ:MM (angka = jam x 100
   + menit, titik dua di tengah menyala saat detik genap dan mati saat
   detik ganjil — jadi berkedip tiap detik). Modul TM1637 memakai
   protokol 2 kabel mirip I2C (CLK + DIO) dan didorong library TM1637
   1.2.0 (Avishorp).
4. Dua tombol SET mengatur jam dan menit. Tiap tekan menambah 1 — jam
   0-23 lalu kembali ke 0, menit 0-59 lalu kembali ke 0 dan detik ikut
   direset ke 0 supaya menitnya pas. Tahan tombol 0,5 detik, lalu
   angkanya bertambah berulang tiap 150 ms. Setiap perubahan langsung
   ditulis kembali ke RTC dengan `rtc.adjust()`, jadi jam tetap benar
   walau ESP32 di-restart.
5. Semua tombol aktif LOW (INPUT_PULLUP), jadi tidak butuh resistor
   pull-up eksternal. Debounce 50 ms mengabaikan pantulan tombol.
6. Status JJ:MM:SS dikirim ke Serial Monitor tiap 5 detik (baud
   115200).

Kenapa waktu disimpan di RTC, bukan di ESP32 sendiri? ESP32 tidak
punya jam real-time berbaterai — begitu listrik padam, waktunya hilang
dan harus diatur ulang. DS3231 punya osilator kristal dengan kompensasi
suhu di dalam cipnya, jadi waktunya tetap akurat dan tetap jalan dari
baterai kecil CR2032 selama bertahun-tahun.

## Wiring

| Komponen | Pin modul | ESP32 |
| --- | --- | --- |
| Modul RTC DS3231 SDA (data I2C) | SDA | GPIO21 |
| Modul RTC DS3231 SCL (clock I2C) | SCL | GPIO22 |
| Modul RTC DS3231 VCC | VCC | 3V3 |
| Modul RTC DS3231 GND | GND | GND |
| Modul TM1637 CLK (jam) | CLK | GPIO18 |
| Modul TM1637 DIO (data) | DIO | GPIO19 |
| Modul TM1637 VCC | VCC | 3V3 |
| Modul TM1637 GND | GND | GND |
| Tombol SET jam kaki 1 (aktif LOW) | 1 | GPIO32 (INPUT_PULLUP) |
| Tombol SET jam kaki 2 | 2 | GND |
| Tombol SET menit kaki 1 (aktif LOW) | 1 | GPIO33 (INPUT_PULLUP) |
| Tombol SET menit kaki 2 | 2 | GND |
| ESP32 | USB | Adaptor USB 5V |

Skema lengkap (gaya Fritzing): lihat `schematic.png`.

PERHATIAN: modul TM1637 dan DS3231 dicatu 3V3 dari ESP32 — jangan
mencatu 5V ke pin GPIO ESP32. Kalau layar menampilkan 88:88 terus
atau ESP32 menulis "RTC DS3231 tidak ditemukan!" di Serial Monitor,
periksa dulu wiring SDA/SCL dan catu 3V3-nya.

## Konfigurasi

Di `JamDigital7SegmenRtcDs3231/JamDigital7SegmenRtcDs3231.ino`:

- `PIN_TM1637_CLK` (18) / `PIN_TM1637_DIO` (19) — CLK dan DIO modul TM1637.
- `PIN_RTC_SDA` (21) / `PIN_RTC_SCL` (22) — bus I2C modul DS3231.
- `PIN_TOMBOL_JAM` (32) / `PIN_TOMBOL_MENIT` (33) — tombol SET, aktif LOW.
- `JAM_MAKSIMAL` (23) / `MENIT_MAKSIMAL` (59) — batas atas pengaturan.
- `KECERAHAN_LAYAR` (7) — kecerahan TM1637, 0-7.
- `BIT_TITIK_DUA` (0x40) — bit colon di tengah layar TM1637.
- `DEBOUNCE_TOMBOL_MS` (50) — abaikan pantulan tombol.
- `TUNDA_REPEAT_MS` (500) / `INTERVAL_REPEAT_MS` (150) — tahan tombol
  0,5 detik, lalu tambah berulang tiap 150 ms.
- `INTERVAL_BACA_MS` (200) — baca RTC tiap 200 ms.
- `INTERVAL_STATUS_MS` (5000) — status JJ:MM:SS di Serial Monitor.
- `BAUD_SERIAL` (115200).

## Komponen

- ESP32 DevKit
- Modul RTC DS3231 (dengan baterai CR2032)
- Modul TM1637 4 digit 7-segmen
- 2x tombol tekan (push button)
- Adaptor USB 5V (catu ESP32)

## Verifikasi compile

Terverifikasi compile dengan Arduino CLI 1.6.0, ESP32 core 4.0.0,
board `esp32:esp32:esp32`:

- Sketch memakai 319.925 byte (24%) dari program storage (maksimal 1.310.720 byte).
- Variabel global memakai 24.744 byte (7%) dari dynamic memory (maksimal 327.680 byte).
- Library eksternal: RTClib 2.1.4 (Adafruit) dan TM1637 1.2.0 (Avishorp)
  — lihat `libraries/RTClib.zip` dan `libraries/TM1637.zip`.

## Lisensi

MIT — lihat `LICENSE`.
