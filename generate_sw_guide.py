import os
from PIL import Image, ImageDraw, ImageFont

# Load cropped SW1 and SW2 image
sw_img = Image.open('sw1_full_range.png').convert('RGB')
w, h = sw_img.size

# Scale up 3x for crisp drawing
scale = 3
sw_img = sw_img.resize((w * scale, h * scale), Image.Resampling.LANCZOS)
sw_w, sw_h = sw_img.size

# Create wider canvas 1380 x 780
canvas = Image.new('RGB', (1380, 780), color=(15, 23, 42))
draw = ImageDraw.Draw(canvas)

# Title bar
draw.rectangle([(0, 0), (1380, 65)], fill=(30, 41, 59))

try:
    font_title = ImageFont.truetype('arialbd.ttf', 24)
    font_sub = ImageFont.truetype('arialbd.ttf', 17)
    font_body = ImageFont.truetype('arial.ttf', 15)
    font_body_b = ImageFont.truetype('arialbd.ttf', 15)
    font_label = ImageFont.truetype('arialbd.ttf', 13)
except:
    font_title = font_sub = font_body = font_body_b = font_label = None

draw.text((25, 18), 'PANDUAN LUBANG PATERI SW1 & SW2 UNTUK OLED I2C (SCL & SDA)', fill=(251, 191, 36), font=font_title)

# Paste PCB image on the left
canvas.paste(sw_img, (30, 90))

# Highlight SW1 Top Row (SCL)
sw1_t1x, sw1_t1y = 30 + int(63 * scale), 90 + int(45 * scale)
sw1_t2x, sw1_t2y = 30 + int(100 * scale), 90 + int(45 * scale)

draw.ellipse([(sw1_t1x - 12, sw1_t1y - 12), (sw1_t1x + 12, sw1_t1y + 12)], fill=(59, 130, 246), outline=(255, 255, 255), width=2)
draw.ellipse([(sw1_t2x - 12, sw1_t2y - 12), (sw1_t2x + 12, sw1_t2y + 12)], fill=(59, 130, 246), outline=(255, 255, 255), width=2)
draw.line([(sw1_t1x, sw1_t1y), (sw1_t2x, sw1_t2y)], fill=(59, 130, 246), width=3)

# Arrow from SW1 top row
draw.line([(sw1_t1x, sw1_t1y), (sw1_t1x - 40, sw1_t1y - 40)], fill=(59, 130, 246), width=3)
draw.rectangle([(20, sw1_t1y - 75), (280, sw1_t1y - 35)], fill=(30, 58, 138), outline=(96, 165, 250), width=2)
draw.text((30, sw1_t1y - 68), 'SW1: SOLDER SCL (CLOCK)', fill=(255, 255, 255), font=font_label)
draw.text((30, sw1_t1y - 50), '(Pilih salah satu lubang atas ini)', fill=(147, 197, 253), font=font_label)

# Highlight SW2 Top Row (SDA)
sw2_t1x, sw2_t1y = 30 + int(135 * scale), 90 + int(45 * scale)
sw2_t2x, sw2_t2y = 30 + int(172 * scale), 90 + int(45 * scale)

draw.ellipse([(sw2_t1x - 12, sw2_t1y - 12), (sw2_t1x + 12, sw2_t1y + 12)], fill=(234, 179, 8), outline=(255, 255, 255), width=2)
draw.ellipse([(sw2_t2x - 12, sw2_t2y - 12), (sw2_t2x + 12, sw2_t2y + 12)], fill=(234, 179, 8), outline=(255, 255, 255), width=2)
draw.line([(sw2_t1x, sw2_t1y), (sw2_t2x, sw2_t2y)], fill=(234, 179, 8), width=3)

# Arrow from SW2 top row
draw.line([(sw2_t2x, sw2_t2y), (sw2_t2x + 40, sw2_t2y - 40)], fill=(234, 179, 8), width=3)
draw.rectangle([(sw2_t2x + 40, sw2_t2y - 75), (sw2_t2x + 300, sw2_t2y - 35)], fill=(113, 63, 18), outline=(253, 224, 71), width=2)
draw.text((sw2_t2x + 50, sw2_t2y - 68), 'SW2: SOLDER SDA (DATA)', fill=(255, 255, 255), font=font_label)
draw.text((sw2_t2x + 50, sw2_t2y - 50), '(Pilih salah satu lubang atas ini)', fill=(254, 240, 138), font=font_label)

# Highlight Bottom Rows (GND)
sw1_b1x, sw1_b1y = 30 + int(63 * scale), 90 + int(95 * scale)
sw1_b2x, sw1_b2y = 30 + int(100 * scale), 90 + int(95 * scale)
sw2_b1x, sw2_b1y = 30 + int(135 * scale), 90 + int(95 * scale)
sw2_b2x, sw2_b2y = 30 + int(172 * scale), 90 + int(95 * scale)

for bx, by in [(sw1_b1x, sw1_b1y), (sw1_b2x, sw1_b2y), (sw2_b1x, sw2_b1y), (sw2_b2x, sw2_b2y)]:
    draw.ellipse([(bx - 10, by - 10), (bx + 10, by + 10)], fill=(34, 197, 94), outline=(255, 255, 255), width=1)

draw.line([(sw1_b1x, sw1_b1y), (sw1_b1x - 30, sw1_b1y + 40)], fill=(34, 197, 94), width=2)
draw.rectangle([(20, sw1_b1y + 35), (320, sw1_b1y + 75)], fill=(20, 83, 45), outline=(134, 239, 172), width=1)
draw.text((30, sw1_b1y + 45), 'BARIS BAWAH: SEMUA ADALAH GROUND (GND)', fill=(134, 239, 172), font=font_label)

# Right Side Panel: Explanations
rx = 800
rw = 550
draw.rectangle([(rx, 90), (rx + rw, 395)], fill=(30, 41, 59), outline=(71, 85, 105), width=2)
draw.text((rx + 20, 105), 'KENAPA SETIAP SUIS ADA 4 LUBANG?', fill=(56, 189, 248), font=font_sub)
draw.text((rx + 20, 140), '1. Suis push button 6mm mempunyai 4 kaki (2 pasang):', fill=(241, 245, 249), font=font_body_b)
draw.text((rx + 20, 168), '   - Dua lubang BARIS ATAS (Kiri & Kanan) bersambung', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 192), '     terus antara satu sama lain di dalam PCB (Trace PIC).', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 220), '   - Dua lubang BARIS BAWAH (Kiri & Kanan) bersambung', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 244), '     terus ke satah Ground (GND).', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 275), '2. Maksudnya hang hanya perlu pilih SALAH SATU lubang', fill=(251, 191, 36), font=font_body_b)
draw.text((rx + 20, 300), '   daripada dua lubang di BARIS ATAS:', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 328), '   - SW1 Lubang Atas = Sambung ke SCL (Clock PIC RC0)', fill=(147, 197, 253), font=font_body_b)
draw.text((rx + 20, 355), '   - SW2 Lubang Atas = Sambung ke SDA (Data PIC RA4)', fill=(253, 224, 71), font=font_body_b)

# Box 2: Ujian Multimeter
draw.rectangle([(rx, 415), (rx + rw, 740)], fill=(30, 41, 59), outline=(71, 85, 105), width=2)
draw.text((rx + 20, 430), 'CARA SAHKAN GUNA MULTIMETER (1 SAAT SAHAJA):', fill=(74, 222, 128), font=font_sub)
draw.text((rx + 20, 465), '1. Setkan Multimeter ke mod Continuity (Buzzer / Beep).', fill=(241, 245, 249), font=font_body)
draw.text((rx + 20, 495), '2. Sentuh probe HITAM ke Pin 3 (GND):', fill=(241, 245, 249), font=font_body)
draw.text((rx + 20, 525), '   - Bila probe MERAH sentuh lubang BARIS BAWAH:', fill=(134, 239, 172), font=font_body_b)
draw.text((rx + 20, 550), '     -> Meter BEEP panjang! (Sebab baris bawah memang Ground).', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 585), '   - Bila probe MERAH sentuh lubang BARIS ATAS:', fill=(251, 191, 36), font=font_body_b)
draw.text((rx + 20, 610), '     -> Meter SENYAP / tiada beep! (Bukti ia adalah SIGNAL PIC).', fill=(203, 213, 225), font=font_body)
draw.text((rx + 20, 648), '3. Jadi SOLDER WAYAR DI LUBANG BARIS ATAS SAHAJA!', fill=(239, 68, 68), font=font_body_b)
draw.text((rx + 20, 678), '   - Wayar SCL -> Pateri ke Lubang Atas SW1', fill=(147, 197, 253), font=font_body_b)
draw.text((rx + 20, 705), '   - Wayar SDA -> Pateri ke Lubang Atas SW2', fill=(253, 224, 71), font=font_body_b)

# Bottom box
draw.rectangle([(30, 600), (760, 740)], fill=(30, 41, 59), outline=(245, 158, 11), width=1)
draw.text((45, 615), 'RINGKASAN SAMBUNGAN 4 WAYAR OLED 1.3\" I2C:', fill=(245, 158, 11), font=font_sub)
draw.text((45, 645), '1. SCL (Clock) -> Solder ke LUBANG ATAS SW1 (Kaki PIC RC0)', fill=(147, 197, 253), font=font_body_b)
draw.text((45, 670), '2. SDA (Data)  -> Solder ke LUBANG ATAS SW2 (Kaki PIC RA4)', fill=(253, 224, 71), font=font_body_b)
draw.text((45, 695), '3. VCC (+5V)   -> Solder ke Pad VDD Proto Grid (Sebelah kanan SW2)', fill=(241, 245, 249), font=font_body)
draw.text((45, 715), '4. GND (0V)    -> Solder ke LUBANG BAWAH SW1 / SW2 (atau Pin 3)', fill=(134, 239, 172), font=font_body)

canvas.save('sw1_sw2_soldering_guide.png')
print('Wider SW1/SW2 Soldering Guide saved successfully!')
