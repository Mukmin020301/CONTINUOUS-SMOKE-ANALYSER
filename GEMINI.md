# Focus Applied Technologies (FAT) — Firmware Coding Standards

This project uses PICBASIC PRO (PBP) and Microchip PIC microcontrollers (e.g. PIC18F46K22, PIC18F26K22) for automotive and industrial test equipment.
All generated and modified code MUST strictly adhere to the senior engineering coding style modeled after `D:\Users\ANG\Desktop\Mukmin\DBW\DBW.pbp`.

---

## 1. Variable Naming (Short, Physical & Direct)
- **STRICTLY FORBIDDEN**: Long, verbose, AI-style names or snake_case identifiers (e.g. `is_button_pressed_flag`, `btn_hold_duration_counter`, `k_absorption_coefficient_value`, `filtered_optical_adc_reading`).
- **MANDATORY**: Short, concise 1-6 character names directly representing hardware, physical units, or standard abbreviations:
  - **Sensors & Voltages**: `TPS`, `TPSav`, `TPSO`, `TPSn`, `OpasR` (Raw), `OpasZ` (Zero/Dark), `OpasP` (Percent), `Opas100`, `Opas50`, `Duty100`, `kVal`, `V1`, `V2`, `V3`, `V4`, `Vi` (Idle Volts), `Vw` (WOT Volts).
  - **Buttons & Switches**: `But`, `ButIn`, `ButO` (Old/Previous), `Si`, `Sw`, `Sii`, `Swi`, `SIO`, `SWO`.
  - **Counters & Temporaries**: `i`, `j`, `n`, `t`, `time`, `num`, `pct`, `cyc`.

---

## 2. Non-Blocking Loop Timing (No Blind Delays)
- **NEVER** place long blocking delays (e.g. `PAUSE 500`, `PAUSE 1000`) inside the main loop.
- Use fast, non-blocking tick counters so inputs (pushbuttons, switches) remain responsive within 5-10ms:
  ```pbp
  PAUSE 5
  time = time + 1
  IF time = 100 THEN
      time = 0
      ' Periodic 500ms tasks (ADC report, telemetry, etc.)
  ENDIF
  ```

---

## 3. Fast Embedded Math & Filtering (Single-Line Integer)
- Avoid floating-point arithmetic, complex math libraries, and large array buffers.
- Use single-line exponential moving average filters for analog smoothing:
  ```pbp
  tpsav = (3 * tpsav + tps) / 4
  ```
- Use direct integer scaling:
  ```pbp
  TPSO = V1 + (V2 - V1) * tps / 255
  opasp = (Opas100 - OpasR) * 100 / (Opas100 - OpasZ)
  ```

---

## 4. Code Formatting & Control Flow
- Use multi-statement colon `:` syntax for closely related operations:
  ```pbp
  t = 0 : led1 = 1
  led1 = 1 - led1 : portc.2 = 0
  ```
- Prefer clean, pragmatic control flow (`SELECT CASE`, simple labels, `GOTO`).
- Serial telemetry (`HSEROUT`): Keep compact and clean using `#` shortcut for `DEC` (e.g. `HSEROUT [#tps, ",", #tpso, 13]`).
- Comments: Must be concise, strictly in **English**, and focused on hardware connections or functional notes.

---

## 5. Reference Codebase
- Primary style reference: `reference/DBW.pbp` (or `D:\Users\ANG\Desktop\Mukmin\DBW\DBW.pbp`)

---

## 6. Versioning & Development Workflow
- **NEVER** overwrite the stable baseline `SMOKE-1.pbp` directly when experimenting or adding new features.
- Always create a new file (e.g. `SMOKE-2.pbp`) for new modifications, tests, or bug fixes.
- Only merge back into `SMOKE-1.pbp` once the code is 100% verified, tested on hardware, and approved by the user.
