/*
 * ============================================================================
 * Project:  FAT SMOKE 1.0 - CONTINUOUS SMOKE ANALYZER
 * File:     oled_sh1106.h
 * Target:   PIC18F46K22 @ 16 MHz
 * Purpose:  Zero-Dependency Lightweight Software I2C Driver for 1.3" OLED (SH1106)
 * Features:
 *   - Auto-detection across 4 Wiring Configurations:
 *       * Mode 0: SW1/SW2 Normal  (RC0 = SCL, RA4 = SDA)
 *       * Mode 1: SW1/SW2 Swapped (RA4 = SCL, RC0 = SDA)
 *       * Mode 2: Header P2 Normal  (RB6 = SCL, RB7 = SDA)
 *       * Mode 3: Header P2 Swapped (RB7 = SCL, RB6 = SDA)
 *   - Pure SH1106 Datasheet-Compliant Initialization Burst
 *   - Forced All-Pixels-ON Test (0xA5) at boot to instantly verify glass illumination!
 *   - Live Serial Diagnostics to COM4 (Baud 9600)
 * ============================================================================
 */

#ifndef OLED_SH1106_H
#define OLED_SH1106_H

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef _XTAL_FREQ
#define _XTAL_FREQ 16000000UL
#endif

// External UART functions from main.c
extern void UART1_Write(char c);
extern void UART1_Print(const char *str);

// Active I2C state
static uint8_t g_i2c_mode      = 1; // Default to Mode 1 (Swapped SW1/SW2)
static uint8_t g_oled_addr      = 0x78;
static bool    g_oled_detected  = false;

// ----------------------------------------------------------------------------
// Low-Level Pin Control
// ----------------------------------------------------------------------------
static inline void i2c_scl_low(void) {
    switch (g_i2c_mode) {
        case 0: LATCbits.LATC0 = 0; TRISCbits.TRISC0 = 0; break;
        case 1: LATAbits.LATA4 = 0; TRISAbits.TRISA4 = 0; break;
        case 2: LATBbits.LATB6 = 0; TRISBbits.TRISB6 = 0; break;
        case 3: LATBbits.LATB7 = 0; TRISBbits.TRISB7 = 0; break;
    }
}

static inline void i2c_scl_high(void) {
    // SCL active push-pull for crisp clock edges
    switch (g_i2c_mode) {
        case 0: LATCbits.LATC0 = 1; TRISCbits.TRISC0 = 0; break;
        case 1: LATAbits.LATA4 = 1; TRISAbits.TRISA4 = 0; break;
        case 2: LATBbits.LATB6 = 1; TRISBbits.TRISB6 = 0; break;
        case 3: LATBbits.LATB7 = 1; TRISBbits.TRISB7 = 0; break;
    }
}

static inline void i2c_sda_drive(uint8_t bit) {
    if (bit) {
        // High: Release line or drive high
        switch (g_i2c_mode) {
            case 0: LATAbits.LATA4 = 1; TRISAbits.TRISA4 = 0; break;
            case 1: LATCbits.LATC0 = 1; TRISCbits.TRISC0 = 0; break;
            case 2: LATBbits.LATB7 = 1; TRISBbits.TRISB7 = 0; break;
            case 3: LATBbits.LATB6 = 1; TRISBbits.TRISB6 = 0; break;
        }
    } else {
        // Low: Pull to GND
        switch (g_i2c_mode) {
            case 0: LATAbits.LATA4 = 0; TRISAbits.TRISA4 = 0; break;
            case 1: LATCbits.LATC0 = 0; TRISCbits.TRISC0 = 0; break;
            case 2: LATBbits.LATB7 = 0; TRISBbits.TRISB7 = 0; break;
            case 3: LATBbits.LATB6 = 0; TRISBbits.TRISB6 = 0; break;
        }
    }
}

static inline void i2c_sda_release(void) {
    // Float line to High-Z for ACK
    switch (g_i2c_mode) {
        case 0: TRISAbits.TRISA4 = 1; break;
        case 1: TRISCbits.TRISC0 = 1; break;
        case 2: TRISBbits.TRISB7 = 1; break;
        case 3: TRISBbits.TRISB6 = 1; break;
    }
}

static inline uint8_t i2c_sda_read(void) {
    switch (g_i2c_mode) {
        case 0: return (uint8_t)(PORTAbits.RA4 & 1);
        case 1: return (uint8_t)(PORTCbits.RC0 & 1);
        case 2: return (uint8_t)(PORTBbits.RB7 & 1);
        case 3: return (uint8_t)(PORTBbits.RB6 & 1);
    }
    return 1;
}

static inline uint8_t i2c_scl_read(void) {
    switch (g_i2c_mode) {
        case 0: return (uint8_t)(PORTCbits.RC0 & 1);
        case 1: return (uint8_t)(PORTAbits.RA4 & 1);
        case 2: return (uint8_t)(PORTBbits.RB6 & 1);
        case 3: return (uint8_t)(PORTBbits.RB7 & 1);
    }
    return 1;
}

static void i2c_dly(void) {
    __delay_us(8); // ~50 kHz clock: highly reliable and immune to capacitance
}

// ----------------------------------------------------------------------------
// Low-Level I2C Functions
// ----------------------------------------------------------------------------
void I2C_Soft_Init(void) {
    // SW1 & SW2 pins as digital output High
    LATCbits.LATC0 = 1; TRISCbits.TRISC0 = 0;
    LATAbits.LATA4 = 1; TRISAbits.TRISA4 = 0;
    
    // Header P2 pins as digital output High with weak pullups enabled
    INTCON2bits.RBPU = 0;
    WPUBbits.WPUB6 = 1;
    WPUBbits.WPUB7 = 1;
    LATBbits.LATB6 = 1; TRISBbits.TRISB6 = 0;
    LATBbits.LATB7 = 1; TRISBbits.TRISB7 = 0;
    
    i2c_scl_high();
    i2c_sda_drive(1);
}

void I2C_Soft_Start(void) {
    i2c_sda_drive(1);
    i2c_scl_high();
    i2c_dly();
    i2c_sda_drive(0);
    i2c_dly();
    i2c_scl_low();
    i2c_dly();
}

void I2C_Soft_Stop(void) {
    i2c_sda_drive(0);
    i2c_dly();
    i2c_scl_high();
    i2c_dly();
    i2c_sda_drive(1);
    i2c_dly();
}

bool I2C_Soft_Write(uint8_t data) {
    for (uint8_t i = 0; i < 8; i++) {
        i2c_sda_drive((data & 0x80) ? 1 : 0);
        i2c_dly();
        i2c_scl_high();
        i2c_dly();
        i2c_scl_low();
        i2c_dly();
        data <<= 1;
    }
    // Read Slave ACK
    // Precharge line to 1 before release so floating open pins don't retain false 0
    i2c_sda_drive(1);
    __delay_us(2);
    i2c_sda_release();
    i2c_dly();
    i2c_scl_high();
    i2c_dly();
    bool ack = (i2c_sda_read() == 0);
    i2c_scl_low();
    i2c_dly();
    return ack;
}

bool I2C_Soft_Probe(uint8_t addr) {
    // 1. Stuck-LOW Line Check
    i2c_scl_high();
    i2c_sda_release();
    i2c_dly();
    if (i2c_sda_read() == 0 || i2c_scl_read() == 0) {
        return false;
    }
    
    // 2. Perform Transaction
    I2C_Soft_Start();
    bool ack = I2C_Soft_Write(addr);
    I2C_Soft_Stop();
    return ack;
}

// ----------------------------------------------------------------------------
// Display Commands & Coordinates
// ----------------------------------------------------------------------------
void SH1106_SetPos(uint8_t page, uint8_t col) {
    uint8_t c = col + 2; // SH1106 visible column offset
    I2C_Soft_Start();
    I2C_Soft_Write(g_oled_addr);
    I2C_Soft_Write(0x00); // Continuous command stream
    I2C_Soft_Write((uint8_t)(0xB0 | (page & 0x07)));
    I2C_Soft_Write((uint8_t)(0x00 | (c & 0x0F)));
    I2C_Soft_Write((uint8_t)(0x10 | ((c >> 4) & 0x0F)));
    I2C_Soft_Stop();
}

// ----------------------------------------------------------------------------
// 5x7 ASCII Font Table
// ----------------------------------------------------------------------------
static const uint8_t font5x7[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // ' ' (0x20)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // '!'
    {0x00, 0x07, 0x00, 0x07, 0x00}, // '"'
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // '#'
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // '$'
    {0x23, 0x13, 0x08, 0x64, 0x62}, // '%'
    {0x36, 0x49, 0x55, 0x22, 0x50}, // '&'
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '''
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // '('
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // ')'
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // '*'
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // '+'
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ','
    {0x08, 0x08, 0x08, 0x08, 0x08}, // '-'
    {0x00, 0x60, 0x60, 0x00, 0x00}, // '.'
    {0x20, 0x10, 0x08, 0x04, 0x02}, // '/'
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // '0' (0x30)
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // '9'
    {0x00, 0x36, 0x36, 0x00, 0x00}, // ':'
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ';'
    {0x08, 0x14, 0x22, 0x41, 0x00}, // '<'
    {0x14, 0x14, 0x14, 0x14, 0x14}, // '='
    {0x00, 0x41, 0x22, 0x14, 0x08}, // '>'
    {0x02, 0x01, 0x51, 0x09, 0x06}, // '?'
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // '@'
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 'A' (0x41)
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 'B'
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 'C'
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 'D'
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 'E'
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // 'F'
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 'G'
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 'H'
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 'I'
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 'J'
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 'K'
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 'L'
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 'M'
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 'N'
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 'O'
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 'P'
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 'Q'
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 'R'
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 'S'
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 'T'
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 'U'
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 'V'
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // 'W'
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 'X'
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 'Y'
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 'Z'
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // '['
    {0x02, 0x04, 0x08, 0x10, 0x20}, // '\'
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ']'
    {0x04, 0x02, 0x01, 0x02, 0x04}, // '^'
    {0x40, 0x40, 0x40, 0x40, 0x40}, // '_'
    {0x00, 0x01, 0x02, 0x04, 0x00}, // '`'
    {0x20, 0x54, 0x54, 0x54, 0x78}, // 'a' (0x61)
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // 'b'
    {0x38, 0x44, 0x44, 0x44, 0x20}, // 'c'
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // 'd'
    {0x38, 0x54, 0x54, 0x54, 0x18}, // 'e'
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // 'f'
    {0x08, 0x14, 0x54, 0x54, 0x3C}, // 'g'
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // 'h'
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // 'i'
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // 'j'
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // 'k'
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // 'l'
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // 'm'
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // 'n'
    {0x38, 0x44, 0x44, 0x44, 0x38}, // 'o'
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // 'p'
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // 'q'
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // 'r'
    {0x48, 0x54, 0x54, 0x54, 0x20}, // 's'
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // 't'
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // 'u'
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // 'v'
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // 'w'
    {0x44, 0x28, 0x10, 0x28, 0x44}, // 'x'
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // 'y'
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // 'z'
    {0x00, 0x08, 0x36, 0x41, 0x00}, // '{'
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // '|'
    {0x00, 0x41, 0x36, 0x08, 0x00}, // '}'
    {0x08, 0x08, 0x2A, 0x1C, 0x08}  // '~'
};

// ----------------------------------------------------------------------------
// Universal Robust Command Dispatchers
// ----------------------------------------------------------------------------
static void oled_delay_ms(uint16_t ms) {
    while (ms--) {
        __delay_ms(1);
    }
}

void OLED_SendCmd1(uint8_t cmd) {
    I2C_Soft_Start();
    I2C_Soft_Write(g_oled_addr);
    I2C_Soft_Write(0x00); // Command stream
    I2C_Soft_Write(cmd);
    I2C_Soft_Stop();
}

void OLED_SendCmd2(uint8_t cmd, uint8_t arg) {
    I2C_Soft_Start();
    I2C_Soft_Write(g_oled_addr);
    I2C_Soft_Write(0x00); // Command stream
    I2C_Soft_Write(cmd);
    I2C_Soft_Write(arg);
    I2C_Soft_Stop();
}

void OLED_Clear(void) {
    for (uint8_t page = 0; page < 8; page++) {
        SH1106_SetPos(page, 0);
        I2C_Soft_Start();
        I2C_Soft_Write(g_oled_addr);
        I2C_Soft_Write(0x40); // Data mode
        for (uint8_t col = 0; col < 132; col++) {
            I2C_Soft_Write(0x00);
        }
        I2C_Soft_Stop();
    }
}

// ----------------------------------------------------------------------------
// Universal Dual-Controller Initialization (SSD1306 / SH1106 / SSD1315)
// ----------------------------------------------------------------------------
void OLED_InitDisplay(void) {
    oled_delay_ms(150); // Generous power-on delay for internal POR circuit
    
    // 1. Display OFF
    OLED_SendCmd1(0xAE);
    
    // 2. Set Clock Divide & Oscillator Frequency
    OLED_SendCmd2(0xD5, 0x80);
    
    // 3. Set Multiplex Ratio (1/64 duty)
    OLED_SendCmd2(0xA8, 0x3F);
    
    // 4. Set Display Offset = 0
    OLED_SendCmd2(0xD3, 0x00);
    
    // 5. Set Display Start Line = 0
    OLED_SendCmd1(0x40);
    
    // 6. Charge Pump / DC-DC Converter (ENABLE BOTH SSD1306 & SH1106 BOOSTERS!)
    // For SSD1306 / SSD1315:
    OLED_SendCmd2(0x8D, 0x14); // 0x8D = Charge Pump, 0x14 = Enable Charge Pump (ignored by SH1106)
    // For SH1106:
    OLED_SendCmd2(0xAD, 0x8B); // 0xAD = DC-DC Mode, 0x8B = Built-in DC-DC ON (ignored by SSD1306)
    OLED_SendCmd1(0x32);       // SH1106 Pump Voltage: 8.0V (ignored by SSD1306)
    
    // 7. Addressing Mode: Page Addressing (native to SH1106, standard for SSD1306)
    OLED_SendCmd2(0x20, 0x02);
    
    // 8. Segment Remap (COL127 = SEG0) & COM Output Scan Direction (Remapped)
    OLED_SendCmd1(0xA1);
    OLED_SendCmd1(0xC8);
    
    // 9. COM Pins Hardware Config
    OLED_SendCmd2(0xDA, 0x12);
    
    // 10. Maximum Contrast Control (255 / 0xFF = Full Brightness)
    OLED_SendCmd2(0x81, 0xFF);
    
    // 11. Pre-charge Period
    OLED_SendCmd2(0xD9, 0xF1);
    
    // 12. VCOMH Deselect Level
    OLED_SendCmd2(0xDB, 0x40);
    
    // 13. Deactivate Scrolling
    OLED_SendCmd1(0x2E);
    
    // 14. Normal Display (non-inverted)
    OLED_SendCmd1(0xA6);
    
    // 15. Set Column Offset (0x02 offset for SH1106 132-col RAM)
    OLED_SendCmd1(0x02);
    OLED_SendCmd1(0x10);
    
    // 16. Entire Display Resume to RAM Content (Standard SSD1306/SH1106)
    OLED_SendCmd1(0xA4);
    
    // 17. Display ON!
    OLED_SendCmd1(0xAF);
    
    oled_delay_ms(100);
    OLED_Clear();
}

// ----------------------------------------------------------------------------
// Scanning Engine
// ----------------------------------------------------------------------------
static const char *MODE_NAMES[4] = {
    "SW1/SW2 Normal  [RC0=SCL, RA4=SDA]",
    "SW1/SW2 Swapped [RA4=SCL, RC0=SDA]",
    "Header P2 Normal  [RB6=SCL, RB7=SDA]",
    "Header P2 Swapped [RB7=SCL, RB6=SDA]"
};

void OLED_ScanAndReport(void) {
    UART1_Print("\r\n--- [I2C DIAGNOSTIC SCANNER] ---\r\n");
    
    for (uint8_t m = 0; m < 4; m++) {
        g_i2c_mode = m;
        I2C_Soft_Init();
        __delay_ms(5);
        
        i2c_scl_high();
        i2c_sda_release();
        __delay_us(50);
        uint8_t scl_val = i2c_scl_read();
        uint8_t sda_val = i2c_sda_read();
        
        UART1_Print(" Mode ");
        char m_ch = '0' + m; UART1_Write(m_ch);
        UART1_Print(": ");
        UART1_Print(MODE_NAMES[m]);
        UART1_Print("\r\n  Pin Levels: SCL=");
        UART1_Write(scl_val ? '1' : '0');
        UART1_Print(", SDA=");
        UART1_Write(sda_val ? '1' : '0');
        
        if (scl_val == 0 || sda_val == 0) {
            UART1_Print(" [STUCK LOW / SHORTED!]\r\n");
            continue;
        }
        
        // Probe 0x78 and 0x7A
        bool ack78 = I2C_Soft_Probe(0x78);
        bool ack7a = I2C_Soft_Probe(0x7A);
        
        UART1_Print(" | Probing: 0x78=");
        UART1_Print(ack78 ? "ACK! " : "NAK ");
        UART1_Print("0x7A=");
        UART1_Print(ack7a ? "ACK!\r\n" : "NAK\r\n");
        
        if (ack78 || ack7a) {
            g_i2c_mode = m;
            g_oled_addr = ack78 ? 0x78 : 0x7A;
            g_oled_detected = true;
            UART1_Print(">> LOCKED! OLED responding on Mode ");
            UART1_Write(m_ch);
            UART1_Print(" at address 0x");
            char abuf[8]; sprintf(abuf, "%02X", g_oled_addr); UART1_Print(abuf);
            UART1_Print(" <<\r\n");
            OLED_InitDisplay();
            return;
        }
    }
    
    g_oled_detected = false;
    UART1_Print(">> RESULT: No ACK on any pin configuration.\r\n");
    UART1_Print("--------------------------------\r\n");
}

void OLED_PrintStatus(void) {
    char sbuf[36];
    uint8_t scl = i2c_scl_read();
    uint8_t sda = i2c_sda_read();
    sprintf(sbuf, "[m:%u,det:%u,SCL:%u,SDA:%u,0x%02X]", 
            (unsigned int)g_i2c_mode, 
            (unsigned int)(g_oled_detected ? 1 : 0), 
            (unsigned int)scl, 
            (unsigned int)sda, 
            (unsigned int)g_oled_addr);
    UART1_Print(sbuf);
}

void OLED_Init(void) {
    I2C_Soft_Init();
    OLED_ScanAndReport();
}

// ----------------------------------------------------------------------------
// Printing Routines
// ----------------------------------------------------------------------------
void OLED_Print(uint8_t page, uint8_t col, const char *str) {
    if (!g_oled_detected) return;
    SH1106_SetPos(page, col);
    I2C_Soft_Start();
    I2C_Soft_Write(g_oled_addr);
    I2C_Soft_Write(0x40); // Data mode
    
    while (*str) {
        char c = *str++;
        if (c < 0x20 || c > 0x7E) c = ' ';
        uint8_t idx = (uint8_t)(c - 0x20);
        for (uint8_t i = 0; i < 5; i++) {
            I2C_Soft_Write(font5x7[idx][i]);
        }
        I2C_Soft_Write(0x00); // 1-pixel space between characters
    }
    I2C_Soft_Stop();
}

static uint8_t expand_nibble(uint8_t nib) {
    uint8_t res = 0;
    if (nib & 1) res |= 0x03;
    if (nib & 2) res |= 0x0C;
    if (nib & 4) res |= 0x30;
    if (nib & 8) res |= 0xC0;
    return res;
}

void OLED_PrintBig(uint8_t start_page, uint8_t col, const char *str) {
    if (!g_oled_detected) return;
    // Upper half
    SH1106_SetPos(start_page, col);
    I2C_Soft_Start();
    I2C_Soft_Write(g_oled_addr);
    I2C_Soft_Write(0x40);
    const char *p = str;
    while (*p) {
        char c = *p++;
        if (c < 0x20 || c > 0x7E) c = ' ';
        uint8_t idx = (uint8_t)(c - 0x20);
        for (uint8_t i = 0; i < 5; i++) {
            uint8_t upper = expand_nibble(font5x7[idx][i] & 0x0F);
            I2C_Soft_Write(upper);
            I2C_Soft_Write(upper); // 2x horizontal
        }
        I2C_Soft_Write(0x00);
        I2C_Soft_Write(0x00);
    }
    I2C_Soft_Stop();

    // Lower half
    SH1106_SetPos((uint8_t)(start_page + 1), col);
    I2C_Soft_Start();
    I2C_Soft_Write(g_oled_addr);
    I2C_Soft_Write(0x40);
    p = str;
    while (*p) {
        char c = *p++;
        if (c < 0x20 || c > 0x7E) c = ' ';
        uint8_t idx = (uint8_t)(c - 0x20);
        for (uint8_t i = 0; i < 5; i++) {
            uint8_t lower = expand_nibble((uint8_t)((font5x7[idx][i] >> 4) & 0x0F));
            I2C_Soft_Write(lower);
            I2C_Soft_Write(lower); // 2x horizontal
        }
        I2C_Soft_Write(0x00);
        I2C_Soft_Write(0x00);
    }
    I2C_Soft_Stop();
}

void OLED_ClearPages(uint8_t start_page, uint8_t end_page) {
    if (!g_oled_detected) return;
    for (uint8_t p = start_page; p <= end_page; p++) {
        SH1106_SetPos(p, 0);
        I2C_Soft_Start();
        I2C_Soft_Write(g_oled_addr);
        I2C_Soft_Write(0x40);
        for (uint8_t col = 0; col < 132; col++) {
            I2C_Soft_Write(0x00);
        }
        I2C_Soft_Stop();
    }
}

void OLED_ShowSplash(void) {
    if (!g_oled_detected) return;
    
    // --- STAGE 1: HARDWARE PROOF TEST (Force ALL Pixels ON for 2.0 seconds) ---
    // If the glass and DC-DC booster work, the ENTIRE PANEL turns solid bright white!
    UART1_Print("\r\n[OLED] >>> HARDWARE ILLUMINATION TEST (0xA5: ALL PIXELS ON - 2 SEC) <<<\r\n");
    OLED_SendCmd1(0xA5); // Force All Pixels ON
    oled_delay_ms(2000);
    
    // Return to normal RAM mode
    OLED_SendCmd1(0xA4); // Output follows RAM content
    OLED_Clear();
    
    // --- STAGE 2: Splash Text with Solid White Borders ---
    SH1106_SetPos(0, 0);
    I2C_Soft_Start();
    I2C_Soft_Write(g_oled_addr);
    I2C_Soft_Write(0x40);
    for (uint8_t i = 0; i < 128; i++) I2C_Soft_Write(0xFF);
    I2C_Soft_Stop();

    OLED_Print(1, 16, "==================");
    OLED_Print(2, 22, "FAT SMOKE 1.0");
    OLED_Print(3, 10, "DIESEL OPACIMETER");
    OLED_Print(4, 16, "==================");
    OLED_Print(5, 14, "BENCH TEST MODE");
    OLED_Print(6, 28, "REV 5.0 (XC8)");

    SH1106_SetPos(7, 0);
    I2C_Soft_Start();
    I2C_Soft_Write(g_oled_addr);
    I2C_Soft_Write(0x40);
    for (uint8_t i = 0; i < 128; i++) I2C_Soft_Write(0xFF);
    I2C_Soft_Stop();

    __delay_ms(1500);
    OLED_Clear();
}

void OLED_ShowDashboard(float opacity, float k, float v_sens, float tube_temp, uint8_t pwm_duty, const char *state_str) {
    if (!g_oled_detected) return;
    char buf[24];
    
    // Page 0: Title Bar
    OLED_Print(0, 0, "[ FAT SMOKE 1.0 ]   ");
    
    // Page 1 & 2: Big Bold Opacity % (e.g. "OP: 12.5%")
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 100.0f) opacity = 100.0f;
    sprintf(buf, "OP:%5.1f%%", opacity);
    OLED_PrintBig(1, 4, buf);
    
    // Page 4: Extinction Coefficient k (m^-1)
    if (k < 0.0f) k = 0.0f;
    sprintf(buf, "k  :%5.2f m^-1   ", k);
    OLED_Print(4, 0, buf);
    
    // Page 5: Sensor Voltage & LED PWM Duty
    sprintf(buf, "Vs :%4.2fV PWM:%3u%% ", v_sens, (unsigned int)((pwm_duty * 100) / 255));
    OLED_Print(5, 0, buf);
    
    // Page 6: Chamber Temperature & Status
    sprintf(buf, "Tmp:%4.1fC [BENCH]  ", tube_temp);
    OLED_Print(6, 0, buf);
    
    // Page 7: State Machine Status Bar
    sprintf(buf, "ST : %-12s   ", state_str);
    OLED_Print(7, 0, buf);
}

#endif // OLED_SH1106_H
