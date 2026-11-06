# CONTINUOUS SMOKE ANALYSER — MASTER PROJECT CONTEXT

> **Tujuan Dokumen:** Satu fail ringkasan menyeluruh untuk disuap (*feed*) atau diletakkan dalam Antigravity di PC rumah supaya AI memahami keseluruhan latar belakang, litar perkakasan, kod firmware, gaya koding boss, dan status terkini projek tanpa perlu terang dari awal.

---

## 1. Maklumat Asas Projek
* **Syarikat:** Focus Applied Technologies (FAT)
* **Produk:** Continuous Optical Smoke Analyser (Penganalisis Asap Ekzos Diesel Aliran Tertutup)
* **Pengguna Utama:** Mukmin & Boss (Senior Automotive/Embedded Engineer)
* **Prinsip Fizik:** Hukum Beer-Lambert ($I/I_0 = e^{-kL}$)
* **Panjang Kebuk Pengukuran:** Tiub tertutup tetap $L = 21.5\text{ cm} = 0.215\text{ m}$ (sweet spot serapan 50% pada $k = 3.22\text{ m}^{-1}$)
* **Target Mikropengawal:** Microchip PIC18F46K22 @ 16 MHz
* **Papan Litar PCB:** Fuel Stretcher V2.0 PCB
* **Bahasa & IDE:** PICBASIC PRO (PBP 2.60 / PBP3) melalui MicroCode Studio Plus

---

## 2. Peta Sambungan Perkakasan (Fuel Stretcher V2.0 -> Komponen)
* **P1 Pin 15 (`injOut2` / RC2 / Q2 IGBT):** Memacu katod Red LED pemancar cahaya (PWM 2 kHz)
* **P1 Pin 2 (`TPS` / RD4 / AN24):** Isyarat voltan analog daripada sensor fotodetektor OPT101 (melalui U2 Op-Amp)
* **P1 Pin 8 (`Boost` / RA5):** Butang kalibrasi (*Pushbutton* ke +5V dengan perintang pull-down 1.5k $\Omega$ ke GND)
* **P1 Pin 11 (`txRS` / RC6):** Komunikasi Serial Telemetri RS-232 (9600 Baud, 8-N-1 ke PC / COM4 PuTTY)
* **Pin 12:** Bekalan Kuasa +12V (Batt)
* **Pin 3 / 13:** Ground (GND)
* **RD2 (D1):** Onboard Green LED (Status Kalibrasi & Butang)
* **RD3 (D2):** Onboard Red LED (Status Kuasa/PWM)

---

## 3. Kod Firmware Terkini: `SMOKE-1.pbp` (atau `D:\PBP\SMOKE-1.pbp`)
* **Logik Utama (`Main:`):**
  * Kitaran *non-blocking timer* pantas (5ms selang masa: `pause 5`, `time = time + 1`).
  * Telemetri dihantar setiap 500ms (`time = 100`).
  * Bacaan analog sensor OPT101 dipuratakan daripada **16 sampel ADC** untuk mematikan *noise* dan riak PWM.
  * Formula opasiti dikunci (*clamped*) antara 0% (udara bersih) hingga 100% (gelap penuh).
  * Pengiraan nilai pekali serapan **$k$-value ($\text{m}^{-1}$)** secara automatik menggunakan `LOOKUP2` Beer-Lambert bagi tiub $0.215\text{ m}$.
  * Format Telemetri: `Opas Raw: 412 | Opas%: 0% | k: 0.00 m^-1`.

* **Logik Butang (Whiteboard Specification):**
  * **Short Push (< 1.5s):** Fast Re-Zero 0% & 100% (`RECAL`).
  * **Long Push (> 1.5s):** Kalibrasi 3-Titik Penuh (`CAL_3PT`):
    1. *Blink Fast @ 0/100:* Kelip laju 6 kali, ukur `OpasZ` (0% gelap, LED OFF) dan `Duty100` (100% jernih).
    2. *Double-Blink @ 50:* Green LED kelip dua kali berterusan, prompt operator masukkan *50% ND Filter*.
    3. *Button Push:* Operator tekan butang, PIC simpan bacaan `Opas50`.
    4. *Done (Solid LED):* LED hijau menyala mantap 1.5 saat, operator cabut filter, kembali membaca normal.

---

## 4. Peraturan Wajib Gaya Koding Boss (Senior Engineer Style)
Disalin daripada `GEMINI.md` berasaskan rujukan utama `D:\Users\ANG\Desktop\Mukmin\DBW\DBW.pbp`:
1. **Nama Variable Pendek & Terus (1-6 Huruf):**
   * Sensor: `OpasR`, `OpasZ`, `OpasP`, `Opas100`, `Opas50`, `k_val`, `Duty100`, `TPS`, `V1`, `V2`.
   * Butang: `But`, `ButIn`, `ButO`.
   * Kaunter: `i`, `j`, `k`, `t`, `time`.
   * **DILARANG:** Gaya AI meleret / `snake_case` (e.g. `is_button_pressed_flag`, `k_absorption_coefficient_value`).
2. **Tiada Blocking Delay dalam Loop:** Guna kaunter pusingan, elak `PAUSE 500` dalam `Main:`.
3. **Matematik Integer 1-Baris:** Guna skala integer terus dan penapis `tpsav = (3*tpsav + tps)/4`.
4. **Sintaks Padat:** Gunakan tanda `:` untuk baris berkait (`t=0:led1=1`).
5. **Komen Bahasa Inggeris:** Semua komen wajib dalam English dan ringkas.

---

## 5. Status Semasa Projek
* Firmware `SMOKE-1.pbp` telah bebas dari semua bug perkakasan/perisian dan lulus kompilasi 100%.
* Litar butang berfungsi sempurna dengan perintang pull-down 1.5k $\Omega$ ke GND dan butang ke +5V.
* Pembacaan telemetri memaparkan `Opas Raw`, `Opas %`, dan `k-value` dengan stabil tanpa riak atau nombor negatif.
