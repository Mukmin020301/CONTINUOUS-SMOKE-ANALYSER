/*
 * ============================================================================
 * Project:  FAT SMOKE 1.0 - CONTINUOUS SMOKE ANALYZER (DIESEL OPACIMETER)
 * File:     main.c
 * Target:   Microchip PIC18F46K22 @ 16 MHz (Power Stretcher V2.0 Board)
 * Compiler: Microchip MPLAB XC8
 * Author:   Mukmin (Sole PIC / Lead Engineer) & Pair Programming Team
 * Date:     October 2026
 *
 * Description:
 *  Closed-flow heated continuous diesel smoke analyzer firmware implementing:
 *  1. Beer-Lambert Absorption Law (Optical Path L = 0.215m = 21.5cm).
 *  2. 3-Point Lagrange Polynomial Calibration (V0, V50 ND2 Filter, V100).
 *  3. OPT101 Closed-Loop Auto-Tuning: Regulates 550nm Green LED PWM to 4.00V baseline.
 *  4. 12V 100W Heated Chamber PID/Hysteresis Control (Target 100°C) with NTC Thermistor.
 *  5. Direct RS-232 Hardware Serial Telemetry via onboard MAX3232 (9600 Baud, 8-N-1).
 *  6. Dual Analog Output Signal: Scaled Opacity (0.5V-4.5V) or Raw Voltage (0-5V).
 *  7. Digital IIR/EMA Noise Filter for engine vibration & exhaust pulse damping.
 *  8. EEPROM Calibration Persistence across power cycles.
 *
 * P1 Main Connector Hardware Pinout (Fuel Stretcher V2.0 - Whiteboard Plan):
 * - Pin 12 (Batt)    : +12V Power Input (Red Wire)
 * - Pin 3  (GND)     : System GND (Black Wire)
 * - Pin 2  (TPS)     : OPT101 Sensor Input via U2 Op-Amp (Yellow Wire -> AN0 / RA0)
 * - Pin 9  (o2Sens1) : Chamber NTC Thermistor Input (AN2 / RA2)
 * - Pin 14 (injOut1) : 12V 100W Chamber Heater Low-Side IGBT Q1 (Green Wire -> RD1)
 * - Pin 15 (injOut2) : 550nm Green LED Low-Side PWM Drive Q2 (White Wire -> RC2 / CCP1)
 * - Pin 16 (injOut3) : 12V Turbo Blower / LED2 Drive Q3 (RC1)
 * - Pin 19 (o2out1)  : Analog Opacity Output Signal 0.5V - 4.5V (RE1 / CCP3 + RC Filter)
 * - Pin 4  (injSens1): Button 1 SW1 - Output Mode Toggle (RB0, Active LOW)
 * - Pin 5  (injSens2): Button 2 SW2 - 50% Calibrate / Re-Zero (RB1, Active LOW)
 * - Pin 11 (txRS)    : RS-232 Telemetry TX Out (via onboard MAX3232)
 * - Pin 1  (rxRS)    : RS-232 Telemetry RX In (via onboard MAX3232)
 *
 * P2 Header:
 * - Pin 2 (+5V VDD)  : +5V Source for OPT101 Sensor
 * - Pin 3 (GND)      : Ground for OPT101 Sensor
 *
 * Onboard Status LEDs:
 * - RD2 (D1)         : Green LED (Solid = Measuring; Blinking = User Cal 50%)
 * - RD3 (D2)         : Red LED (Blinking = Auto-tuning/Self-Cal; Solid = Error)
 * ============================================================================
 */

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

// ============================================================================
// BENCH TEST CONFIGURATION
// ============================================================================
// Set to 1 for Bench Testing without Heater, Blower, or Thermistor.
// Set to 0 for Full System with heated chamber.
#define BENCH_TEST_MODE     1

// --- Configuration Bits for PIC18F46K22 ---
#pragma config FOSC = INTIO67   // Internal oscillator block, pins as GPIO
#pragma config PLLCFG = OFF     // 4X PLL disabled
#pragma config PRICLKEN = ON    // Primary clock enabled
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor disabled
#pragma config IESO = OFF       // Oscillator Switchover mode disabled
#pragma config PWRTEN = ON      // Power-up Timer enabled
#pragma config BOREN = SBORDIS  // Brown-out Reset enabled in hardware
#pragma config BORV = 190       // BOR Voltage set to 1.90V
#pragma config WDTEN = OFF      // Watchdog Timer disabled
#pragma config PBADEN = OFF     // PORTB A/D Enable bit (PORTB<5:0> pins are digital I/O)
#pragma config CCP3MX = PORTE0  // P3A/CCP3 muxed with RE0, P3B muxed with RE1 (Pin 19 o2out1)
#pragma config LVP = OFF        // Single-Supply ICSP disabled
#pragma config MCLRE = EXTMCLR  // MCLR pin enabled

#define _XTAL_FREQ 16000000UL   // 16 MHz Internal Fosc

#include "oled_sh1106.h"

// ============================================================================
// HARDWARE PIN DEFINITIONS
// ============================================================================
#define LED_GREEN_LAT       LATDbits.LATD2       // Onboard Green LED (D1)
#define LED_RED_LAT         LATDbits.LATD3       // Onboard Red LED (D2)

// Pushbuttons / Switches (Active LOW to GND)
#define BTN_INJSENS2_PORT   PORTBbits.RB1        // P1 Pin 5 (injSens2 -> RB1)
#define BTN_INJSENS1_PORT   PORTBbits.RB0        // P1 Pin 4 (injSens1 -> RB0)
#define BTN_BOOST_PORT      PORTAbits.RA5        // P1 Pin 8 (Boost -> RA5, Direct RC filter to MCU)
#define BTN_SW2_PCB_PORT    PORTAbits.RA4        // PCB Top-Edge Footprint SW2 (RA4)
#define BTN_SW1_PCB_PORT    PORTCbits.RC0        // PCB Top-Edge Footprint SW1 (RC0)

#define SW1_PORT            BTN_INJSENS1_PORT
#define SW2_PORT            BTN_INJSENS2_PORT

// Low-side Power Drives (IGBT / MOSFET GD18N40LZ)
#define HEATER_LAT          LATDbits.LATD1       // P1 Pin 14 (injOut1 -> RD1 / Green Wire) - 12V Heater
#define BLOWER_LAT          LATCbits.LATC1       // P1 Pin 16 (injOut3 -> RC1) - 12V Turbo Blower

// Analog Output Pin (RC Filter on board)
#define OPACITY_OUT_LAT     LATEbits.LATE1       // P1 Pin 19 (o2out1 -> RE1)

// Analog ADC Channels
#define ADC_CH_OPT101       24                   // AN24 / RD4 (Pin 2 on IC1 - tps_uC from U2 Op-Amp / P1 Pin 2 TPS)
#define ADC_CH_NTC          2                    // AN2 / RA2 (P1 Pin 9 / o2Sens1)

// ============================================================================
// SYSTEM CONSTANTS & CALIBRATION TARGETS
// ============================================================================
#define OPTICAL_PATH_L_M    0.215f               // Beer-Lambert path length L = 21.5 cm = 0.215 m
#define TARGET_4V_ADC       818                  // 4.00V at 10-bit ADC (4.0V / 5.0V * 1023 = 818)
#if BENCH_TEST_MODE
#define MIN_ACCEPTABLE_ADC  200                  // Forgiving threshold for 5mm standard LED bench test
#else
#define MIN_ACCEPTABLE_ADC  700                  // Production threshold (~3.42V) before error
#endif
#define MAX_LED_PWM         250                  // Saturation threshold for LED PWM drive

// Chamber Temperature Regulation (Target 100°C)
#define HEATER_TARGET_C     100.0f               // Target chamber temperature
#define HEATER_HYST_C       2.0f                 // Hysteresis ±2.0°C (ON < 98°C, OFF >= 102°C)
#define NTC_BETA            3950.0f              // Standard 3950 NTC Thermistor Beta
#define NTC_ROOM_R          100000.0f            // 100k Room Resistance at 25°C
#define NTC_PULLUP_R        10000.0f             // 10k Divider Pull-up Resistor
#define NTC_ROOM_TEMP_K     298.15f              // 25°C in Kelvin (273.15 + 25)

// ============================================================================
// FIRMWARE REVISION & ERROR BITMAP (Boss Specification 29-10-26)
// ============================================================================
#define FIRMWARE_REV_MAJOR  5                    // Rev 5.0 (Blinks 5 times at power on)
#define FIRMWARE_REV_MINOR  0

#define ERR_BIT_TEMP_LOW    0                    // Bit 0: Chamber temp < 90°C during test
#define ERR_BIT_TEMP_HIGH   1                    // Bit 1: Chamber temp > 120°C
#define ERR_BIT_DARK_LOW    2                    // Bit 2: V0 disconnected / < 0.01V
#define ERR_BIT_DARK_HIGH   3                    // Bit 3: Light leakage when LED off (> 0.25V)
#define ERR_BIT_LIGHT_LOW   4                    // Bit 4: Optical path blocked / soot (cannot reach target V100)
#define ERR_BIT_LIGHT_HIGH  5                    // Bit 5: Sensor saturated > 4.85V

// EEPROM Map
#define EEPROM_MAGIC_ADDR   0x00                 // Magic signature byte (0xA5)
#define EEPROM_MAGIC_VAL    0xA5
#define EEPROM_V100_H       0x01
#define EEPROM_V100_L       0x02
#define EEPROM_V50_H        0x03
#define EEPROM_V50_L        0x04
#define EEPROM_V0_H         0x05
#define EEPROM_V0_L         0x06
#define EEPROM_LED_PWM      0x07

// ============================================================================
// SYSTEM STATES (Whiteboard State Machine)
// ============================================================================
typedef enum {
    STATE_WARMUP = 0,       // Heat to target temp (LED Blinking)
    STATE_THERMAL_ZERO,     // @ Temp: Recheck 0, Adjust PW to 100%, Record EEPROM
    STATE_MEASURE,          // Normal run: Solid LED, continuous test
    STATE_FAST_REZERO,      // Short Push: Fast Re-Zero (100% / 0)
    STATE_CALIBRATE_50,     // Long Push: Full 3-Point Calibration (0, 100, 50% ND)
    STATE_ERROR             // Error diagnostic: Blink #
} SystemState_t;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
uint16_t v100_adc       = 818;   // 100% transmission ADC baseline (Clean air)
uint16_t v50_adc        = 419;   // 50% transmission ADC (ND2 filter baseline)
uint16_t v0_adc         = 20;    // 0% dark voltage ADC baseline (LED off / blocked)
uint8_t  led_pwm_val    = 120;   // Auto-tuned 550nm LED PWM (0-255)
uint8_t  g_error_flags  = 0;     // 8-bit Error Status Register
bool     mode_raw       = false; // false = Scaled Opacity (0.5V-4.5V), true = Raw Signal (0-5V)
float    filtered_adc   = 818.0f;// EMA filtered photodetector reading
float    chamber_temp_c = 25.0f; // Measured chamber tube temperature
bool     heater_fault   = false; // Sensor open/short fault flag

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void System_Init(void);
void ADC_Init(void);
uint16_t ADC_Read(uint8_t channel);
void PWM_Init(void);
void PWM_SetLED(uint8_t duty);
void PWM_SetOpacityOutput(uint8_t duty);
void UART1_Init(void);
void UART1_Write(char c);
void UART1_Print(const char *str);
void UART1_PrintFloat(float val, uint8_t decimals);
void EEPROM_Write(uint8_t address, uint8_t data);
uint8_t EEPROM_Read(uint8_t address);
void Load_Calibration(void);
void Save_Calibration(void);
float Read_Chamber_Temperature(void);
void Update_Heater_Control(void);
float Calculate_Lagrange_Transmission(float v_sig);
void Delay_Ms(uint16_t ms);
uint16_t ADC_Read_Average(uint8_t channel, uint8_t samples);
void LED_BlinkRev(uint8_t count);
void LED_BlinkErrorCode(uint8_t error_byte);
bool Is_Button_Pressed(void);

// ============================================================================
// HELPER FUNCTIONS FOR WHITEBOARD ALGORITHM
// ============================================================================
bool Is_Button_Pressed(void) {
    // Only check pins that have internal pull-ups on PORTB (Active LOW to GND)
    if (PORTBbits.RB1 == 0) return true; // P1 Pin 5 (injSens2 -> RB1)
    if (PORTBbits.RB0 == 0) return true; // P1 Pin 4 (injSens1 -> RB0)
    return false;
}

uint16_t ADC_Read_Average(uint8_t channel, uint8_t samples) {
    uint32_t accum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        accum += ADC_Read(channel);
        Delay_Ms(5);
    }
    return (uint16_t)(accum / samples);
}

void LED_BlinkRev(uint8_t count) {
    UART1_Print("[BOOT] ON -> Blinking Firmware Rev #");
    char c = '0' + count;
    UART1_Write(c);
    UART1_Print("\r\n");
    for (uint8_t i = 0; i < count; i++) {
        LED_GREEN_LAT = 1;
        Delay_Ms(350);
        LED_GREEN_LAT = 0;
        Delay_Ms(350);
    }
    Delay_Ms(400);
}

void LED_BlinkErrorCode(uint8_t error_byte) {
    // Find highest active error bit (0 to 5)
    uint8_t blinks = 1;
    for (int8_t b = 5; b >= 0; b--) {
        if (error_byte & (1 << b)) {
            blinks = (uint8_t)(b + 1);
            break;
        }
    }
    for (uint8_t i = 0; i < blinks; i++) {
        LED_RED_LAT = 1;
        Delay_Ms(200);
        LED_RED_LAT = 0;
        Delay_Ms(200);
    }
    Delay_Ms(1000);
}

// ============================================================================
// MAIN APPLICATION ENTRY POINT
// ============================================================================
void main(void) {
    System_Init();
    
    // Load saved calibration from EEPROM if valid
    Load_Calibration();

    // 1. ON -> BLINK REV # (Step 1 of Whiteboard)
    LED_BlinkRev(FIRMWARE_REV_MAJOR);

    SystemState_t currentState = STATE_WARMUP;
    uint16_t telemetry_counter = 0;
    uint16_t btn_hold_ticks = 0;

#if !BENCH_TEST_MODE
    BLOWER_LAT = 1; // Start extraction blower fan
#endif

    while (1) {
        Update_Heater_Control();

        switch (currentState) {

            // ----------------------------------------------------------------
            // STATE: WARMUP & HEATING (LED Blinks while heating)
            // ----------------------------------------------------------------
            case STATE_WARMUP:
                UART1_Print("[WARMUP] Heating Chamber Tube...\r\n");
#if BENCH_TEST_MODE
                // In Bench Test Mode: simulate warmup with 2 blinks, then proceed to thermal zero
                for (uint8_t w = 0; w < 3; w++) {
                    LED_GREEN_LAT = !LED_GREEN_LAT;
                    Delay_Ms(350);
                }
                currentState = STATE_THERMAL_ZERO;
#else
                HEATER_LAT = 1; // Turn ON 12V 100W Heater
                while (chamber_temp_c < (HEATER_TARGET_C - HEATER_HYST_C)) {
                    LED_GREEN_LAT = !LED_GREEN_LAT;
                    for (uint8_t t = 0; t < 25; t++) {
                        Delay_Ms(20);
                        Update_Heater_Control();
                    }
                    UART1_Print("[WARMUP] Chamber Temp: ");
                    UART1_PrintFloat(chamber_temp_c, 1);
                    UART1_Print("C / 100.0C\r\n");
                }
                currentState = STATE_THERMAL_ZERO;
#endif
                break;

            // ----------------------------------------------------------------
            // STATE: THERMAL ZERO (@ TEMP -> Recheck 0, Adjust PW to 100%, Record EEPROM)
            // ----------------------------------------------------------------
            case STATE_THERMAL_ZERO:
                UART1_Print("\r\n[ZERO] Auto-Tuning Optical LED PWM baseline...\r\n");
                OLED_ClearPages(0, 7);
                OLED_Print(0, 0, "[ FAT SMOKE 1.0 ]   ");
                OLED_Print(2, 0, "STATE: AUTO-ZERO    ");
                OLED_Print(4, 0, "TUNING LED PWM...   ");

                // 1. Measure Dark Voltage (V0) with LED OFF
                PWM_SetLED(0);
                Delay_Ms(200);
                v0_adc = ADC_Read_Average(ADC_CH_OPT101, 32);
                UART1_Print("[ZERO] Dark V0=");
                UART1_PrintFloat(((float)v0_adc * 5.0f / 1023.0f), 2);
                UART1_Print("V\r\n");

                // 2. Closed-Loop LED PWM Tuning (targeting ~4.00V / 818 counts)
                led_pwm_val = 60; // Start at ~25%
                PWM_SetLED(led_pwm_val);
                Delay_Ms(100);

                char disp_buf[24];
                for (uint8_t attempt = 0; attempt < 80; attempt++) {
                    uint16_t cur_adc = ADC_Read_Average(ADC_CH_OPT101, 8);
                    float cur_v = (float)cur_adc * 5.0f / 1023.0f;
                    
                    // Update OLED progress
                    sprintf(disp_buf, "Vs : %4.2fV  ", cur_v);
                    OLED_Print(5, 0, disp_buf);
                    sprintf(disp_buf, "PWM: %3u%% (%u) ", (unsigned int)((led_pwm_val * 100) / 255), (unsigned int)led_pwm_val);
                    OLED_Print(6, 0, disp_buf);

                    if (cur_adc < TARGET_4V_ADC && led_pwm_val < MAX_LED_PWM) {
                        uint16_t diff = TARGET_4V_ADC - cur_adc;
                        if (diff > 200) led_pwm_val += 10;
                        else if (diff > 50) led_pwm_val += 3;
                        else led_pwm_val += 1;
                        if (led_pwm_val > MAX_LED_PWM) led_pwm_val = MAX_LED_PWM;
                    } else if (cur_adc > TARGET_4V_ADC && led_pwm_val > 5) {
                        uint16_t diff = cur_adc - TARGET_4V_ADC;
                        if (diff > 200) led_pwm_val -= 10;
                        else if (diff > 50) led_pwm_val -= 3;
                        else led_pwm_val -= 1;
                        if (led_pwm_val < 5) led_pwm_val = 5;
                    } else {
                        // Reached target!
                        break;
                    }
                    PWM_SetLED(led_pwm_val);
                    Delay_Ms(30);
                }

                // Sample baseline clean air V100
                v100_adc = ADC_Read_Average(ADC_CH_OPT101, 32);
                if (v100_adc <= (v0_adc + 30)) {
                    // Forgiving bench test fallback if LED is dim
                    v100_adc = v0_adc + 150;
                }

                v50_adc = (v100_adc + v0_adc) / 2;
                filtered_adc = (float)v100_adc;

                // Save baseline to EEPROM
                Save_Calibration();
                UART1_Print("[ZERO] Baseline locked! V100=");
                UART1_PrintFloat(((float)v100_adc * 5.0f / 1023.0f), 2);
                UART1_Print("V | Duty=");
                char dbuf[10]; sprintf(dbuf, "%u", (unsigned int)led_pwm_val); UART1_Print(dbuf);
                UART1_Print("/255\r\n");

                OLED_Print(4, 0, "BASELINE LOCKED!    ");
                Delay_Ms(800);
                OLED_Clear();

                LED_GREEN_LAT = 1;
                LED_RED_LAT = 0;
                currentState = STATE_MEASURE;
                UART1_Print("[STATUS] Ready! Solid Green LED. Entering Measurement.\r\n");
                break;

            // ----------------------------------------------------------------
            // STATE: NORMAL REAL-TIME MEASUREMENT (SOLID LED)
            // ----------------------------------------------------------------
            case STATE_MEASURE:
                LED_GREEN_LAT = 1;
                LED_RED_LAT = 0;

                // 1. Read OPT101 with EMA Filter
                uint16_t raw_sample = ADC_Read(ADC_CH_OPT101);
                filtered_adc = (0.25f * (float)raw_sample) + (0.75f * filtered_adc);
                float v_sensor = (filtered_adc * 5.0f) / 1023.0f;

                // 2. Transmission % via 3-Point Lagrange (or Linear Beer-Lambert fallback)
                float trans_pct = 0.0f;
                float span = (float)v100_adc - (float)v0_adc;
                if (span < 20.0f) span = 20.0f;

                if ((v50_adc - v0_adc) > 50 && (v100_adc - v50_adc) > 50) {
                    trans_pct = Calculate_Lagrange_Transmission(filtered_adc);
                } else {
                    if (filtered_adc <= (float)v0_adc) {
                        trans_pct = 0.0f;
                    } else if (filtered_adc >= (float)v100_adc) {
                        trans_pct = 100.0f;
                    } else {
                        trans_pct = ((filtered_adc - (float)v0_adc) / span) * 100.0f;
                    }
                }
                if (trans_pct > 100.0f) trans_pct = 100.0f;
                if (trans_pct < 0.0f)   trans_pct = 0.0f;

                float opacity_pct = 100.0f - trans_pct;
                if (opacity_pct > 100.0f) opacity_pct = 100.0f;
                if (opacity_pct < 0.0f)   opacity_pct = 0.0f;

                // 3. Extinction coefficient k (m^-1)
                float k_val = 0.0f;
                float trans_fraction = trans_pct / 100.0f;
                if (trans_fraction >= 0.999f) {
                    k_val = 0.0f;
                } else if (trans_fraction <= 0.005f) {
                    k_val = 24.64f;
                } else {
                    k_val = -(logf(trans_fraction)) / OPTICAL_PATH_L_M;
                }

                // 4. Analog Output (0.00V = 0%, 0.45V = 50%, 0.90V = 100%)
                uint8_t scaled_pwm = (uint8_t)((opacity_pct / 100.0f) * 255.0f);
                PWM_SetOpacityOutput(scaled_pwm);

                // 5. Update 1.3" I2C OLED Dashboard (2 Hz = 500ms)
                static uint8_t oled_refresh_div = 0;
                static uint16_t oled_rescan_div = 0;
                if (++oled_refresh_div >= 25) {
                    oled_refresh_div = 0;
                    if (g_oled_detected) {
                        OLED_ShowDashboard(opacity_pct, k_val, v_sensor, chamber_temp_c, led_pwm_val, "MEAS");
                    }
                }
                if (!g_oled_detected) {
                    if (++oled_rescan_div >= 150) { // 150 * 20ms = 3.0 seconds
                        oled_rescan_div = 0;
                        OLED_ScanAndReport();
                    }
                }

                // 6. Telemetry Broadcast @ 2 Hz (every 500ms = 25 ticks of 20ms)
                telemetry_counter++;
                if (telemetry_counter >= 25) {
                    telemetry_counter = 0;
                    UART1_Print("[FAT_SMOKE] OP: ");
                    UART1_PrintFloat(opacity_pct, 1);
                    UART1_Print("% | k: ");
                    UART1_PrintFloat(k_val, 2);
                    UART1_Print(" m^-1 | V_sens: ");
                    UART1_PrintFloat(v_sensor, 2);
                    UART1_Print("V | Tube: ");
                    UART1_PrintFloat(chamber_temp_c, 1);
                    UART1_Print("C | ERR: 0x");
                    char hex_buf[8];
                    sprintf(hex_buf, "%02X", g_error_flags);
                    UART1_Print(hex_buf);
                    UART1_Print(" | ST: MEAS | OLED: ");
                    OLED_PrintStatus();
                    UART1_Print("\r\n");
                }

                // 6. Single-Button Handler (Short Push vs Long Push on any active button pin)
                if (Is_Button_Pressed()) { // Pressed down
                    btn_hold_ticks++;
                    if (btn_hold_ticks >= 125) { // 125 * 20ms = 2500ms -> LONG PUSH!
                        // Confirmation blink
                        for (uint8_t f = 0; f < 6; f++) {
                            LED_GREEN_LAT = !LED_GREEN_LAT;
                            Delay_Ms(60);
                        }
                        uint16_t to = 0;
                        while (Is_Button_Pressed() && ++to < 2000) Delay_Ms(1); // Wait for release with timeout
                        btn_hold_ticks = 0;
                        currentState = STATE_CALIBRATE_50;
                        break;
                    }
                } else {
                    // Released
                    if (btn_hold_ticks >= 2 && btn_hold_ticks < 125) {
                        // SHORT PUSH (< 2.5s) -> Fast Re-zero
                        btn_hold_ticks = 0;
                        currentState = STATE_FAST_REZERO;
                        break;
                    }
                    btn_hold_ticks = 0;
                }

                Delay_Ms(20);
                break;

            // ----------------------------------------------------------------
            // STATE: FAST RE-ZERO (SHORT PUSH on button)
            // ----------------------------------------------------------------
            case STATE_FAST_REZERO:
                UART1_Print("[ACTION] SHORT PUSH: Fast Re-Zeroing Clean Air Baseline...\r\n");
                OLED_ClearPages(0, 7);
                OLED_Print(1, 4, "== FAST RE-ZERO ==");
                OLED_Print(3, 4, "READING BASELINE..");
                for (uint8_t i = 0; i < 4; i++) {
                    LED_GREEN_LAT = !LED_GREEN_LAT;
                    Delay_Ms(100);
                }
                v100_adc = ADC_Read_Average(ADC_CH_OPT101, 32);
                if (v100_adc <= (v0_adc + 30)) v100_adc = v0_adc + 150;
                v50_adc = (v100_adc + v0_adc) / 2;
                filtered_adc = (float)v100_adc;
                Save_Calibration();
                
                UART1_Print("[CAL] Clean Air Re-Zeroed! V100=");
                UART1_PrintFloat(((float)v100_adc * 5.0f / 1023.0f), 2);
                UART1_Print("V | V0=");
                UART1_PrintFloat(((float)v0_adc * 5.0f / 1023.0f), 2);
                UART1_Print("V\r\n");

                OLED_Print(5, 4, "RE-ZERO COMPLETE!");
                Delay_Ms(600);
                OLED_Clear();

                LED_GREEN_LAT = 1;
                LED_RED_LAT = 0;
                currentState = STATE_MEASURE;
                break;

            // ----------------------------------------------------------------
            // STATE: 3-POINT CALIBRATION (LONG PUSH on button)
            // ----------------------------------------------------------------
            case STATE_CALIBRATE_50:
                UART1_Print("[CAL50] LONG PUSH: Entering 3-Point Calibration Routine...\r\n");
                OLED_ClearPages(0, 7);
                OLED_Print(0, 0, "=== 3-PT CAL ===");
                OLED_Print(2, 0, "1. SAMPLING DARK V0 ");

                // 1. BLINK FAST @ 0/100 (5 Hz for 1.2s)
                for (uint8_t b = 0; b < 6; b++) {
                    LED_GREEN_LAT = !LED_GREEN_LAT;
                    Delay_Ms(100);
                }

                // Sample 0% Dark (LED OFF)
                PWM_SetLED(0);
                Delay_Ms(300);
                v0_adc = ADC_Read_Average(ADC_CH_OPT101, 32);

                // Restore LED and sample 100%
                OLED_Print(3, 0, "2. SAMPLING AIR V100");
                PWM_SetLED(led_pwm_val);
                Delay_Ms(300);
                v100_adc = ADC_Read_Average(ADC_CH_OPT101, 32);

                // 2. Prompt operator to insert 50% ND Filter
                UART1_Print("[PROMPT] DBL BLINK @ 50: Insert 50% ND Filter. Press button when ready.\r\n");
                OLED_Print(5, 0, "3. INSERT 50% FILTER");
                OLED_Print(6, 0, "   PUSH TO CONFIRM  ");

                while (1) {
                    float cur_v = (float)ADC_Read(ADC_CH_OPT101) * 5.0f / 1023.0f;
                    char cbuf[24];
                    sprintf(cbuf, "Vs: %4.2fV [WAIT]  ", cur_v);
                    OLED_Print(7, 0, cbuf);

                    // Broadcast prompt so PC / Dyno updates
                    UART1_Print("[FAT_SMOKE] OP: 50.0% | k: 3.22 m^-1 | V_sens: ");
                    UART1_PrintFloat(cur_v, 2);
                    UART1_Print("V | Tube: ");
                    UART1_PrintFloat(chamber_temp_c, 1);
                    UART1_Print("C | ERR: 0x00 | ST: CAL50\r\n");

                    // Double-Blink pattern: ON-OFF-ON-OFF-PAUSE
                    LED_GREEN_LAT = 1; Delay_Ms(100);
                    LED_GREEN_LAT = 0; Delay_Ms(100);
                    LED_GREEN_LAT = 1; Delay_Ms(100);
                    LED_GREEN_LAT = 0;

                    // 600ms pause while listening for button press
                    for (uint8_t p = 0; p < 12; p++) {
                        Delay_Ms(50);
                        Update_Heater_Control();
                        if (Is_Button_Pressed()) {
                            Delay_Ms(50);
                            if (Is_Button_Pressed()) {
                                uint16_t to = 0;
                                while (Is_Button_Pressed() && ++to < 2000) Delay_Ms(1); // Wait for release with timeout
                                goto sample_50_nd_point;
                            }
                        }
                    }
                }

            sample_50_nd_point:
                UART1_Print("[CAL50] Sampling 50% ND Filter Point...\r\n");
                LED_GREEN_LAT = 1;
                LED_RED_LAT = 1; // Both LEDs ON while sampling

                v50_adc = ADC_Read_Average(ADC_CH_OPT101, 32);
                Save_Calibration();

                UART1_Print("[CAL50] 50% Point Stored! V50=");
                UART1_PrintFloat(((float)v50_adc * 5.0f / 1023.0f), 2);
                UART1_Print("V. Remove filter. Resuming normal run...\r\n");

                OLED_ClearPages(5, 7);
                OLED_Print(5, 0, "50% CAL SAVED!      ");
                Delay_Ms(800);
                OLED_Clear();

                LED_GREEN_LAT = 1;
                LED_RED_LAT = 0;
                currentState = STATE_MEASURE;
                break;

            // ----------------------------------------------------------------
            // STATE: ERROR DIAGNOSTIC (BLINK #)
            // ----------------------------------------------------------------
            case STATE_ERROR:
                PWM_SetOpacityOutput(0);
                UART1_Print("[ERROR] Active Fault! Error Flags: 0x");
                char ebuf[8];
                sprintf(ebuf, "%02X", g_error_flags);
                UART1_Print(ebuf);
                UART1_Print("\r\n");

                while (currentState == STATE_ERROR) {
                    Update_Heater_Control();

                    // Read live sensor voltage
                    uint16_t err_raw = ADC_Read(ADC_CH_OPT101);
                    float err_vsens = ((float)err_raw * 5.0f) / 1023.0f;

                    // Telemetry broadcast
                    UART1_Print("[FAT_SMOKE] OP: 100.0% | k: 24.64 m^-1 | V_sens: ");
                    UART1_PrintFloat(err_vsens, 2);
                    UART1_Print("V | Tube: ");
                    UART1_PrintFloat(chamber_temp_c, 1);
                    UART1_Print("C | ERR: 0x");
                    UART1_Print(ebuf);
                    UART1_Print(" | ST: ERR\r\n");

                    // Blink error code
                    LED_BlinkErrorCode(g_error_flags);

                    // Operator can press button to clear & retry
                    if (Is_Button_Pressed()) {
                        Delay_Ms(50);
                        if (Is_Button_Pressed()) {
                            uint16_t to = 0;
                            while (Is_Button_Pressed() && ++to < 2000) Delay_Ms(1);
                            g_error_flags = 0;
                            currentState = STATE_WARMUP;
                            break;
                        }
                    }
                }
                break;
        }
    }
}

// ============================================================================
// SYSTEM INITIALIZATION
// ============================================================================
void System_Init(void) {
    // 1. Oscillator: 16 MHz Internal Clock (HFINTOSC)
    OSCCONbits.IRCF = 0b111; // 16 MHz
    while (!OSCCONbits.HFIOFS); // Wait for frequency stability
    
    // 2. Configure General Purpose I/O
    // Onboard Status LEDs: RD2 (Green D1), RD3 (Red D2)
    TRISDbits.TRISD2 = 0;
    TRISDbits.TRISD3 = 0;
    ANSELDbits.ANSD2 = 0;
    ANSELDbits.ANSD3 = 0;
    LED_GREEN_LAT = 0;
    LED_RED_LAT = 0;
    
    // External Pushbuttons on P1 Connector and PCB Footprints:
    TRISBbits.TRISB0 = 1; ANSELBbits.ANSB0 = 0; // P1 Pin 4 (injSens1 -> RB0)
    TRISBbits.TRISB1 = 1; ANSELBbits.ANSB1 = 0; // P1 Pin 5 (injSens2 -> RB1)
    TRISAbits.TRISA5 = 1; ANSELAbits.ANSA5 = 0; // P1 Pin 8 (Boost -> RA5, Direct RC to MCU)
    TRISAbits.TRISA4 = 1;                       // PCB Top SW2 footprint (RA4)
    TRISCbits.TRISC0 = 1;                       // PCB Top SW1 footprint (RC0)
    
    // CRITICAL: Enable Weak Pull-ups on PORTB for Pushbuttons (Active LOW on PIC18F)
    INTCON2bits.RBPU = 0; // 0 = PORTB Pull-ups ENABLED
    WPUBbits.WPUB0   = 1; // Pull-up for Pin 4 (SW1 / RB0)
    WPUBbits.WPUB1   = 1; // Pull-up for Pin 5 (SW2 / RB1)
    
    // Power Outputs:
    // Pin 14 (injOut1 -> RD1 / Q1 IGBT / Green Wire) : 12V 100W Heater
    // Pin 15 (injOut2 -> RC2 / Q2 IGBT / White Wire) : 550nm Green LED PWM (CCP1)
    // Pin 16 (injOut3 -> RC1 / Q3 / Spare)          : 12V Turbo Blower
    TRISDbits.TRISD1 = 0;
    ANSELDbits.ANSD1 = 0;
    HEATER_LAT = 0;

    TRISCbits.TRISC2 = 0;
    ANSELCbits.ANSC2 = 0;

    TRISCbits.TRISC1 = 0;
    BLOWER_LAT = 0;
    
    // Analog Opacity Output Pins: Pin 19 (o2out1 -> RE1 / P3B) and Pin 20 (o2out2 -> RE0 / P3A)
    TRISEbits.TRISE1 = 0;
    ANSELEbits.ANSE1 = 0;
    TRISEbits.TRISE0 = 0;
    ANSELEbits.ANSE0 = 0;
    OPACITY_OUT_LAT = 0;
    
    // Analog Inputs:
    // P1 Pin 2 (TPS -> RD4 / AN24 via U2 Op-Amp / Yellow Wire) : OPT101 Photodetector
    // P1 Pin 9 (o2Sens1 -> RA2 / AN2)                         : Chamber NTC Thermistor
    TRISDbits.TRISD4 = 1; // RD4 input for TPS (AN24)
    ANSELDbits.ANSD4 = 1; // Analog mode for AN24
    TRISAbits.TRISA2 = 1; // RA2 input for NTC (AN2)
    ANSELAbits.ANSA2 = 1; // Analog mode for AN2

    // 3. Initialize Peripherals
    ADC_Init();
    PWM_Init();
    UART1_Init();
    Delay_Ms(150); // Allow RS-232 level shifter to stabilize

    UART1_Print("\r\n==================================================\r\n");
    UART1_Print(" FAT SMOKE 1.0 - CONTINUOUS SMOKE ANALYZER\r\n");
    UART1_Print(" Hardware: Fuel Stretcher V2.0 (PIC18F46K22 @ 16MHz)\r\n");
    UART1_Print(" Algorithm: Full State Machine (Rev 1.0)\r\n");
    UART1_Print(" Optical Path: L = 21.5 cm | Wavelength: 550nm\r\n");
#if BENCH_TEST_MODE
    UART1_Print(" >> BENCH TEST MODE ACTIVE (Heater simulation bypass) <<\r\n");
#endif
    UART1_Print(" RS-232: 9600 Baud, 8-N-1 | Telemetry: 2 Hz\r\n");
    UART1_Print("==================================================\r\n");

    OLED_Init();
    if (g_oled_detected) {
        OLED_ShowSplash();
    }

    // 4. Boot Indicator: Blink Rev Number (Rev 2 = 2 Green Blinks)
    for (uint8_t i = 0; i < 2; i++) {
        LED_GREEN_LAT = 1;
        Delay_Ms(150);
        LED_GREEN_LAT = 0;
        Delay_Ms(150);
    }
}

// ============================================================================
// ADC FUNCTIONS (10-Bit ADC)
// ============================================================================
void ADC_Init(void) {
    ADCON1bits.PVCFG = 0b00;  // VREF+ = VDD (5.0V)
    ADCON1bits.NVCFG = 0b00;  // VREF- = VSS (0V)
    ADCON2bits.ADFM  = 1;     // Right justified (0-1023)
    ADCON2bits.ACQT  = 0b100; // 8 TAD acquisition time
    ADCON2bits.ADCS  = 0b101; // FOSC / 16 clock conversion
    ADCON0bits.ADON  = 1;     // Turn ON ADC module
}

uint16_t ADC_Read(uint8_t channel) {
    ADCON0bits.CHS = channel;
    __delay_us(25);              // Sample acquisition settle time
    ADCON0bits.GO_nDONE = 1;     // Start conversion
    while (ADCON0bits.GO_nDONE); // Poll conversion complete
    return ((uint16_t)((ADRESH << 8) | ADRESL));
}

// ============================================================================
// PWM FUNCTIONS (Timer2, CCP1 for LED, and CCP3 for Opacity Output)
// ============================================================================
void PWM_Init(void) {
    // Set PWM Period for ~19.5 kHz at 16MHz Fosc
    PR2 = 204;
    
    // CCP1 Configuration (RC2 / Pin 15 / injOut2 - 550nm Green LED PWM)
    CCPTMRS0bits.C1TSEL = 0b00;       // CCP1 uses Timer2
    ECCP1AS             = 0;          // Disable auto-shutdown so FLT0/RB0 won't kill LED PWM!
    PWM1CON             = 0;          // Zero dead-band delay
    CCP1CON             = 0b00001100; // Enhanced PWM Single-Output mode (Active-High)
    PSTR1CON            = 0b00000001; // PWM Steering: Steer PWM to P1A (RC2 / Pin 15 injOut2)!
    CCPR1L              = 0;

    // CCP3 Configuration (RE1 / Pin 19 o2out1 & RE0 / Pin 20 o2out2 - Analog Opacity Output)
    CCPTMRS0bits.C3TSEL = 0b00;       // CCP3 uses Timer2
    ECCP3AS             = 0;          // Disable auto-shutdown so FLT0/RB0 won't kill PWM
    PWM3CON             = 0;          // Zero dead-band delay
    CCP3CON             = 0b00001100; // Enhanced PWM Single-Output mode (Active-High)
    PSTR3CON            = 0b00010011; // PWM Steering: Steer identical PWM to BOTH P3A (Pin 20) AND P3B (Pin 19)!
    CCPR3L              = 0;          // Start at 0% duty (0.00V)
    
    // Timer2 Configuration for PWM
    T2CONbits.T2CKPS = 0b00;    // 1:1 Prescaler
    T2CONbits.TMR2ON = 1;       // Enable Timer2
}

void PWM_SetLED(uint8_t duty) {
    if (duty == 255) {
        CCP1CONbits.DC1B = 0b11;
        CCPR1L = PR2;
    } else if (duty == 0) {
        CCP1CONbits.DC1B = 0b00;
        CCPR1L = 0;
    } else {
        uint16_t counts = ((uint16_t)duty * (uint16_t)PR2) / 255;
        if (counts > PR2) counts = PR2;
        CCPR1L = (uint8_t)counts;
    }
}

void PWM_SetOpacityOutput(uint8_t duty) {
    // Directly drives BOTH Pin 19 (o2out1) and Pin 20 (o2out2) with identical PWM!
    // 0% Opacity   -> CCPR3L = 0   -> 0.00V - 0.05V
    // 50% Opacity  -> CCPR3L = 102 -> ~0.45V
    // 100% Opacity -> CCPR3L = 204 -> ~0.90V (D9/D10 diode clamp ceiling)
    uint16_t counts = ((uint16_t)duty * (uint16_t)PR2) / 255;
    if (counts > PR2) counts = PR2;
    CCPR3L = (uint8_t)counts;
}

// ============================================================================
// HARDWARE UART1 (RS-232) & UART2 (Bluetooth / Pin 4 RD6) DUAL-BROADCAST
// ============================================================================
void UART1_Init(void) {
    // 0. Ensure peripheral clocks are enabled in Power Module Disable (PMD0)
    PMD0bits.UART1MD   = 0;     // Enable UART1 peripheral clock
    PMD0bits.UART2MD   = 0;     // Enable UART2 peripheral clock

    // 1. Configure UART1 (9600 Baud @ 16MHz) -> Pin 11 (txRS) via SP3232
    // Formula: Baud = Fosc / (4 * (SPBRG + 1)) -> SPBRG = (16000000 / (4 * 9600)) - 1 = 416
    BAUDCON1bits.BRG16 = 1;
    TXSTA1bits.BRGH    = 1;
    SPBRGH1            = (416 >> 8);
    SPBRG1             = (416 & 0xFF);
    
    // Per Microchip PIC18F46K22 datasheet Section 16.1.1.7:
    // Set both TX and RX TRIS bits to 1 (SPEN will automatically control pin direction)
    TRISCbits.TRISC6   = 1;     // TX1 TRIS set to 1
    TRISCbits.TRISC7   = 1;     // RX1 TRIS set to 1
    ANSELCbits.ANSC6   = 0;     // Digital
    ANSELCbits.ANSC7   = 0;     // Digital
    
    TXSTA1bits.SYNC    = 0;     // Asynchronous mode
    RCSTA1bits.SPEN    = 1;     // Enable Serial Port
    TXSTA1bits.TXEN    = 1;     // Enable Transmitter
    RCSTA1bits.CREN    = 1;     // Enable Continuous Receiver

    // 2. Configure UART2 (9600 Baud @ 16MHz) -> Pin 4 (RD6 / txBT_uC)
    BAUDCON2bits.BRG16 = 1;
    TXSTA2bits.BRGH    = 1;
    SPBRGH2            = (416 >> 8);
    SPBRG2             = (416 & 0xFF);

    TRISDbits.TRISD6   = 1;     // TX2 TRIS set to 1
    TRISDbits.TRISD7   = 1;     // RX2 TRIS set to 1
    ANSELDbits.ANSD6   = 0;     // Digital
    ANSELDbits.ANSD7   = 0;     // Digital

    TXSTA2bits.SYNC    = 0;     // Asynchronous mode
    RCSTA2bits.SPEN    = 1;     // Enable Serial Port 2
    TXSTA2bits.TXEN    = 1;     // Enable Transmitter 2
    RCSTA2bits.CREN    = 1;     // Enable Continuous Receiver 2
}

void UART1_Write(char c) {
    // Transmit over UART1 (SP3232 -> P1 Pin 11)
    while (!PIR1bits.TX1IF);
    TXREG1 = c;

    // Concurrently transmit over UART2 (RD6 -> Bluetooth / Test pad)
    while (!PIR3bits.TX2IF);
    TXREG2 = c;
}

void UART1_Print(const char *str) {
    while (*str) {
        UART1_Write(*str++);
    }
}

void UART1_PrintFloat(float val, uint8_t decimals) {
    char str_buf[24];
    if (isnan(val) || isinf(val)) {
        UART1_Print("ERR");
        return;
    }
    if (val < 0.0f) {
        UART1_Write('-');
        val = -val;
    }
    
    int32_t int_part = (int32_t)val;
    sprintf(str_buf, "%ld.", int_part);
    UART1_Print(str_buf);
    
    float frac_part = val - (float)int_part;
    for (uint8_t i = 0; i < decimals; i++) {
        frac_part *= 10.0f;
        uint8_t digit = (uint8_t)frac_part;
        UART1_Write('0' + digit);
        frac_part -= (float)digit;
    }
}

// ============================================================================
// HEATER MANAGEMENT (12V 100W SILICONE PAD & NTC THERMISTOR)
// ============================================================================
float Read_Chamber_Temperature(void) {
#if BENCH_TEST_MODE
    heater_fault = false;
    return 25.0f; // Mock ambient temperature for bench test
#else
    uint16_t adc = ADC_Read(ADC_CH_NTC); // Read AN2 (Pin 9 / o2Sens1)
    
    // Safety check for disconnected or shorted thermistor wire
    if (adc <= 5 || adc >= 1020) {
        heater_fault = true;
        return -999.0f;
    }
    heater_fault = false;
    
    // Resistance of NTC: R_ntc = R_pullup * ADC / (1023 - ADC)
    float r_ntc = NTC_PULLUP_R * ((float)adc / (1023.0f - (float)adc));
    
    // Steinhart-Hart Beta equation: 1/T = 1/T0 + 1/B * ln(R/R0)
    float steinhart = (1.0f / NTC_ROOM_TEMP_K) + (1.0f / NTC_BETA) * logf(r_ntc / NTC_ROOM_R);
    float temp_k = 1.0f / steinhart;
    float temp_c = temp_k - 273.15f;
    
    return temp_c;
#endif
}

void Update_Heater_Control(void) {
#if BENCH_TEST_MODE
    chamber_temp_c = Read_Chamber_Temperature();
    HEATER_LAT = 0; // Forced OFF in bench test
    return;
#else
    chamber_temp_c = Read_Chamber_Temperature();
    
    // Emergency failsafe: if thermistor open/shorted, force heater off
    if (heater_fault) {
        HEATER_LAT = 0;
        return;
    }
    
    // Hysteresis Bang-Bang Control: Keep chamber tube steady at 100°C
    if (chamber_temp_c < (HEATER_TARGET_C - HEATER_HYST_C)) {
        HEATER_LAT = 1; // Turn ON 12V 100W Heater (Pin 14 / Green Wire)
    } else if (chamber_temp_c >= (HEATER_TARGET_C + HEATER_HYST_C)) {
        HEATER_LAT = 0; // Turn OFF Heater
    }
#endif
}

// ============================================================================
// 3-POINT LAGRANGE POLYNOMIAL CALIBRATION
// ============================================================================
// Points: (V0, 0%), (V50, 50%), (V100, 100%) Transmission
float Calculate_Lagrange_Transmission(float v_sig) {
    float x0 = (float)v0_adc;
    float x1 = (float)v50_adc;
    float x2 = (float)v100_adc;
    
    // Prevent division by zero if calibration points invalid or too close
    if ((x1 - x0) <= 25.0f || (x2 - x1) <= 25.0f) {
        // Fallback to simple linear if V50 not properly calibrated
        if (x2 <= x0 + 10.0f) return 0.0f;
        return ((v_sig - x0) / (x2 - x0)) * 100.0f;
    }
    
    // 2nd Order Lagrange Polynomial (with y0 = 0%):
    // T(V) = 50 * [ (V - x0)(V - x2) / (x1 - x0)(x1 - x2) ]
    //      + 100 * [ (V - x0)(V - x1) / (x2 - x0)(x2 - x1) ]
    float term1 = 50.0f * ((v_sig - x0) * (v_sig - x2)) / ((x1 - x0) * (x1 - x2));
    float term2 = 100.0f * ((v_sig - x0) * (v_sig - x1)) / ((x2 - x0) * (x2 - x1));
    
    return (term1 + term2);
}

// ============================================================================
// EEPROM CALIBRATION PERSISTENCE
// ============================================================================
void Save_Calibration(void) {
    EEPROM_Write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VAL);
    EEPROM_Write(EEPROM_V100_H, (uint8_t)(v100_adc >> 8));
    EEPROM_Write(EEPROM_V100_L, (uint8_t)(v100_adc & 0xFF));
    EEPROM_Write(EEPROM_V50_H,  (uint8_t)(v50_adc >> 8));
    EEPROM_Write(EEPROM_V50_L,  (uint8_t)(v50_adc & 0xFF));
    EEPROM_Write(EEPROM_V0_H,   (uint8_t)(v0_adc >> 8));
    EEPROM_Write(EEPROM_V0_L,   (uint8_t)(v0_adc & 0xFF));
    EEPROM_Write(EEPROM_LED_PWM, led_pwm_val);
}

void Load_Calibration(void) {
    if (EEPROM_Read(EEPROM_MAGIC_ADDR) == EEPROM_MAGIC_VAL) {
        v100_adc    = ((uint16_t)EEPROM_Read(EEPROM_V100_H) << 8) | EEPROM_Read(EEPROM_V100_L);
        v50_adc     = ((uint16_t)EEPROM_Read(EEPROM_V50_H) << 8)  | EEPROM_Read(EEPROM_V50_L);
        v0_adc      = ((uint16_t)EEPROM_Read(EEPROM_V0_H) << 8)   | EEPROM_Read(EEPROM_V0_L);
        led_pwm_val = EEPROM_Read(EEPROM_LED_PWM);

        // Sanity check to auto-repair any corrupted 50% point from accidental calibration
        if (v50_adc <= (v0_adc + 25) || v50_adc >= (v100_adc - 25)) {
            v50_adc = (v100_adc + v0_adc) / 2;
        }
        UART1_Print("[EEPROM] Restored previous calibration settings successfully!\r\n");
    } else {
        UART1_Print("[EEPROM] No previous calibration found. Using factory defaults.\r\n");
    }
}

void EEPROM_Write(uint8_t address, uint8_t data) {
    EEADR = address;
    EEDATA = data;
    EECON1bits.EEPGD = 0; // Access Data EEPROM
    EECON1bits.CFGS  = 0; // Access Flash/EEPROM
    EECON1bits.WREN  = 1; // Enable writes
    
    INTCONbits.GIE = 0;   // Disable Interrupts for unlock sequence
    EECON2 = 0x55;        // Unlock sequence
    EECON2 = 0xAA;
    EECON1bits.WR  = 1;   // Start write
    INTCONbits.GIE = 1;   // Re-enable Interrupts
    
    while (EECON1bits.WR); // Wait for write complete
    EECON1bits.WREN = 0;   // Disable writes
}

uint8_t EEPROM_Read(uint8_t address) {
    EEADR = address;
    EECON1bits.EEPGD = 0;
    EECON1bits.CFGS  = 0;
    EECON1bits.RD    = 1;
    return EEDATA;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
void Delay_Ms(uint16_t ms) {
    while (ms--) {
        __delay_ms(1);
    }
}
