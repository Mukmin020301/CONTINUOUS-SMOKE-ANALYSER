import os
from PIL import Image, ImageDraw, ImageFont

# Load cropped PCB
pcb = Image.open('q2_region_test.jpg').convert('RGBA')
pw, ph = pcb.size

# Let's crop slightly tighter around Q2 and connector P1
# x: 0 to 260, y: 10 to 220
tight = pcb.crop((0, 10, 260, 220))
tw, th = tight.size

# Upscale 3x
scale = 3
tight = tight.resize((tw * scale, th * scale), Image.Resampling.LANCZOS)
tw, th = tight.size

canvas = Image.new('RGB', (1420, 860), color=(15, 23, 42))
draw = ImageDraw.Draw(canvas)

# Title Bar
draw.rectangle([(0, 0), (1420, 70)], fill=(30, 41, 59))

try:
    font_title = ImageFont.truetype('arialbd.ttf', 24)
    font_sub = ImageFont.truetype('arialbd.ttf', 17)
    font_body = ImageFont.truetype('arial.ttf', 15)
    font_body_b = ImageFont.truetype('arialbd.ttf', 15)
    font_label = ImageFont.truetype('arialbd.ttf', 13)
except:
    font_title = font_sub = font_body = font_body_b = font_label = None

draw.text((30, 20), 'PANDUAN PEMASANGAN TRANSISTOR PAD Q2 (PWM LED 550nm)', fill=(251, 191, 36), font=font_title)

# Paste PCB image
canvas.paste(tight, (30, 90))

# Red outline around Q2 footprint
qx1, qy1 = int(95 * scale) + 30, int((32 - 10) * scale) + 90
qx2, qy2 = int(225 * scale) + 30, int((95 - 10) * scale) + 90

draw.rectangle([(qx1, qy1), (qx2, qy2)], outline=(239, 68, 68), width=4)
draw.rectangle([(qx1, qy1 - 30), (qx1 + 290, qy1)], fill=(239, 68, 68))
draw.text((qx1 + 10, qy1 - 25), 'PAD TRANSISTOR Q2 (KOSONG)', fill=(255, 255, 255), font=font_label)

# Pin 1 Gate: x ~ 205, y ~ 45
p1x, p1y = int(205 * scale) + 30, int((45 - 10) * scale) + 90
draw.ellipse([(p1x - 10, p1y - 10), (p1x + 10, p1y + 10)], fill=(59, 130, 246), outline=(255, 255, 255), width=2)
draw.line([(p1x, p1y), (p1x + 60, p1y - 35)], fill=(59, 130, 246), width=3)
draw.rectangle([(p1x + 60, p1y - 55), (p1x + 360, p1y - 15)], fill=(30, 58, 138))
draw.text((p1x + 70, p1y - 48), 'PIN 1: GATE / BASE (PIC RC2 via R17)', fill=(147, 197, 253), font=font_label)

# Pin 2 / Tab Drain: x ~ 140, y ~ 65
p2x, p2y = int(140 * scale) + 30, int((65 - 10) * scale) + 90
draw.ellipse([(p2x - 10, p2y - 10), (p2x + 10, p2y + 10)], fill=(234, 179, 8), outline=(255, 255, 255), width=2)
draw.line([(p2x, p2y), (p2x, p2y + 65)], fill=(234, 179, 8), width=3)
draw.rectangle([(p2x - 140, p2y + 65), (p2x + 230, p2y + 110)], fill=(113, 63, 18))
draw.text((p2x - 130, p2y + 72), 'PIN 2 / TAB: DRAIN / COLLECTOR', fill=(253, 224, 71), font=font_label)
draw.text((p2x - 130, p2y + 90), 'Tersambung ke Pin 15 (injOut2 / Wayar Putih)', fill=(254, 240, 138), font=font_label)

# Pin 3 Source: x ~ 205, y ~ 75
p3x, p3y = int(205 * scale) + 30, int((75 - 10) * scale) + 90
draw.ellipse([(p3x - 10, p3y - 10), (p3x + 10, p3y + 10)], fill=(34, 197, 94), outline=(255, 255, 255), width=2)
draw.line([(p3x, p3y), (p3x + 60, p3y + 40)], fill=(34, 197, 94), width=3)
draw.rectangle([(p3x + 60, p3y + 25), (p3x + 360, p3y + 65)], fill=(20, 83, 45))
draw.text((p3x + 70, p3y + 35), 'PIN 3: SOURCE / EMITTER (ke Ground/GND)', fill=(134, 239, 172), font=font_label)

# Wayar Putih Pin 15 callout
wpx, wpy = 30 + int(35 * scale), 90 + int((48 - 10) * scale)
draw.ellipse([(wpx - 8, wpy - 8), (wpx + 8, wpy + 8)], fill=(255, 255, 255), outline=(0, 0, 0), width=2)
draw.line([(wpx, wpy), (wpx - 10, wpy - 45)], fill=(255, 255, 255), width=2)
draw.rectangle([(10, wpy - 75), (280, wpy - 45)], fill=(51, 65, 85))
draw.text((20, wpy - 68), 'Pin 15 (injOut2 - Wayar Putih)', fill=(255, 255, 255), font=font_label)

# Right Side Panel: Explanations
rx = 840
draw.rectangle([(rx, 90), (1390, 385)], fill=(30, 41, 59), outline=(71, 85, 105), width=2)
draw.text((rx + 20, 105), 'KENAPA BOSS SURUH INSTALL TRANSISTOR?', fill=(56, 189, 248), font=font_sub)
draw.text((rx + 20, 140), '1. LED 550nm perlukan kawalan PWM automatik:', fill=(241, 245, 249), font=font_body_b)
draw.text((rx + 20, 165), '   PIC perlu adjust kecerahan LED supaya OPT101', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 188), '   dapat baca tepat 4.00V (Clean Air Baseline).', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 220), '2. Transistor Q2 adalah suis Low-Side (ke GND):', fill=(241, 245, 249), font=font_body_b)
draw.text((rx + 20, 245), '   PIC Pin 36 (RC2) bagi denyutan PWM -> Gate Q2 switch', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 268), '   Katod (-) LED ke Ground pada frekuensi 19.5 kHz.', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 300), '3. Di board hang sekarang, pad Q2 masih KOSONG!', fill=(239, 68, 68), font=font_body_b)
draw.text((rx + 20, 325), '   Cuma ada solder pool & dawai jumper tanpa transistor.', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 350), '   Mesti pasang 1 transistor (MOSFET / NPN) di situ!', fill=(251, 191, 36), font=font_body_b)

# Box 2: Komponen yang boleh guna
draw.rectangle([(rx, 405), (1390, 715)], fill=(30, 41, 59), outline=(71, 85, 105), width=2)
draw.text((rx + 20, 420), 'PILIHAN KOMPONEN YANG BOLEH DIGUNAKAN:', fill=(74, 222, 128), font=font_sub)
draw.text((rx + 20, 455), 'PILIHAN A: N-Channel Logic MOSFET (PALING BAGUS)', fill=(255, 255, 255), font=font_body_b)
draw.text((rx + 20, 480), '- Contoh SMD: DPAK GD18N40LZ / IRLR024N / FDD8447L', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 503), '- Contoh Melalui Lubang (TO-92): 2N7000 / BS170', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 526), '  Pinout: Gate -> Pin 1  |  Drain -> Tab  |  Source -> Pin 3', fill=(147, 197, 253), font=font_body_b)

draw.text((rx + 20, 560), 'PILIHAN B: Bipolar NPN Transistor (Mudah Jumpa)', fill=(255, 255, 255), font=font_body_b)
draw.text((rx + 20, 585), '- Contoh: 2N2222 / 2N3904 / BC547 / BD139', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 608), '  Pinout: Base -> Pin 1  |  Collector -> Tab  |  Emitter -> Pin 3', fill=(253, 224, 71), font=font_body_b)

draw.text((rx + 20, 642), 'LITAR PENDAWAIAN LED 550nm:', fill=(245, 158, 11), font=font_body_b)
draw.text((rx + 20, 668), '+5V Board ---> [ Resistor 68 - 100 Ohm ] ---> Anod (+) LED', fill=(241, 245, 249), font=font_body)
draw.text((rx + 20, 690), 'Katod (-) LED ---> Sambung ke Pin 15 (Wayar Putih)', fill=(56, 189, 248), font=font_body_b)

# Bottom Full Width Box: Current 160mA explanation
draw.rectangle([(30, 735), (1390, 835)], fill=(30, 41, 59), outline=(245, 158, 11), width=2)
draw.text((45, 747), 'KENAPA SETUP INI TARIK CURRENT SAMPAI 160mA? ADAKAH SEBAB NANO?', fill=(245, 158, 11), font=font_sub)
draw.text((45, 777), 'YA, SEBAHAGIAN BESARNYA KERANA NANO + OLED! Pecahan arus pada 12V:', fill=(241, 245, 249), font=font_body_b)
draw.text((45, 800), 'Arduino Nano (~35mA) + OLED SH1106 (~30mA) + PIC18F (~20mA) + Linear Reg 1117 & LED (~70mA) = 155-160 mA. SANGAT NORMAL & SIHAT (~1.9W)!', fill=(148, 163, 184), font=font_body)

canvas.save('q2_transistor_pad_guide.png')
print('Enhanced guide saved successfully: q2_transistor_pad_guide.png')
