/*
 * ============================================================================
 * FAT SMOKE 1.0 - OLED DASHBOARD DISPLAY (ARDUINO NANO - U8G2 EDITION)
 * Target: Arduino Nano (ATmega328P @ 16MHz)
 * Display: 0.96" OLED (128x64, 7-Pin SPI Mode - SSD1306 / SH1106)
 * Library: U8g2 (Ultra-fast, only 128 bytes RAM, 100% reliable)
 * Input: Serial from Fuel Stretcher V2.0 (Pin 11 txRS @ 9600 on Pin D2)
 * ============================================================================
 * 
 * WIRING GUIDE:
 * -------------------------------------------------------------
 * OLED 7-Pin SPI        -->   Arduino Nano
 * -------------------------------------------------------------
 * GND                   -->   GND
 * VCC                   -->   5V
 * D0 (SCK/Clock)        -->   D13 (Hardware SPI SCK)
 * D1 (MOSI/Data)        -->   D11 (Hardware SPI MOSI)
 * RES (Reset)           -->   D8
 * DC (Command/Data)     -->   D7
 * CS (Chip Select)      -->   D6
 * -------------------------------------------------------------
 * Fuel Stretcher V2.0   -->   Arduino Nano
 * -------------------------------------------------------------
 * Pin 3 (GND)           -->   GND
 * Pin 11 (txRS)         -->   D2 (SoftwareSerial RX)
 * ============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>

// Correct constructor for SH1106 128x64 OLED Controller (Hardware SPI)
// Pins: CS=6, DC=7, RESET=8 (D0 is D13 SCK, D1 is D11 MOSI)
U8G2_SH1106_128X64_NONAME_1_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 6, /* dc=*/ 7, /* reset=*/ 8);

// SoftwareSerial on Pin D2 (RX) with INVERSE LOGIC = true for direct RS-232 from SP3232 (Pin 11 txRS)
SoftwareSerial picSerial(2, 3, true);

// Serial buffer
char rxBuffer[100];
uint8_t rxIndex = 0;
uint32_t lastDataTime = 0;

// Live Telemetry Values
float   g_opacity   = 0.0;
float   g_k_val     = 0.0;
float   g_v_sens    = 0.0;
float   g_temp_c    = 25.0;
uint8_t g_err_code  = 0;
char    g_state_str[8] = "MEAS";
bool    g_hasData   = false;

void parseTelemetry(const char* line);
void drawScreen(float opacity, float k, float v_sens, float temp, uint8_t err, const char* state, bool signalLoss);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  Serial.println(F("[BOOT] FAT SMOKE 1.0 - SH1106 OLED Initializing..."));

  // 1. Initialize U8g2 Display with Hardware SPI
  u8g2.begin();
  u8g2.setContrast(255);

  // 2. Render initial Dashboard immediately
  drawScreen(0.0, 0.00, 0.00, 25.0, 0, "INIT", true);
  Serial.println(F("[BOOT] Dashboard rendered on screen."));

  // 3. Start reading PIC RS-232 telemetry
  picSerial.begin(9600);
  Serial.println(F("[BOOT] PIC Serial listening on Pin D2."));
}

void loop() {
  bool packetReady = false;

  // 1. Read incoming serial stream from Fuel Stretcher
  while (picSerial.available() > 0) {
    char c = picSerial.read();
    Serial.write(c); // Echo directly to PC Serial Monitor for live viewing

    if (c == '[') {
      // Synchronize to start of telemetry packet
      rxIndex = 0;
      rxBuffer[rxIndex++] = c;
    } else if (c == '\n' || c == '\r') {
      if (rxIndex > 15) {
        rxBuffer[rxIndex] = '\0';
        parseTelemetry(rxBuffer);
        lastDataTime = millis();
        g_hasData = true;
        packetReady = true;
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); // Toggle LED on each packet
      }
      rxIndex = 0;
    } else {
      if (rxIndex < sizeof(rxBuffer) - 1) {
        rxBuffer[rxIndex++] = c;
      }
    }
  }

  // 2. Refresh Screen ONLY when new packet arrives or on timeout heartbeat
  static uint32_t lastDrawTime = 0;
  bool timeout = (millis() - lastDataTime > 2500);

  if (packetReady || (timeout && (millis() - lastDrawTime > 500))) {
    drawScreen(g_opacity, g_k_val, g_v_sens, g_temp_c, g_err_code, g_state_str, timeout);
    lastDrawTime = millis();
  }
}

// ============================================================================
// PARSE TELEMETRY STRING
// [FAT_SMOKE] OP: 0.0% | k: 0.00 m^-1 | V_sens: 4.34V | Tube: 25.0C | ERR: 0x00 | ST: MEAS
// ============================================================================
void parseTelemetry(const char* line) {
  char* op_ptr = strstr(line, "OP: ");
  if (op_ptr) g_opacity = atof(op_ptr + 4);

  char* k_ptr = strstr(line, "k: ");
  if (k_ptr) g_k_val = atof(k_ptr + 3);

  char* v_ptr = strstr(line, "V_sens: ");
  if (v_ptr) g_v_sens = atof(v_ptr + 8);

  char* t_ptr = strstr(line, "Tube: ");
  if (t_ptr) g_temp_c = atof(t_ptr + 6);

  char* err_ptr = strstr(line, "ERR: 0x");
  if (err_ptr) {
    g_err_code = (uint8_t)strtol(err_ptr + 7, NULL, 16);
  }

  char* st_ptr = strstr(line, "ST: ");
  if (st_ptr) {
    strncpy(g_state_str, st_ptr + 4, 5);
    g_state_str[5] = '\0';
  }
}

// ============================================================================
// DRAW DASHBOARD USING U8G2 PAGE BUFFER
// ============================================================================
void drawScreen(float opacity, float k, float v_sens, float temp, uint8_t err, const char* state, bool signalLoss) {
  char str_buf[16];

  u8g2.firstPage();
  do {
    // ------------------------------------------------------------------------
    // CASE 1: SYSTEM ERROR POPUP BANNER (Active only when PIC state is ERR)
    // ------------------------------------------------------------------------
    if (strncmp(state, "ERR", 3) == 0 && !signalLoss) {
      u8g2.drawFrame(0, 0, 128, 64);
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.drawStr(12, 12, "! SYSTEM FAULT !");
      u8g2.drawHLine(4, 15, 120);

      u8g2.setFont(u8g2_font_6x10_tf);
      if (err & (1 << 4)) {
        u8g2.drawStr(4, 28, "ERR 4: OPTIC BLOCKED");
        u8g2.drawStr(4, 40, "Check glass & soot");
      } else if (err & (1 << 3)) {
        u8g2.drawStr(4, 28, "ERR 3: LIGHT LEAK");
        u8g2.drawStr(4, 40, "Check chamber seal");
      } else if (err & (1 << 2)) {
        u8g2.drawStr(4, 28, "ERR 2: SENSOR OPEN");
        u8g2.drawStr(4, 40, "Check OPT101 wiring");
      } else if (err & (1 << 5)) {
        u8g2.drawStr(4, 28, "ERR 5: SATURATED");
        u8g2.drawStr(4, 40, "Sensor > 4.85V");
      } else if (err & (1 << 1)) {
        u8g2.drawStr(4, 28, "ERR 1: TEMP HIGH");
        u8g2.drawStr(4, 40, "Chamber > 120 C");
      } else if (err & (1 << 0)) {
        u8g2.drawStr(4, 28, "ERR 0: TEMP LOW");
        u8g2.drawStr(4, 40, "Chamber < 90 C");
      }
      u8g2.setFont(u8g2_font_5x8_tf);
      u8g2.drawStr(10, 57, "Press button to retry");
      continue;
    }

    // ------------------------------------------------------------------------
    // CASE 2: 50% ND FILTER CALIBRATION PROMPT
    // ------------------------------------------------------------------------
    if (strncmp(state, "CAL50", 5) == 0) {
      u8g2.drawFrame(0, 0, 128, 64);
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.drawStr(14, 12, "CALIBRATION 50%");
      u8g2.drawHLine(4, 15, 120);

      u8g2.setFont(u8g2_font_7x14B_tf);
      u8g2.drawStr(8, 32, "INSERT 50% FILTER");

      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.drawStr(8, 46, "V_sens: ");
      dtostrf(v_sens, 3, 2, str_buf);
      u8g2.drawStr(50, 46, str_buf);
      u8g2.drawStr(80, 46, "V");

      u8g2.setFont(u8g2_font_5x8_tf);
      u8g2.drawStr(8, 58, "Press button to lock");
      continue;
    }

    // ------------------------------------------------------------------------
    // CASE 3: NORMAL MEASUREMENT DASHBOARD
    // ------------------------------------------------------------------------
    // 1. Header
    u8g2.setFont(u8g2_font_6x10_tf);
    if (err > 0) {
      u8g2.drawStr(2, 9, "FS1.0 [WARN]");
    } else {
      u8g2.drawStr(2, 9, "FAT SMOKE 1.0");
    }

    if (signalLoss) {
      u8g2.drawStr(80, 9, "NO PIC");
    } else {
      dtostrf(temp, 3, 1, str_buf);
      u8g2.drawStr(82, 9, str_buf);
      u8g2.drawStr(112, 9, "C");
    }
    u8g2.drawHLine(0, 11, 128);

    // 2. Large Opacity Reading
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(2, 23, "OPACITY:");

    u8g2.setFont(u8g2_font_logisoso16_tf); // High-contrast clean large font
    dtostrf(opacity, 4, 1, str_buf);
    u8g2.drawStr(34, 38, str_buf);
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(92, 38, "%");

    // 3. Sub-data Row (k value & Sensor Voltage)
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(2, 49, "k:");
    dtostrf(k, 3, 2, str_buf);
    u8g2.drawStr(14, 49, str_buf);
    u8g2.drawStr(44, 49, "m-1");

    u8g2.drawStr(74, 49, "V:");
    dtostrf(v_sens, 3, 2, str_buf);
    u8g2.drawStr(86, 49, str_buf);
    u8g2.drawStr(114, 49, "V");

    // 4. Analog Bar Gauge (0 to 100%)
    u8g2.drawFrame(2, 53, 124, 9);
    int barWidth = (int)((opacity / 100.0f) * 120.0f);
    if (barWidth < 0) barWidth = 0;
    if (barWidth > 120) barWidth = 120;
    if (barWidth > 0) {
      u8g2.drawBox(4, 55, barWidth, 5);
    }
    // 50% legal limit marker notch
    u8g2.drawVLine(64, 52, 2);

  } while (u8g2.nextPage());
}
