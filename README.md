# Continuous Smoke Analyser

**Focus Applied Technologies (FAT)**  
Continuous optical smoke opacity meter and absorption coefficient ($k$-value) analyser for diesel engines.

---

## 📌 Project Overview
- **Microcontroller:** Microchip PIC18F46K22 @ 16 MHz
- **Hardware Board:** Fuel Stretcher V2.0 PCB
- **Language / Toolchain:** PICBASIC PRO (PBP 2.60 / PBP3) & MicroCode Studio Plus
- **Measurement Chamber:** Closed tube with fixed optical path length $L = 21.5\text{ cm} (0.215\text{ m})$
- **Physics Principle:** Beer-Lambert Law ($I/I_0 = e^{-kL}$)

---

## 📂 Repository Contents
- **`SMOKE-1.pbp`**: Primary PICBASIC PRO firmware with non-blocking timing, 16-sample ADC oversampling, Beer-Lambert $k$-value lookup, and whiteboard-compliant 3-point calibration.
- **`GEMINI.md`**: Firmware coding standards modeled after senior engineering standards (`DBW.pbp`). Automatically loaded as instructions by Antigravity AI.
- **`PROJECT_CONTEXT.md`**: Comprehensive briefing covering hardware pinouts, circuit physics, calibration SOP, and firmware architecture.
- **`reference/`**: Reference firmware files (`DBW.pbp`) illustrating the company's established coding standards.
- **`fat_smoke_oled_meter/`**: Secondary display / meter code.
- **Schematics & Diagrams**: Wiring, pinouts, and PCB images for Fuel Stretcher V2.0.

---

## 🚀 Working with Antigravity on Any PC
When cloning this repository to a new computer:
1. Clone the repo:
   ```bash
   git clone <repo-url>
   ```
2. Open the project folder in **Antigravity**.
3. Antigravity automatically reads `GEMINI.md` and `PROJECT_CONTEXT.md`, instantly adopting all boss coding rules, hardware mapping, and full project context without needing re-explanation!
