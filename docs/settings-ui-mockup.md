# Settings UI Mockup

## Goal

ทำให้หน้า Settings ใช้งานง่ายบนจอ e-ink และปุ่มกดจริงของ X3:

- เห็นภาพรวมเร็ว
- เข้าเมนูที่ใช้บ่อยได้ใน 1-2 จอ
- ลดความงงจาก tab ด้านบน
- ค่า current setting ต้องอ่านง่ายกว่าชื่อเมนู

แนวคิดหลักคือเปลี่ยนจาก `tab + long list` ไปเป็น `Settings Home -> Category Page -> Detail/Picker`

และต้องออกแบบโดยสมมติว่า:

- ผู้ใช้ `แตะหน้าจอไม่ได้`
- ผู้ใช้มีแค่ `Up / Down / Confirm / Back`
- ผู้ใช้ต้องเข้าใจได้จากภาพ ไม่ใช่จากการลองกดมั่ว
- ความสวยต้องไม่ทำให้ปุ่มใช้งานยากขึ้น

---

## Structure

### Screen 1: Settings Home

หน้าแรกเป็น “หมวดใหญ่” 4 หมวด ไม่แสดงรายการยาวทันที

- Reading
- Display
- Controls
- System

แต่ละหมวดมี summary สั้น 2-3 ค่า เพื่อให้รู้ทันทีว่าตอนนี้ตั้งอะไรอยู่

ตัวอย่าง:

```text
+--------------------------------------------------+
| Settings                                  PokiInk |
+--------------------------------------------------+
|                                                  |
| > Reading                                        |
|   Cloud Loop • 24 pt • Status bar on             |
|                                                  |
|   Display                                        |
|   Custom sleep • Refresh 5 pages • Clock on      |
|                                                  |
|   Controls                                       |
|   Tilt L/R • Front buttons remapped              |
|                                                  |
|   System                                         |
|   Thai / English • Bangkok • Sleep 10 min        |
|                                                  |
+--------------------------------------------------+
| Back                    Open                      |
+--------------------------------------------------+
```

### ทำไมแบบนี้ดีกว่า

- ผู้ใช้ไม่ต้องจำว่าตอนนี้อยู่ tab ไหน
- ปุ่มขึ้นลงใช้กับหมวดได้ตรงไปตรงมา
- เห็นค่าหลักทันทีโดยไม่ต้องกดเข้าไปก่อน
- โครงสร้างแบบนี้เหมาะกับ non-touch มากกว่า เพราะทุกจอมีลำดับสายตาเดียว

---

## UX Principles For 4-Button Devices

นี่คือหลักที่ควรยึดตลอดทั้งหน้า settings:

### 1. One Axis At A Time

ในแต่ละหน้าควรมี “ทิศทางหลัก” แค่แบบเดียว

- หน้า list ใช้ `Up / Down`
- หน้า picker ใช้ `Up / Down`
- อย่าให้หน้าหนึ่งต้องทั้งเลื่อนแนวตั้งและสลับ tab แนวนอนพร้อมกัน

เหตุผล:

- ผู้ใช้ไม่มี touch จึงไม่มี affordance จากนิ้ว
- ถ้ามี 2 แกนพร้อมกัน ผู้ใช้ต้องเดาตลอดว่าตอนนี้ปุ่มจะไปควบคุมอะไร

### 2. Back Must Always Mean The Same Thing

`Back` ต้องแปลว่า “ย้อนออก 1 ชั้น” เสมอ

- ไม่ใช้เป็น toggle
- ไม่ใช้เป็น cancel บางหน้า แต่เป็น jump home ในอีกหน้าหนึ่ง

เหตุผล:

- เครื่องกดด้วยนิ้วโป้งแบบไม่มองปุ่มบ่อย
- consistency สำคัญกว่าความลัด

### 3. Confirm Must Have One Clear Job

`Confirm` ต้องมีความหมายเดียวในแต่ละจอ

- หน้า home = เปิดหมวด
- หน้า category = เปลี่ยนค่า หรือเปิด detail
- หน้า picker = เลือกค่าปัจจุบัน

อย่ามีหน้าที่กด `Confirm` แล้วบางรายการ toggle ทันที แต่บางรายการย้าย focus ไปอีก panel โดยไม่มีสัญญาณภาพ

### 4. Focus Must Be Obvious

ทุกครั้งผู้ใช้ต้องรู้ทันทีว่า “ตอนนี้เลือกอะไรอยู่”

หลักที่แนะนำ:

- แถวที่เลือกต้องต่างจากแถวอื่นชัด
- ถ้าใช้พื้นดำ ต้องใช้ทั้งแถว ไม่ใช่แค่ลูกศรเล็กๆ
- ถ้าใช้กรอบ ให้กรอบหนาพอและมี padding
- ค่า current value ของแถวที่ถูกเลือกควรเด่นขึ้นด้วย ไม่ใช่โดนกลืน

### 5. Never Hide Important State

ค่าปัจจุบันต้องมองเห็นจากหน้ารายการเลย

ตัวอย่างที่ดี:

- `Refresh Frequency          5 pages`
- `Motion Page Turn           Tilt L/R`
- `Language                   Thai / English`

ตัวอย่างที่ไม่ดี:

- มีแต่ชื่อเมนู แต่ไม่โชว์ค่า ต้องกดเข้าไปดูก่อนทุกครั้ง

---

## Interaction Model

### Recommended Mapping

- `Up` = เลื่อนขึ้น
- `Down` = เลื่อนลง
- `Confirm` = เปิด / เลือก / เปลี่ยน
- `Back` = ย้อนกลับ

### Continuous Press

ถ้าจะรองรับการกดค้าง:

- `Up/Down ค้าง` = scroll เร็ว
- ไม่ควรใช้ `Confirm ค้าง` กับหน้า settings หลัก

### Pagination

ถ้ารายการยาว:

- ให้ list เลื่อนแบบทีละแถว
- ใช้ fade หรือ cut indicator เบาๆ ที่บน/ล่างว่ามีรายการต่อ
- อย่าพยายามยัดทั้งหมวดลงหน้าเดียวจนแน่น

---

## Beauty On E-Ink

ความสวยของ settings บน e-ink ควรมาจาก “จังหวะ” มากกว่า “กราฟิก”

สิ่งที่ทำให้ดูดี:

- ระยะห่างสม่ำเสมอ
- ตัวอักษร 2 ระดับชัดเจน: label กับ value
- เส้นคั่นบางและนุ่ม
- focus state ที่ดูตั้งใจ
- การจัดแนวซ้าย-ขวาที่คม

สิ่งที่ไม่ควรพึ่งมากเกินไป:

- icon เยอะ
- กล่องทึบเต็มหน้า
- ลวดลาย/เส้นประดับเยอะ
- การกลับดำทั้งจอบ่อยๆ

### Visual Tone ที่เหมาะกับ PokiInk

- นุ่ม
- calm
- clean
- ใช้พื้นที่ว่างให้รู้สึกหายใจได้

### Suggested Visual Formula

หนึ่งแถวควรมี:

- `ชื่อ setting` ชิดซ้าย
- `ค่าปัจจุบัน` ชิดขวา
- `chevron >` เฉพาะรายการที่เปิดต่อได้

ถ้าเป็น toggle:

- แสดง `On / Off` ชัดๆ แทนการวาดสวิตช์แบบมือถือ

---

## Visual Hierarchy

เพื่อให้สวยและใช้ง่ายพร้อมกัน ควรแบ่งชั้นสายตาแบบนี้:

### Header

- บาง
- เบา
- ไม่กินพื้นที่แนวตั้งเกินไป

### Primary Text

- ชื่อหมวด / ชื่อ setting
- คม ชัด อ่านง่าย

### Secondary Text

- summary ใต้หมวด
- tip สั้นๆ
- คำอธิบายไฟล์หรือสถานะ

### Current Value

- ต้องเด่นกว่า secondary text
- แนะนำให้ใช้ bold หรือ contrast ที่หนักกว่า

---

## Home Screen UX

หน้าแรกของ settings ควรทำหน้าที่เหมือน “dashboard สั้นๆ”

สิ่งที่ควรเห็นทันที:

- หมวดอะไรบ้าง
- ตอนนี้ค่าหลักของแต่ละหมวดเป็นอะไร
- ถ้ากดเข้าไปจะเจออะไร

### Summary Line ที่ดี

- สั้น
- อ่านรู้เรื่อง
- ไม่ใช้ศัพท์เทคนิคเกินไป

ตัวอย่าง:

- `Cloud Loop • 24 pt • Wide`
- `Custom sleep • Clock on`
- `Tilt L/R • Buttons reversed`
- `Thai / English • Sleep 10 min`

---

## Category Page UX

หน้า category เป็นหน้าที่ผู้ใช้จะอยู่บ่อยที่สุด จึงควร “เบาและเร็ว”

กติกาที่ควรใช้:

- รายการสูงเท่ากันทุกแถว
- หลีกเลี่ยง subtitle 2 บรรทัดทุกแถว เพราะทำให้สแกนยาก
- ถ้ามีค่าที่ยาว ให้ตัดกลาง
- แถวที่เป็น action ควรมี `>` ชัด
- แถวที่เป็น toggle ไม่ต้องมี `>`

### Recommended Row Types

1. Simple toggle

```text
Clock                             On
```

2. Enum / option picker

```text
Refresh Frequency                 5 pages   >
```

3. Action

```text
Select Wallpaper                  poki.bmp  >
```

4. Danger / utility

```text
Clear Reading Cache                          >
```

ควรแยกกลุ่ม utility ไว้ล่างสุดด้วย spacing เพิ่มอีกนิด

---

## Category Page

พอกดเข้าแต่ละหมวด จะเจอรายการแบบอ่านง่าย 1 คอลัมน์ โดยมีค่า current value ชิดขวา

ตัวอย่างหมวด `Display`

```text
+--------------------------------------------------+
| Display                                   3 / 4  |
+--------------------------------------------------+
| Sleep Screen                      Custom      >  |
| Select Wallpaper                  poki-01.bmp >  |
| Refresh Frequency                 5 pages     >  |
| Clock                             On          >  |
| Battery %                         Reader only >  |
| Preview Direction                 Right       >  |
| Sunlight Fading Fix               Off         >  |
|                                                  |
+--------------------------------------------------+
| Back                   Change / Open             |
+--------------------------------------------------+
```

### Pattern ของแต่ละรายการ

- `Toggle` : แสดง `On / Off`
- `Enum` : แสดงค่าปัจจุบัน + `>`
- `Action` : แสดงค่าหรือ subtitle ถ้ามี + `>`
- `Danger action` : แยกไว้ล่างสุด ไม่ปนกับค่าทั่วไป

### รายละเอียดที่ควรใช้

- แสดงได้ 6-7 แถวต่อหน้าแบบโปร่งๆ
- แถวที่เลือกใช้พื้นดำหรือกรอบหนา
- ค่า current value ใช้ font หนากว่าคำอธิบาย
- ถ้าชื่อไฟล์ยาวให้ตัดกลาง เช่น `poki-wall...night.bmp`

---

## Detail Page / Quick Picker

สำหรับ setting ที่ต้องเลือกค่า เช่น font size, sleep mode, timezone

```text
+--------------------------------------------------+
| Sleep Screen                                     |
+--------------------------------------------------+
| Current: Custom                                  |
|                                                  |
|   Dark                                           |
|   Light                                          |
| > Custom                                         |
|   None                                           |
|                                                  |
| Tip: Custom uses image from /sleep/              |
+--------------------------------------------------+
| Back                    Select                    |
+--------------------------------------------------+
```

สำหรับ toggle ที่ simple มาก อาจไม่ต้องเปิดหน้าใหม่ ถ้ากดแล้วสลับทันทีได้

### Rule of Thumb

- ถ้ามีตัวเลือก 2 ค่าและเข้าใจง่าย: toggle ทันทีได้
- ถ้ามี 3 ค่าขึ้นไป: เปิด picker
- ถ้าค่าเปลี่ยนแล้วกระทบพฤติกรรมมาก: ควรมีหน้ากลางให้เห็นก่อนกดเลือก

### Picker UX ที่ดี

- ค่าปัจจุบันโชว์บนสุด
- รายการเลือกไม่เกิน 5-6 แถวต่อหน้า
- ถ้ารายการยาว เช่น timezone ให้มี shortcut หรือแบ่งกลุ่มในอนาคต

---

## Recommended Information Architecture

### Reading

- Font Family
- Font Size
- Line Spacing
- Screen Margin
- Paragraph Alignment
- Embedded Style
- Hyphenation
- Extra Paragraph Spacing
- Text Anti-Aliasing
- Images
- Customise Status Bar
- Thai Dictionary

### Display

- Sleep Screen
- Select Wallpaper
- Refresh Frequency
- Clock
- Battery %
- Preview Direction
- Sunlight Fading Fix

### Controls

- Side Button Layout
- Motion Page Turn
- Long Press Skip
- Short Power Button
- Remap Front Buttons

### System

- Wi-Fi Networks
- Language
- Timezone
- Time to Sleep
- Check for Updates
- Clear Reading Cache
- About

---

## Navigation Rules

เพื่อให้ใช้งานง่ายกับปุ่มจริง:

- `Up/Down` = เลื่อนรายการ
- `OK` = เข้า / เปลี่ยน / เปิด picker
- `Back` = ย้อนกลับ 1 ชั้น
- ในหน้า Home ของ Settings:
  - `Back` = ออกจาก Settings
  - `OK` = เข้าหมวด

สิ่งที่ควรเลี่ยง:

- ใช้ `OK` กับ tab bar ก่อนแล้วค่อยเลื่อนในรายการ
- ซ่อนหมวดไว้ด้านบนจนผู้ใช้ไม่เห็นโครงสร้าง
- ทำให้ action กับ toggle หน้าตาเหมือนกันหมด
- ทำ footer hint ที่เปลี่ยนความหมายมั่วๆ ทุกหน้า

### Button Hint Strategy

แถบ hint ด้านล่างควรคงภาษาที่สม่ำเสมอ:

- หน้า home: `Back / Open`
- หน้ารายการ: `Back / Change`
- หน้า picker: `Back / Select`
- หน้า action: `Back / Open`

ผู้ใช้จะเริ่มจำ muscle memory ได้เร็วมากถ้าคำพวกนี้ไม่สลับไปมา

---

## Visual Direction

โทนที่เหมาะกับ PokiInk:

- monochrome, โปร่ง, นุ่ม, อ่านง่าย
- มี icon เล็กๆ ได้ แต่ไม่ควรแน่น
- ใช้เส้นคั่นบางๆ มากกว่ากล่องหนาเต็มจอ
- เน้น whitespace มากกว่าลูกเล่น

ข้อเสนอ:

- Header บางลง
- ใช้ subtitle เล็กสำหรับ summary
- ค่า current value ชิดขวาให้สแกนเร็ว
- ปุ่ม hint ด้านล่างคงไว้แบบเดิมเพื่อไม่ต้องเรียนรู้ใหม่
- ใช้ focus state ที่ “นุ่มแต่ชัด” เช่น inverse row หรือเส้นกรอบหนา 2px
- เวลาสลับหน้าควรรีเฟรชเฉพาะส่วนที่จำเป็นเพื่อลดการกระพริบ

---

## What Makes It Feel Premium

แม้ไม่มี touch ก็ทำให้รู้สึก premium ได้ ถ้าใส่ใจ 4 เรื่องนี้:

1. Focus ไม่หลุด
2. ข้อความไม่แน่น
3. ค่า current value อ่านได้ไว
4. ทุกหน้ามีจังหวะใช้งานเหมือนกัน

พูดง่ายๆ คือ “สวย” บนเครื่องแบบนี้ไม่ได้แปลว่าต้องมีองค์ประกอบเยอะ แต่แปลว่ากดแล้วรู้สึกนิ่ง มั่นใจ และไม่ต้องเดา

---

## Best First Implementation

ถ้าจะเริ่มทำแบบคุ้มที่สุด ผมแนะนำลำดับนี้:

1. เปลี่ยน `SettingsActivity` ให้เป็นหน้า `Settings Home`
2. เพิ่ม `SettingsCategoryActivity` สำหรับแต่ละหมวด
3. ทำ row component กลางให้รองรับ `toggle / value / action`
4. คงโครง `SettingInfo` เดิมไว้ เพื่อลดการรื้อ logic
5. ค่อยเพิ่ม detail picker สำหรับ enum ที่ใช้บ่อย

แนวนี้จะได้ UI ใหม่ที่ดูง่ายขึ้นมาก โดยไม่ต้องรื้อ data model ทั้งก้อน

---

## Quick Mockup Summary

```text
Settings Home
  -> Reading
  -> Display
  -> Controls
  -> System

Category Page
  -> row list with current values

Detail Page
  -> simple picker or toggle
```

ถ้าจะทำต่อ ผมแนะนำเริ่มจาก `Settings Home + Display page` ก่อน เพราะเห็นผลเร็วสุดและเป็นหมวดที่ใช้บ่อยสุด

ถ้าจะให้ผมช่วยต่อ ผมทำให้ได้ 2 แบบ:

1. ร่าง mockup แบบละเอียดขึ้นอีกเป็น ASCII หลายหน้า
2. เริ่มลงมือเขียน UI จริงในโค้ดจาก `Settings Home` ก่อน
