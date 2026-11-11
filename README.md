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
- **`firmware/`**:
  - `SMOKE-1.pbp`: Baseline PICBASIC PRO bench test firmware (stable 6 Nov 2026 version).
  - `SMOKE-2.pbp`: Hardware-verified PICBASIC PRO firmware with non-blocking timing, 8-bit duty cycle scan, 16-sample ADC oversampling, Beer-Lambert $k$-value lookup table ($L = 0.215\text{ m}$), and whiteboard-compliant 3-point calibration (0%, 100%, 50% ND filter).
  - `fat_smoke_oled_meter/`: Secondary OLED dashboard display sketch (Arduino Nano).
  - `legacy/`: Historical C and PBP firmware backups.
- **`hardware/`**:
  - `schematics/`: Fuel Stretcher V2.0 circuit schematics, PDF schematics, and pinout diagrams.
  - `guides/`: Hardware modification, jumper, and soldering guides (SW1, SW2, Q2, R4/R6 bridge).
  - `photos/`: PCB inspection macro photos, microscope captures, and whiteboard notes.
- **`mechanical/`**: SolidWorks 3D CAD sketches, PTFE optical housing dimensions, and product render concepts.
- **`docs/`**: Project documentation, Word reports, and component costing spreadsheets.
- **`tools/`**: Flashing scripts (`flash_pic.bat`, `upload_oled.bat`), compiler configurations, and Python guide generators.
- **`reference/`**: Senior engineering reference firmware (`DBW.pbp`).
- **`GEMINI.md`**: Firmware coding standards modeled after senior engineering style (`DBW.pbp`).
- **`PROJECT_CONTEXT.md`**: Comprehensive briefing covering hardware pinouts, circuit physics, calibration SOP, and firmware architecture.
- **`PUSH_TO_GITHUB.bat`**: One-click batch utility to push commits to GitHub.

---

## 🚀 Working with Antigravity on Any PC
When cloning this repository to a new computer:
1. Clone the repo:
   ```bash
   git clone <repo-url>
   ```
2. Open the project folder in **Antigravity**.
3. Antigravity automatically reads `GEMINI.md` and `PROJECT_CONTEXT.md`, instantly adopting all boss coding rules, hardware mapping, and full project context without needing re-explanation!
