/*
 * ============================================================================
 * FAT SMOKE 1.0 - OPTICAL LED PWM BENCH TEST FIRMWARE
 * Microcontroller: Microchip PIC18F46K22 @ 16 MHz
 * Board: Focus Applied Technologies - Fuel Stretcher V2.0
 * 
 * PURPOSE:
 * Bench test dedicated to stepping the optical source LED PWM duty cycle:
 *   0% -> 10% -> 20% -> 30% -> 40% -> 50% -> 60% -> 70% -> 80% -> 90% -> 100%
 * 
 * HARDWARE CONNECTIONS:
 * - Pin 15 (injOut2 / White Wire) : Low-side drive via Q2 IGBT (Cathode of Red LED)
 * - +5V (Regulator U1 / Proto Grid): Anode of Red LED (via 220R - 470R resistor)
 * - P1 Pin 2 (TPS / Yellow Wire)  : OPT101 Photodetector (AN24 / RD4 via U2 Op-Amp)
 * - P1 Pin 11 (txRS / Pin 11)     : 9600 Baud RS-232 telemetry output to PuTTY
 * - RD2 (D1)                      : Onboard Green LED (Blinks on step transition)
 * - RD3 (D2)                      : Onboard Red LED (Follows PWM status)
 * 
 * INTERACTIVE CONTROLS (PuTTY & Onboard Buttons):
 * - [SPACE]  : Pause / Freeze current PWM step (for multimeter / sensor checks)
 * - [+]      : Step up +10% PWM
 * - [-]      : Step down -10% PWM
 * - [A]      : Resume automatic 0%-100% cycling
 * - [0]-[9]  : Jump directly to 0%, 10%, 20% ... 90%
 * - [F]      : Jump directly to 100% (Full)
 * ============================================================================
 */

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

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
#pragma config CCP3MX = PORTE0  // P3A/CCP3 muxed with RE0
#pragma config LVP = OFF        // Single-Supply ICSP disabled
#pragma config MCLRE = EXTMCLR  // MCLR pin enabled

#define _XTAL_FREQ 16000000UL   // 16 MHz Internal Fosc

// Pin definitions
#define LED_GREEN_LAT       LATDbits.LATD2       // Onboard Green LED (D1)
#define LED_RED_LAT         LATDbits.LATD3       // Onboard Red LED (D2)

// Pushbuttons (Active LOW)
#define BTN_INJSENS1_PORT   PORTBbits.RB0        // P1 Pin 4 (injSens1)
#define BTN_INJSENS2_PORT   PORTBbits.RB1        // P1 Pin 5 (injSens2)

#define ADC_CH_OPT101       24                   // AN24 / RD4 (P1 Pin 2 TPS via U2 Op-Amp)

// ----------------------------------------------------------------------------
// Prototypes
// ----------------------------------------------------------------------------
void System_Init(void);
void UART1_Init(void);
void UART1_Write(char c);
void UART1_Print(const char *str);
char UART1_ReadNonBlocking(void);

void ADC_Init(void);
uint16_t ADC_Read(uint8_t channel);
uint16_t ADC_Read_Average(uint8_t channel, uint8_t samples);

void PWM_Init(void);
void PWM_SetLED_Percent(uint8_t percent);
void Delay_Ms(uint16_t ms);

// ----------------------------------------------------------------------------
// Main Application
// ----------------------------------------------------------------------------
void main(void) {
    System_Init();

    UART1_Print("\r\n\r\n");
    UART1_Print("======================================================================\r\n");
    UART1_Print("   FAT SMOKE 1.0 - OPTICAL LED PWM BENCH TEST (REV 5.4)\r\n");
    UART1_Print("   Hardware: Fuel Stretcher V2.0 (PIC18F46K22 @ 16 MHz)\r\n");
    UART1_Print("   Target Drive: injOut2 (P1 Pin 15 / White Wire -> Q2 IGBT)\r\n");
    UART1_Print("   Optical Sensor: OPT101 Photodetector on AN24 (P1 Pin 2 TPS)\r\n");
    UART1_Print("   PWM Frequency: 250 Hz (Optimized for IGBT Logic & Gate Drive)\r\n");
    UART1_Print("   Auto Stepping: 0% -> 10% -> 20% -> 30% -> ... -> 100% -> Loop\r\n");
    UART1_Print("   Keyboard Keys: [0-9] Jump 0-90% | [F] 100% | [+] +5% | [-] -5%\r\n");
    UART1_Print("                  [SPACE] Pause/Resume | [A] Auto-Cycle\r\n");
    UART1_Print("======================================================================\r\n\r\n");

    // Stepping array: 0% to 100% in 10% increments
    const uint8_t pwm_steps[11] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    int8_t step_idx = 0;
    uint8_t current_pct = 0;
    bool is_paused = false;
    bool prev_btn_state = false;

    // Power-on indicator: Slow 1 Hz Blink Test on Red LED (1.0s ON, 1.0s OFF x 2)
    UART1_Print("[BOOT] Step 0: Initial 1 Hz Slow Blink Test on Red LED...\r\n");
    for (uint8_t i = 0; i < 2; i++) {
        UART1_Print("  -> LED ON (100% Full Power - 1.0 sec)\r\n");
        LED_GREEN_LAT = 1;
        PWM_SetLED_Percent(100);
        Delay_Ms(1000);

        UART1_Print("  -> LED OFF (0% Dark - 1.0 sec)\r\n");
        LED_GREEN_LAT = 0;
        PWM_SetLED_Percent(0);
        Delay_Ms(1000);
    }
    UART1_Print("[BOOT] Slow blink test complete. Starting 250 Hz PWM stepping...\r\n\r\n");

    while (1) {
        if (!is_paused) {
            current_pct = pwm_steps[step_idx];
        }

        // 1. Apply PWM to LED
        PWM_SetLED_Percent(current_pct);
        
        // Onboard Red LED on if PWM > 0
        LED_RED_LAT = (current_pct > 0) ? 1 : 0;
        
        // Blink Green LED once on step change
        LED_GREEN_LAT = 1;
        Delay_Ms(30);
        LED_GREEN_LAT = 0;

        // 2. Read OPT101 photodetector (take 16 samples for smooth reading)
        uint16_t adc_val = ADC_Read_Average(ADC_CH_OPT101, 16);
        float v_sens = ((float)adc_val * 5.0f) / 1023.0f;
        uint16_t duty_counts = (uint16_t)(((uint32_t)current_pct * 1000UL) / 100UL);

        // 3. Print Clean Telemetry Bar to PuTTY
        char line_buf[120];
        char bar_buf[22];
        uint8_t filled = current_pct / 5; // 0 to 20 '='
        for (uint8_t b = 0; b < 20; b++) {
            bar_buf[b] = (b < filled) ? '=' : '.';
        }
        bar_buf[20] = '\0';

        sprintf(line_buf, "PWM: %3u%% [%s] | Duty: %4u/1000 | OPT101: %4.2fV (ADC: %4u)%s\r\n",
                (unsigned int)current_pct,
                bar_buf,
                (unsigned int)duty_counts,
                v_sens,
                (unsigned int)adc_val,
                is_paused ? " [PAUSED]" : "");
        UART1_Print(line_buf);

        if (v_sens >= 4.15f && current_pct > 0) {
            UART1_Print("  ** [TIP: OPT101 TEPU/SATURATED!] Jauhkan sedikit LED atau alas sehelai tisu **\r\n");
        }

        // 4. Delay step interval (~1.5s) while checking for keyboard or button input
        uint16_t hold_time_ms = 1500;
        uint16_t elapsed_ms = 0;

        while (elapsed_ms < hold_time_ms || is_paused) {
            Delay_Ms(50);
            if (!is_paused) {
                elapsed_ms += 50;
            }

            // Check Onboard Pushbuttons (injSens1 or injSens2)
            bool btn_now = (BTN_INJSENS1_PORT == 0 || BTN_INJSENS2_PORT == 0);
            if (btn_now && !prev_btn_state) {
                // Button pressed! Toggle Pause
                is_paused = !is_paused;
                if (is_paused) {
                    UART1_Print(">> [BUTTON PRESSED] PAUSED! Press again to resume <<\r\n");
                } else {
                    UART1_Print(">> [BUTTON PRESSED] RESUMED! <<\r\n");
                    break;
                }
            }
            prev_btn_state = btn_now;

            // Check PuTTY Serial Keyboard Input
            char ch = UART1_ReadNonBlocking();
            if (ch != 0) {
                if (ch == ' ') {
                    // Spacebar: Toggle Pause
                    is_paused = !is_paused;
                    if (is_paused) {
                        UART1_Print(">> [KEYBOARD] PAUSED! Press [SPACE] to resume, [0-9] to set %, [+] or [-] to adjust <<\r\n");
                    } else {
                        UART1_Print(">> [KEYBOARD] RESUMED! <<\r\n");
                        break;
                    }
                } else if (ch >= '0' && ch <= '9') {
                    // Direct number jump: 0 -> 0%, 1 -> 10%, 2 -> 20% ... 9 -> 90%
                    current_pct = (uint8_t)((ch - '0') * 10);
                    is_paused = true;
                    UART1_Print(">> [MANUAL KEY: Jump to ");
                    char num_str[10];
                    sprintf(num_str, "%u%%", (unsigned int)current_pct);
                    UART1_Print(num_str);
                    UART1_Print("] >>\r\n");
                    break;
                } else if (ch == 'f' || ch == 'F' || ch == 'm' || ch == 'M') {
                    // Full 100%
                    current_pct = 100;
                    is_paused = true;
                    UART1_Print(">> [MANUAL KEY: Jump to 100% MAX] >>\r\n");
                    break;
                } else if (ch == '+' || ch == '=') {
                    // Step Up by 5%
                    is_paused = true;
                    if (current_pct <= 95) current_pct += 5;
                    else current_pct = 100;
                    UART1_Print(">> [MANUAL +5%] >> ");
                    break;
                } else if (ch == '-' || ch == '_') {
                    // Step Down by 5%
                    is_paused = true;
                    if (current_pct >= 5) current_pct -= 5;
                    else current_pct = 0;
                    UART1_Print(">> [MANUAL -5%] >> ");
                    break;
                } else if (ch == 'a' || ch == 'A') {
                    // Auto Resume
                    is_paused = false;
                    step_idx = (int8_t)(current_pct / 10);
                    UART1_Print(">> [AUTO-CYCLE ACTIVE] <<\r\n");
                    break;
                }
            }
        }

        // Advance to next step if not paused
        if (!is_paused) {
            step_idx++;
            if (step_idx > 10) {
                step_idx = 0; // Wrap around to 0%
                UART1_Print("\r\n--- [RESTARTING 0% TO 100% CYCLE] ---\r\n\r\n");
                Delay_Ms(400);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// System Initialization
// ----------------------------------------------------------------------------
void System_Init(void) {
    // 1. Configure Internal Oscillator to 16 MHz
    OSCCONbits.IRCF = 0b111;    // 16 MHz HF-INTOSC
    while (!OSCCONbits.HFIOFS); // Wait until stable

    // 2. Configure I/O Ports
    // Onboard Status LEDs: RD2 (Green D1), RD3 (Red D2)
    TRISDbits.TRISD2 = 0; ANSELDbits.ANSD2 = 0; LED_GREEN_LAT = 0;
    TRISDbits.TRISD3 = 0; ANSELDbits.ANSD3 = 0; LED_RED_LAT = 0;

    // Pushbutton Inputs with PORTB Weak Pull-ups
    TRISBbits.TRISB0 = 1; ANSELBbits.ANSB0 = 0; // P1 Pin 4 (injSens1)
    TRISBbits.TRISB1 = 1; ANSELBbits.ANSB1 = 0; // P1 Pin 5 (injSens2)
    INTCON2bits.RBPU = 0; // 0 = PORTB Pull-ups ENABLED
    WPUBbits.WPUB0   = 1;
    WPUBbits.WPUB1   = 1;

    // PWM Pin: RC2 / Pin 15 injOut2 (Q2 IGBT Gate)
    TRISCbits.TRISC2 = 0;
    ANSELCbits.ANSC2 = 0;

    // Safety: Force Heater and Blower OFF during LED bench test
    TRISDbits.TRISD1 = 0; ANSELDbits.ANSD1 = 0; LATDbits.LATD1 = 0; // Heater Q1 OFF
    TRISCbits.TRISC1 = 0; LATCbits.LATC1 = 0;                       // Blower Q3 OFF

    // Analog Inputs: RD4 / AN24 (P1 Pin 2 TPS - OPT101 photodetector via U2 op-amp)
    TRISDbits.TRISD4 = 1;
    ANSELDbits.ANSD4 = 1;

    // 3. Initialize Peripherals
    ADC_Init();
    PWM_Init();
    UART1_Init();
    Delay_Ms(150); // RS-232 charge pump stabilize
}

// ----------------------------------------------------------------------------
// PWM Driver (Timer2 & CCP1 @ 250 Hz for Automotive Ignition IGBT Gate Drive)
// ----------------------------------------------------------------------------
void PWM_Init(void) {
    // Set PWM Period for ~250 Hz at 16 MHz Fosc (Prescaler 1:64, PR2 = 249)
    // Formula: F_pwm = 16,000,000 / (4 * 64 * 250) = 250 Hz (T = 4.0 ms)
    PR2 = 249;
    
    // CCP1 Configuration (RC2 / Pin 15 / injOut2 - Q2 IGBT)
    CCPTMRS0bits.C1TSEL = 0b00;       // CCP1 uses Timer2
    ECCP1AS             = 0;          // Disable auto-shutdown so FLT0/RB0 won't kill PWM!
    PWM1CON             = 0;          // Zero dead-band delay
    CCP1CON             = 0b00001100; // Enhanced PWM Single-Output mode (Active-High)
    PSTR1CON            = 0b00000001; // PWM Steering: Steer PWM to P1A (RC2 / Pin 15 injOut2)
    CCPR1L              = 0;
    CCP1CONbits.DC1B    = 0;

    // Timer2 Configuration: 1:64 Prescaler (250 Hz)
    T2CONbits.T2CKPS = 0b11;    // 1:64 Prescaler
    T2CONbits.TMR2ON = 1;       // Enable Timer2
}

void PWM_SetLED_Percent(uint8_t percent) {
    if (percent > 100) percent = 100;
    
    if (percent == 0) {
        CCPR1L = 0;
        CCP1CONbits.DC1B = 0;
        return;
    }
    
    // Max 10-bit count = (PR2 + 1) * 4 = (249 + 1) * 4 = 1000 counts
    uint16_t duty10 = (uint16_t)(((uint32_t)percent * 1000UL) / 100UL);
    CCPR1L = (uint8_t)(duty10 >> 2);
    CCP1CONbits.DC1B = (uint8_t)(duty10 & 0x03);
}

// ----------------------------------------------------------------------------
// ADC Functions (10-Bit ADC)
// ----------------------------------------------------------------------------
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
    __delay_us(25);              // Settle time
    ADCON0bits.GO_nDONE = 1;     // Start conversion
    while (ADCON0bits.GO_nDONE); // Poll conversion complete
    return ((uint16_t)((ADRESH << 8) | ADRESL));
}

uint16_t ADC_Read_Average(uint8_t channel, uint8_t samples) {
    uint32_t accum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        accum += ADC_Read(channel);
        __delay_ms(2);
    }
    return (uint16_t)(accum / samples);
}

// ----------------------------------------------------------------------------
// UART Communication (9600 Baud @ 16 MHz)
// ----------------------------------------------------------------------------
void UART1_Init(void) {
    // 1. Configure UART1 -> P1 Pin 11 (txRS) via SP3232
    BAUDCON1bits.BRG16 = 1;
    TXSTA1bits.BRGH    = 1;
    SPBRGH1            = (416 >> 8);
    SPBRG1             = (416 & 0xFF);

    TRISCbits.TRISC6   = 0;     // TX1 Output
    TRISCbits.TRISC7   = 1;     // RX1 Input
    ANSELCbits.ANSC6   = 0;     // Digital
    ANSELCbits.ANSC7   = 0;     // Digital
    
    TXSTA1bits.SYNC    = 0;     // Asynchronous mode
    RCSTA1bits.SPEN    = 1;     // Enable Serial Port
    TXSTA1bits.TXEN    = 1;     // Enable Transmitter
    RCSTA1bits.CREN    = 1;     // Enable Continuous Receiver

    // 2. Configure UART2 mirror -> Pin 4 (RD6 / txBT_uC)
    BAUDCON2bits.BRG16 = 1;
    TXSTA2bits.BRGH    = 1;
    SPBRGH2            = (416 >> 8);
    SPBRG2             = (416 & 0xFF);

    TRISDbits.TRISD6   = 1;
    ANSELDbits.ANSD6   = 0;
    TXSTA2bits.SYNC    = 0;
    RCSTA2bits.SPEN    = 1;
    TXSTA2bits.TXEN    = 1;
}

void UART1_Write(char c) {
    while (!PIR1bits.TX1IF);
    TXREG1 = c;

    while (!PIR3bits.TX2IF);
    TXREG2 = c;
}

void UART1_Print(const char *str) {
    while (*str) {
        UART1_Write(*str++);
    }
}

char UART1_ReadNonBlocking(void) {
    if (RCSTA1bits.OERR) {
        RCSTA1bits.CREN = 0;
        RCSTA1bits.CREN = 1;
    }
    if (PIR1bits.RC1IF) {
        return RCREG1;
    }
    return 0;
}

void Delay_Ms(uint16_t ms) {
    while (ms--) {
        __delay_ms(1);
    }
}
