'****************************************************************
'*  Name    : FAT_SMOKE_LED_PWM.PBP                             *
'*  Author  : Mukmin & Engineering Team                         *
'*  Notice  : Focus Applied Technologies (c) 2026               *
'*  Target  : PIC18F46K22 @ 16 MHz (Fuel Stretcher V2.0 PCB)    *
'*  IDE     : MicroCode Studio Plus                             *
'*  Compiler: PICBASIC PRO (PBP3 / PBP 2.60)                    *
'*  Date    : November 2026                                     *
'*                                                              *
'*  PURPOSE : Bench Test Optical Red LED PWM & OPT101 Sensor    *
'*  - PWM Drive : Pin 15 (injOut2 / PORTC.2 via Q2 IGBT)        *
'*  - Optical In: Pin 2 (TPS / PORTD.4 / AN24 via U2 Op-Amp)    *
'*  - Telemetry : Pin 11 (txRS / PORTC.6 at 9600 Baud)          *
'****************************************************************

'OSCILLATOR & CLOCK CONFIGURATION================================
OSCCON = %01110111          ' 16 MHz HF-INTOSC stable
DEFINE OSC 16

'ADC CONFIGURATION===============================================
DEFINE ADC_BITS 10          ' 10-bit Analog to Digital
DEFINE ADC_SAMPLEUS 50      ' 50 us acquisition time
ADCON1 = %00000000          ' Vref+ = VDD (5.0V), Vref- = VSS (0V)
ADCON2 = %10100101          ' Right justified, 8 TAD, FOSC/16
ADCON0.0 = 1                ' Enable ADC module

'PORT I/O CONFIGURATION==========================================
' PORTA: Digital inputs/outputs
ANSELA = %00000000
TRISA  = %11111111

' PORTB: Pushbuttons (injSens1 = RB0, injSens2 = RB1)
ANSELB = %00000000          ' Digital
TRISB  = %11111111          ' Inputs
INTCON2.7 = 0               ' Enable PORTB weak pull-ups
WPUB = %00000011            ' Pull-ups on RB0, RB1

' PORTC: Pin 15 injOut2 (RC2 = PWM), Pin 16 injOut3 (RC1 = Blower), Pin 11 txRS (RC6)
ANSELC = %00000000          ' Digital
TRISC  = %10111001          ' RC2=Out (PWM), RC1=Out (Blower), RC6=Out (TX), RC7=In (RX)
PORTC.1 = 0                 ' Safety: Force Blower OFF

' PORTD: Status LEDs (RD2 Green, RD3 Red), Pin 14 injOut1 (RD1 = Heater), Pin 2 TPS (RD4 = AN24)
ANSELD = %00010000          ' RD4 (AN24) is Analog Input (OPT101)
TRISD  = %11110001          ' RD1=Out (Heater), RD2=Out (Green LED), RD3=Out (Red LED), RD4=In (OPT101)
PORTD.1 = 0                 ' Safety: Force Heater OFF
PORTD.2 = 0                 ' Green LED OFF
PORTD.3 = 0                 ' Red LED OFF

'PORT MAPPING & ALIASES==========================================
LED_Green   VAR PORTD.2     ' Onboard Green LED (D1)
LED_Red     VAR PORTD.3     ' Onboard Red LED (D2)
Btn_SW1     VAR PORTB.0     ' Pushbutton 1 (injSens1)
Btn_SW2     VAR PORTB.1     ' Pushbutton 2 (injSens2)

'VARIABLES=======================================================
pct         VAR BYTE        ' Current PWM % (0 to 100)
duty10      VAR WORD        ' 10-bit PWM duty (0 to 1000)
step_idx    VAR BYTE        ' Step index (0 to 10)
adval       VAR WORD        ' 10-bit ADC raw reading (0 to 1023)
ad_accum    VAR WORD        ' Averaging accumulator
v_mv        VAR WORD        ' Millivolts (0 to 5000 mV)
v_whole     VAR BYTE        ' Whole volts (0 to 5 V)
v_dec       VAR BYTE        ' Decimals (0 to 99)
i           VAR BYTE        ' Loop counter
rx_char     VAR BYTE        ' Received serial byte
is_paused   VAR BIT         ' 1 = Paused, 0 = Auto-Cycling
prev_btn    VAR BIT         ' Button state tracker
btn_now     VAR BIT

'HARDWARE UART SETUP (9600 BAUD @ 16 MHz)=========================
TXSTA1   = %00100100        ' TXEN=1, BRGH=1
RCSTA1   = %10010000        ' SPEN=1, CREN=1
BAUDCON1 = %01001000        ' BRG16=1
SPBRG1   = 160              ' 9600 Baud (416 counts)
SPBRGH1  = 1

'TIMER2 & CCP1 HARDWARE PWM (250 Hz for IGBT Drive)==============
PR2         = 249           ' Period = 250 Hz at 16 MHz with 1:64 prescaler
CCPTMRS0    = %00000000     ' CCP1 uses Timer2
ECCP1AS     = 0             ' Auto-shutdown disabled
PWM1CON     = 0             ' Zero dead-band delay
CCP1CON     = %00001100     ' Single PWM active-high mode
PSTR1CON    = %00000001     ' Steer PWM output to P1A (PORTC.2 / Pin 15)
CCPR1L      = 0             ' Start at 0%
T2CON       = %00000111     ' Timer2 ON, Prescaler 1:64

'INITIALIZATION & BOOT BLINK=====================================
CLEAR
is_paused = 0
prev_btn  = 1
step_idx  = 0

PAUSE 500                   ' Power stabilization delay

HSEROUT [13, 10, 13, 10]
HSEROUT ["======================================================================", 13, 10]
HSEROUT ["   FAT SMOKE 1.0 - OPTICAL LED PWM BENCH TEST (PICBASIC PRO)", 13, 10]
HSEROUT ["   Hardware : Focus Applied Tech Fuel Stretcher V2.0 (18F46K22)", 13, 10]
HSEROUT ["   Drive Pin: Pin 15 (injOut2 / PORTC.2 -> Q2 IGBT)", 13, 10]
HSEROUT ["   Sensor In: Pin 2 (TPS / PORTD.4 / AN24 -> OPT101)", 13, 10]
HSEROUT ["   Frequency: 250 Hz (Optimized for IGBT Gate Drive)", 13, 10]
HSEROUT ["   Stepping : 0% -> 10% -> 20% -> ... -> 100% -> Loop", 13, 10]
HSEROUT ["   Controls : [0-9] Direct % | [F] 100% | [+] +5% | [-] -5%", 13, 10]
HSEROUT ["              [SPACE] Pause/Resume | [A] Auto-Cycle", 13, 10]
HSEROUT ["======================================================================", 13, 10, 13, 10]

' Step 0: Slow 1 Hz Boot Blink (1.0s ON, 1.0s OFF x 2)
HSEROUT ["[BOOT] Step 0: Slow 1 Hz Diagnostic Blink on Red LED...", 13, 10]
FOR i = 1 TO 2
    HSEROUT ["  -> LED ON  (100% Full Power - 1.0 sec)", 13, 10]
    LED_Green = 1
    GOSUB Set_PWM_100
    PAUSE 1000

    HSEROUT ["  -> LED OFF (0% Dark - 1.0 sec)", 13, 10]
    LED_Green = 0
    GOSUB Set_PWM_0
    PAUSE 1000
NEXT i
HSEROUT ["[BOOT] Slow blink complete. Starting 250 Hz PWM stepping...", 13, 10, 13, 10]

'MAIN CONTROL LOOP===============================================
Main_Loop:
    ' 1. Determine current percent from step index if not paused
    IF is_paused = 0 THEN
        pct = step_idx * 10
    ENDIF

    ' 2. Apply PWM to LED
    GOSUB Apply_PWM

    ' 3. Onboard indicator LEDs
    IF pct > 0 THEN
        LED_Red = 1
    ELSE
        LED_Red = 0
    ENDIF
    LED_Green = 1
    PAUSE 30
    LED_Green = 0

    ' 4. Read OPT101 Sensor on AN24 (Average 8 samples)
    ad_accum = 0
    FOR i = 1 TO 8
        ADCON0 = (24 << 2) | %00000001  ' Channel 24 (RD4) + ADC ON
        PAUSEUS 30                      ' Acquisition time
        ADCON0.1 = 1                    ' Start ADC conversion
        WHILE ADCON0.1 = 1: WEND        ' Wait for completion
        adval.BYTE1 = ADRESH
        adval.BYTE0 = ADRESL
        ad_accum = ad_accum + adval
        PAUSE 2
    NEXT i
    adval = ad_accum / 8

    ' 5. Calculate millivolts: v_mv = (adval * 5000) / 1023
    v_mv = (adval * 4) + ((adval * 89) / 100)  ' Scaled approx: adval * 4.8876
    v_whole = v_mv / 1000
    v_dec = (v_mv // 1000) / 10

    ' 6. Print Telemetry to PuTTY
    HSEROUT ["PWM: ", DEC3 pct, "% | Duty: ", DEC4 duty10, "/1000 | OPT101: ", DEC v_whole, ".", DEC2 v_dec, "V (ADC: ", DEC4 adval, ")"]
    IF is_paused = 1 THEN
        HSEROUT [" [PAUSED]"]
    ENDIF
    HSEROUT [13, 10]

    IF v_mv >= 4150 AND pct > 0 THEN
        HSEROUT ["  ** [TIP: OPT101 SATURATED!] Jauhkan sedikit LED / alas sehelai tisu **", 13, 10]
    ENDIF

    ' 7. Delay ~1.5s while checking for keyboard or button input
    FOR i = 1 TO 30
        PAUSE 50

        ' Check pushbuttons
        btn_now = Btn_SW1 & Btn_SW2
        IF btn_now = 0 AND prev_btn = 1 THEN
            is_paused = 1 - is_paused
            IF is_paused = 1 THEN
                HSEROUT [">> [BUTTON PRESSED] PAUSED! Press again to resume <<", 13, 10]
            ELSE
                HSEROUT [">> [BUTTON PRESSED] RESUMED! <<", 13, 10]
                GOTO Next_Step
            ENDIF
        ENDIF
        prev_btn = btn_now

        ' Check serial keyboard input
        IF PIR1.5 = 1 THEN              ' RC1IF (Byte received)
            IF RCSTA1.1 = 1 THEN        ' OERR overrun
                RCSTA1.4 = 0: RCSTA1.4 = 1
            ENDIF
            rx_char = RCREG1

            ' Spacebar: Pause/Resume
            IF rx_char = " " THEN
                is_paused = 1 - is_paused
                IF is_paused = 1 THEN
                    HSEROUT [">> [KEYBOARD] PAUSED! Press [SPACE] to resume, [0-9] to set % <<", 13, 10]
                ELSE
                    HSEROUT [">> [KEYBOARD] RESUMED! <<", 13, 10]
                    GOTO Next_Step
                ENDIF
            ENDIF

            ' Direct numbers '0' to '9'
            IF rx_char >= "0" AND rx_char <= "9" THEN
                pct = (rx_char - "0") * 10
                is_paused = 1
                HSEROUT [">> [KEY DIRECT: ", DEC pct, "%] >>", 13, 10]
                GOTO Loop_Immediate
            ENDIF

            ' 'F' or 'f' for 100%
            IF rx_char = "F" OR rx_char = "f" OR rx_char = "M" OR rx_char = "m" THEN
                pct = 100
                is_paused = 1
                HSEROUT [">> [KEY DIRECT: 100% MAX] >>", 13, 10]
                GOTO Loop_Immediate
            ENDIF

            ' '+' Step Up 5%
            IF rx_char = "+" OR rx_char = "=" THEN
                is_paused = 1
                IF pct <= 95 THEN
                    pct = pct + 5
                ELSE
                    pct = 100
                ENDIF
                HSEROUT [">> [MANUAL +5%] >> "]
                GOTO Loop_Immediate
            ENDIF

            ' '-' Step Down 5%
            IF rx_char = "-" OR rx_char = "_" THEN
                is_paused = 1
                IF pct >= 5 THEN
                    pct = pct - 5
                ELSE
                    pct = 0
                ENDIF
                HSEROUT [">> [MANUAL -5%] >> "]
                GOTO Loop_Immediate
            ENDIF

            ' 'A' Auto Resume
            IF rx_char = "A" OR rx_char = "a" THEN
                is_paused = 0
                step_idx = pct / 10
                HSEROUT [">> [AUTO-CYCLE ACTIVE] <<", 13, 10]
                GOTO Next_Step
            ENDIF
        ENDIF
    NEXT i

Next_Step:
    IF is_paused = 0 THEN
        step_idx = step_idx + 1
        IF step_idx > 10 THEN
            step_idx = 0
            HSEROUT [13, 10, "--- [RESTARTING 0% TO 100% CYCLE] ---", 13, 10, 13, 10]
            PAUSE 400
        ENDIF
    ENDIF

Loop_Immediate:
    GOTO Main_Loop

'SUBROUTINES=====================================================
Apply_PWM:
    IF pct > 100 THEN pct = 100
    IF pct = 0 THEN
        GOTO Set_PWM_0
    ENDIF
    ' duty10 = pct * 10 (0 to 1000 counts)
    duty10 = pct * 10
    CCPR1L = duty10 >> 2
    CCP1CON.5 = duty10.1
    CCP1CON.4 = duty10.0
    RETURN

Set_PWM_0:
    duty10 = 0
    CCPR1L = 0
    CCP1CON.5 = 0
    CCP1CON.4 = 0
    RETURN

Set_PWM_100:
    duty10 = 1000
    CCPR1L = 250
    CCP1CON.5 = 0
    CCP1CON.4 = 0
    RETURN

END
