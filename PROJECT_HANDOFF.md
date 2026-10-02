# FriendOS --- Project Handoff & Development Guide

> Last updated: 2026-10-01\
> Status: Display/UI + Buttons + Microphone + Wi-Fi manager/manual provisioning validated; next: complete Manual Wi-Fi saved-network UX\
> Platform: ESP32-S3 / ESP-IDF 5.5.5

------------------------------------------------------------------------

## 1. Project Overview

FriendOS là firmware/framework cho một hệ sinh thái các thiết bị ESP32
có khả năng:

-   tương tác với người dùng bằng giọng nói;
-   hiển thị khuôn mặt/cảm xúc;
-   sử dụng AI/LLM;
-   nhận biết khả năng của các thiết bị Friend khác;
-   giao tiếp Friend ↔ Friend;
-   chia sẻ capability giữa nhiều ESP32;
-   cho phép AI sử dụng phần cứng như các "tools".

Thiết bị đầu tiên đang phát triển:

-   **Board:** ZHENGCHEN_1_54TFT_WIFI
-   **MCU:** ESP32-S3
-   **Display:** 240×240
-   **Friend name trong quá trình phát triển:** **Mộc**

Mục tiêu không phải chỉ tạo một chatbot ESP32. Mục tiêu dài hạn là xây
dựng một kiến trúc có thể chạy trên nhiều loại ESP32 và cho phép nhiều
thiết bị phối hợp thành một Friend hoặc nhiều Friend giao tiếp với nhau.

------------------------------------------------------------------------

## 2. Final Vision

``` text
User
 ↓
Microphone
 ↓
Speech Recognition / STT
 ↓
FriendOS
 ↓
AI / LLM
 ↓
┌───────────────────────────────┐
│ Personality / Emotion / Tools │
└───────────────────────────────┘
 ↓             ↓              ↓
TTS          Display       FriendLink
 ↓             ↓              ↓
Speaker       Face       Other Friends
```

Ví dụ:

> User: "Mộc, xem phòng bên kia có ai không."

Mộc không có camera nhưng phát hiện một Vision Friend có capability
`camera.capture`. Mộc gửi RPC qua FriendLink, Vision Friend chụp ảnh, AI
vision phân tích, Mộc trả lời bằng speaker và LCD đồng thời thay đổi
biểu cảm.

------------------------------------------------------------------------

## 3. Core Architectural Principles

### 3.1 AI provider không điều khiển hardware trực tiếp

Không thiết kế:

``` text
OpenAI → LCD
OpenAI → GPIO
Xiaozhi → servo
```

Thiết kế:

``` text
AI Provider
     ↓
Friend Personality
     ↓
FriendOS
 ├── Emotion Manager
 ├── Tool Manager
 ├── Display
 ├── Audio
 └── FriendLink
```

Nhờ vậy có thể thay OpenAI, Xiaozhi, Local AI hoặc provider khác mà
không phải viết lại phần cứng/UI.

------------------------------------------------------------------------

## 4. Friend vs Node

### NODE

Một board vật lý, ví dụ:

-   Zhengchen ESP32-S3
-   XIAO ESP32-S3 Sense
-   CYD
-   ESP32 servo controller

### FRIEND

Một thực thể logic. Một Friend có thể gồm một hoặc nhiều Node.

``` text
Mộc
 ├── Voice Node
 │     └── Zhengchen
 ├── Vision Node
 │     └── XIAO ESP32-S3 Sense
 └── Motion Node
       └── ESP32 + servo
```

Một Friend không nhất thiết tương ứng với một ESP32.

------------------------------------------------------------------------

## 5. Planned Hardware Roles

### Zhengchen --- Voice Friend / Main UI

Capabilities dự kiến:

-   microphone
-   speaker
-   240×240 display
-   buttons
-   battery monitoring
-   Wi-Fi
-   BLE
-   ESP-NOW

### XIAO ESP32-S3 Sense --- Vision Friend

Capabilities dự kiến:

-   camera
-   microphone
-   image capture
-   vision AI input

Ví dụ capability: `camera.capture`

### CYD --- Control Friend / Dashboard

Có thể dùng để:

-   xem Friend network;
-   xem trạng thái các node;
-   điều khiển;
-   debug FriendLink;
-   hiển thị system information.

------------------------------------------------------------------------

## 6. FriendLink

FriendLink là protocol/application layer để Friend/Node giao tiếp.

Application code không nên gọi trực tiếp `esp_now_send(...)`.

``` text
Application
    ↓
FriendLink
    ↓
Transport
 ├── ESP-NOW
 ├── Wi-Fi
 └── BLE
```

### Transport strategy

**ESP-NOW:** discovery, presence, commands, RPC, events, local
low-latency communication.

**Wi-Fi:** Internet, AI API, large data, image/audio transfer, OTA,
high-bandwidth communication.

**BLE:** onboarding, pairing, provisioning. Không phải transport chính.

**MQTT:** không phải core FriendLink; có thể bổ sung sau như bridge cho
Home Assistant/IoT.

### FriendLink v1 scope

1.  Identity
2.  Discovery
3.  Pairing
4.  Capability
5.  RPC
6.  Event
7.  Presence

Để sau: streaming, OTA distribution, mesh routing, MQTT bridge.

------------------------------------------------------------------------

## 7. Identity, Capability & Presence

Mỗi node cần identity:

``` text
friend_id
node_id
name
protocol_version
```

Discovery packet nên nhỏ:

``` text
HELLO
 ├── ID
 ├── name
 ├── protocol version
 └── capability hash
```

Không broadcast toàn bộ capability profile liên tục. Nếu capability hash
thay đổi thì Friend khác mới request profile đầy đủ.

Hardware tự mô tả capability, ví dụ:

``` text
camera.capture
audio.play
audio.record
display.show
motion.pan
motion.tilt
battery.read
```

AI có thể sử dụng capability như tool:

``` text
AI tool: camera.capture
       ↓
FriendOS Tool Manager
       ↓
FriendLink RPC
       ↓
Vision Node
       ↓
Camera
```

Presence dự kiến:

``` text
ONLINE
BUSY
SLEEPING
OFFLINE
LOW_BATTERY
```

Presence là trạng thái hệ thống/node, không phải emotion.

------------------------------------------------------------------------

## 8. Emotion Architecture

FriendOS phải có emotion abstraction riêng, không phụ thuộc trực tiếp
OpenAI/Xiaozhi.

Emotion dự kiến:

``` text
NEUTRAL
HAPPY
EXCITED
SAD
ANGRY
CONFUSED
THINKING
SURPRISED
SLEEPY
LOVE
LISTENING
SPEAKING
```

Pipeline:

``` text
AI Provider
     ↓
Friend Personality
     ↓
Emotion Manager
     ↓
Display Renderer
```

Display không cần biết emotion đến từ provider nào.

------------------------------------------------------------------------

## 9. AI Strategy

Espressif OpenAI component đã được xem xét cho Chat, Speech,
Transcription, Images, Embeddings.

Voice pipeline dự kiến:

``` text
Microphone → STT → LLM → TTS → Speaker
```

Xiaozhi là nguồn tham khảo tốt cho realtime voice/emotion architecture,
nhưng FriendOS không được phụ thuộc cứng vào Xiaozhi.

Provider abstraction dự kiến:

``` text
AI Provider Interface
 ├── OpenAI Provider
 ├── Xiaozhi Provider
 └── Future Local Provider
```

------------------------------------------------------------------------

## 10. Development Environment --- Confirmed

``` text
OS                  Windows
IDE                 VS Code
ESP-IDF extension   2.3.0
ESP-IDF             5.5.5
IDF path            C:\esp\v5.5.5\esp-idf
ESP tools           C:\Espressif\tools
Python              C:\Espressif\tools\python\v5.5.5\venv\Scripts\python.exe
Project             H:\HISU\Esp32Project-AI\FriendOS
Target              esp32s3
Development port    COM8 (may change)
```

Không hardcode COM8 vào source.

### IDF_TARGET issue đã gặp

Có lúc:

``` text
sdkconfig target = esp32s3
IDF_TARGET       = esp32
```

Fix tạm trong PowerShell:

``` powershell
$env:IDF_TARGET="esp32s3"
echo $env:IDF_TARGET
```

Phải trả về `esp32s3`.

Không vô tình chạy `idf.py set-target esp32`. Board là ESP32-S3.

------------------------------------------------------------------------

## 11. Confirmed Zhengchen Hardware

Các thông tin sau đã được kiểm tra trực tiếp bằng esptool/runtime, không
chỉ dựa vào listing.

### MCU

``` text
ESP32-S3
Package       QFN56
Revision      v0.2
Crystal       40 MHz
Wireless      Wi-Fi + BLE
```

### Flash

`python -m esptool --port COM8 flash_id` xác nhận:

``` text
Manufacturer       ef
Device             4018
Detected size      16 MB
Flash type eFuse   Quad
Flash voltage      3.3 V
```

Boot config đang chạy:

``` text
Flash mode       DIO
Flash frequency  80 MHz
Flash size       16 MB
```

### PSRAM

esptool báo `Embedded PSRAM 8MB (AP_3v3)`.

Runtime xác nhận:

``` text
Vendor       AP Memory
Generation   3
Density      64 Mbit
Capacity     8 MB
Voltage      3 V
Mode         OCTAL
Speed tested 40 MHz
```

Known-good runtime log:

``` text
esp_psram: Found 8MB PSRAM device
esp_psram: Speed: 40MHz
esp_psram: SPI SRAM memory test OK
esp_psram: Adding pool of 8192K of PSRAM memory to heap allocator
```

### Critical PSRAM lesson

**Không cấu hình Quad PSRAM.**

Quad từng gây:

``` text
quad_psram: PSRAM chip is not connected, or wrong PSRAM line mode
cpu_start: Failed to init external RAM!
abort()
```

và reboot loop.

Known-good menuconfig:

``` text
Component config
 → ESP PSRAM
 → Support for external, SPI-connected RAM
 → SPI RAM config

Mode                     Octal
Type                     Auto-detect
Speed                    40 MHz
Initialize during startup Enabled
```

------------------------------------------------------------------------

## 12. LCD --- Confirmed Working

``` text
Controller     ST7789
Resolution     240 × 240
Interface      SPI
Pixel format   RGB565
SPI host       SPI2_HOST
Pixel clock    20 MHz tested
Color inversion REQUIRED
```

### Confirmed pinout

``` c
#define LCD_MOSI 41
#define LCD_SCLK 42
#define LCD_CS   21
#define LCD_DC   40
#define LCD_RST  45
#define LCD_BL   20
```

### Critical LCD quirk --- Color inversion

Sau `esp_lcd_panel_init(panel)` cần:

``` c
ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
```

Không inversion:

``` text
RED   → CYAN
GREEN → MAGENTA
BLUE  → YELLOW
```

Bật inversion:

``` text
RED   → RED
GREEN → GREEN
BLUE  → BLUE
```

Không bỏ inversion trừ khi driver/configuration được chủ động thiết kế
lại.

### RGB565 byte handling

Bring-up renderer đã thành công khi buffer byte được gửi high-byte
trước, low-byte sau:

``` text
RED    F8 00
GREEN  07 E0
BLUE   00 1F
```

Khi tích hợp LVGL phải kiểm tra byte ordering của LVGL; không blindly
giữ hoặc thêm byte swap.

### Geometry test

Một khuôn mặt đơn giản đã hiển thị thành công trên board thật.

Đã xác nhận:

-   full 240×240 works;
-   không thấy coordinate offset bất thường;
-   orientation đúng;
-   không mirror;
-   rectangle drawing đúng;
-   background fill đúng;
-   màu đúng.

Không thay orientation/offset nếu không có lý do cụ thể.

### LVGL integration --- validated

LVGL 9 đã được tích hợp và chạy trên ST7789 240×240.

Known-good design:

-   partial rendering;
-   two render buffers;
-   buffer height: 40 lines;
-   ESP Timer dùng làm LVGL millisecond tick source;
-   `lv_timer_handler()` được service định kỳ;
-   RGB565 buffer cần `lv_draw_sw_rgb565_swap(...)` trước khi gửi LCD;
-   `esp_lcd_panel_draw_bitmap(...)` queue DMA transfer;
-   không gọi `lv_display_flush_ready()` ngay trong flush callback;
-   chỉ gọi `lv_display_flush_ready()` từ ESP-LCD `on_color_trans_done`
    callback sau khi transfer thực sự hoàn tất.

Điều này tránh việc LVGL tái sử dụng render buffer khi DMA vẫn đang đọc.

------------------------------------------------------------------------

## 13. Peripheral Pinout Awaiting Validation

LCD đã được xác minh thực nghiệm. Các pin dưới đây là thông tin board
nhưng **chưa được FriendOS bring-up xác minh**.

### Microphone

``` c
#define MIC_WS   4
#define MIC_SCK  5
#define MIC_SD   6
```

### Speaker

``` c
#define SPK_DOUT 7
#define SPK_BCLK 15
#define SPK_LRCK 16
```

### Buttons

``` c
#define BTN_BOOT     0
#define BTN_VOL_UP   10
#define BTN_VOL_DOWN 39
```

### Battery

``` c
#define BAT_ADC 8
```

------------------------------------------------------------------------

## 14. Current Bring-up Status

``` text
ESP32-S3                 PASS
Flash 16 MB              PASS
PSRAM 8 MB               PASS
PSRAM Octal              PASS
PSRAM memory test        PASS
LCD ST7789               PASS
LCD SPI                  PASS
240×240 geometry         PASS
Backlight                PASS
RGB565                   PASS
Color inversion          PASS
Orientation              PASS
LVGL 9                   PASS
LVGL partial buffer      PASS
LVGL double buffer       PASS
LVGL RGB565 byte swap    PASS
ESP-LCD DMA callback     PASS
ESP Timer / LVGL tick    PASS
Mộc Face UI              PASS
Natural blink            PASS
Emotion UI               PASS

Buttons                  NOT TESTED
Microphone               NOT TESTED
Speaker                  NOT TESTED
Battery ADC              NOT TESTED
Wi-Fi app layer          NOT TESTED
BLE app layer            NOT TESTED
ESP-NOW                  NOT TESTED
AI                       NOT STARTED
FriendLink               NOT STARTED
```

------------------------------------------------------------------------

## 15. Source State & Recommended Structure

`main/main.c` hiện là bring-up/test code. Không tiếp tục để toàn bộ
FriendOS lớn dần trong `main.c`.

Current working structure:

``` text
main/
├── CMakeLists.txt
├── main.c
├── board/
│   ├── board_config.h
│   ├── board_display.h
│   └── board_display.c
└── ui/
    ├── friend_ui.h
    └── friend_ui.c
```

Current responsibility split:

-   `board_config.h`: board-specific pins/config.
-   `board_display.c`: SPI/ST7789/backlight/LVGL display bridge.
-   `friend_ui.c`: Mộc face/emotion/blink UI.
-   `main.c`: orchestration and temporary bring-up tests.

Recommended direction:

``` text
FriendOS/
├── main/
│   └── main.c
├── components/
│   ├── friend_core/
│   │   ├── identity
│   │   ├── state
│   │   └── personality
│   ├── friend_display/
│   │   ├── display_driver
│   │   ├── moc_face
│   │   └── emotion_renderer
│   ├── friend_audio/
│   │   ├── microphone
│   │   └── speaker
│   ├── friend_link/
│   │   ├── discovery
│   │   ├── capability
│   │   ├── rpc
│   │   ├── event
│   │   └── presence
│   ├── friend_ai/
│   │   ├── provider
│   │   ├── openai
│   │   └── xiaozhi
│   └── boards/
│       ├── zhengchen_1_54/
│       ├── xiao_s3_sense/
│       └── cyd/
└── sdkconfig
```

Exact structure có thể thay đổi. Quy tắc quan trọng: **board-specific
code không được lan khắp FriendOS**.

Board abstraction dự kiến:

``` c
board_display_init();
board_audio_init();
board_buttons_init();
board_battery_read();
```

Các pin Zhengchen và quirks như Octal PSRAM/LCD inversion nên nằm trong
board definition.

------------------------------------------------------------------------

## 16. Immediate Next Task --- Buttons

**CURRENT STOPPING POINT: Display, LVGL và Mộc Face UI đã hoạt động ổn
định.**

**NEXT TASK: Validate 3 physical buttons.**

Chưa tích hợp AI.

Các pin cần kiểm tra thực nghiệm:

``` c
#define BTN_BOOT     0
#define BTN_VOL_UP   10
#define BTN_VOL_DOWN 39
```

Milestone:

1.  Khởi tạo GPIO input.
2.  Xác định active HIGH/LOW.
3.  Xác định pull-up/pull-down phù hợp.
4.  Nhấn từng nút và xác nhận log trên Serial Monitor.
5.  Kiểm tra debounce.
6.  Chỉ sau khi PASS mới gán chức năng volume / wake / push-to-talk.

Không nối button trực tiếp vào logic AI ở bước bring-up này.

------------------------------------------------------------------------

## 17. Mộc Face UI --- Current Validated State

Mộc Face UI đã được tích hợp bằng LVGL và chạy ổn định trên màn 240×240.

UI vẫn độc lập với OpenAI/Xiaozhi. Application gọi API abstraction:

``` c
friend_ui_set_emotion(FRIEND_EMOTION_HAPPY);
```

Các biểu cảm đã được thử nghiệm trong quá trình phát triển gồm:

``` text
NEUTRAL
HAPPY
SLEEPY
```

Đã thử các phiên bản UI mở rộng hơn với nhiều emotion/action, nhưng
phiên bản emoji cũ được chọn giữ lại vì hình ảnh ổn định và phù hợp hơn.
Không tiếp tục mở rộng animation trước khi các hardware layer tiếp theo
được bring-up.

Đã xác nhận:

-   khuôn mặt render đúng;
-   natural blink hoạt động;
-   blink không gây lag/tearing thấy rõ;
-   emotion switching hoạt động;
-   UI dùng LVGL partial double buffer;
-   RGB565 cần byte swap trước khi gửi LCD;
-   `lv_display_flush_ready()` chỉ được gọi sau ESP-LCD DMA completion
    callback.

Các emotion/action nâng cao có thể bổ sung sau khi audio + AI pipeline
hoạt động.

------------------------------------------------------------------------

## 18. Recommended Hardware Bring-up Order

Sau display:

1.  LVGL
2.  Buttons
3.  Microphone
4.  Speaker
5.  Battery ADC
6.  Wi-Fi
7.  Basic audio pipeline
8.  AI
9.  FriendLink
10. Multi-board demo

Mục tiêu là validate từng hardware layer độc lập trước khi networking/AI
làm debugging phức tạp.

### Buttons

Test GPIO0, GPIO10, GPIO39. Xác định thực nghiệm active HIGH/LOW,
pull-up và debounce.

### Microphone

Expected pins: WS=4, SCK=5, SD=6.

Milestones:

1.  I2S init
2.  capture PCM
3.  audio level meter

Không bắt đầu STT trước khi raw microphone capture pass.

### Speaker

Expected pins: DOUT=7, BCLK=15, LRCK=16.

Milestones:

1.  simple tone / PCM waveform
2.  stored PCM playback
3.  TTS later

### Battery

Expected GPIO8 ADC.

Cần xác định ADC channel, divider ratio, calibration, battery voltage
conversion và percentage curve. Không giả định ADC value = battery
voltage.

------------------------------------------------------------------------

## 19. AI Integration Order

Không xây tất cả cùng lúc.

``` text
Stage 1:
Text request → LLM → text response

Stage 2:
Microphone → STT → text

Stage 3:
LLM response → TTS → speaker

Stage 4:
Emotion/state → MocFace
```

Final:

``` text
Mic
 ↓
STT
 ↓
AI
 ├── response text
 ├── emotion
 └── tool request
 ↓
TTS + Display + FriendLink
```

------------------------------------------------------------------------

## 20. FriendLink Development Order

Sau khi một Friend hoạt động local:

1.  Two ESP32 devices discover each other
2.  HELLO + Identity
3.  Presence
4.  Capability advertisement
5.  RPC

Ví dụ:

``` text
Zhengchen: camera.capture?
XIAO:      camera.capture available
Zhengchen: RPC camera.capture
XIAO:      result
```

Chỉ sau khi flow này chạy mới xem xét large image/audio transfer.

------------------------------------------------------------------------

## 21. First Multi-Board Demo Goal

Roles:

``` text
Zhengchen       Mộc / Voice Friend
XIAO S3 Sense   Vision Node
CYD             Network Dashboard
```

Scenario:

1.  Mộc discovers XIAO.
2.  XIAO advertises `camera.capture`.
3.  User asks Mộc to look somewhere.
4.  Mộc invokes `camera.capture`.
5.  XIAO captures image.
6.  AI analyzes image.
7.  Mộc speaks answer.
8.  Mộc displays appropriate emotion.
9.  CYD shows both nodes and their state.

Demo này chứng minh kiến trúc chính của FriendOS.

------------------------------------------------------------------------

## 22. Things NOT To Do Yet

Không vội:

-   build mesh networking;
-   implement complex OTA;
-   dùng MQTT làm core protocol;
-   tích hợp nhiều AI providers cùng lúc;
-   tạo UI framework quá lớn;
-   tăng PSRAM lên 80/120 MHz khi chưa cần;
-   viết toàn bộ FriendLink cùng lúc;
-   tiếp tục nhét mọi thứ vào `main.c`.

Bring-up phải incremental và test từng layer.

------------------------------------------------------------------------

## 23. Important Lessons From Bring-up

1.  Không tin seller specs hoàn toàn; Flash và PSRAM phải verify trực
    tiếp.
2.  8 MB PSRAM không có nghĩa có thể đoán Quad/Octal. Quad đã fail;
    Octal được runtime xác nhận.
3.  Boot loop không có nghĩa firmware cũ còn chạy. Lần này nguyên nhân
    là PSRAM init failure.
4.  LCD color issue phải isolate có hệ thống. RGB→CYM cuối cùng được xác
    định là color inversion.
5.  Không thay nhiều LCD parameters cùng lúc.
6.  Hardware bring-up trước AI integration.

------------------------------------------------------------------------

## 24. Known-Good Commands

``` powershell
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash
idf.py -p COM8 monitor
idf.py -p COM8 flash monitor
idf.py menuconfig
```

Chỉ dùng khi thực sự cần:

``` powershell
idf.py fullclean
```

### erase-flash warning

Không casually chạy:

``` powershell
idf.py erase-flash
```

Original firmware có thể mất nếu chưa backup. Normal FriendOS update
không cần full erase.

------------------------------------------------------------------------

## 25. Configuration Durability --- TODO

Nên tạo `sdkconfig.defaults` để project không phụ thuộc duy nhất vào
machine-local `sdkconfig`.

Ít nhất phải đảm bảo configuration có thể tái tạo:

-   ESP32-S3 target;
-   Flash 16 MB;
-   Octal PSRAM;
-   PSRAM startup initialization;
-   các config cần thiết khác khi project trưởng thành.

Mục tiêu: người mới clone project không vô tình quay lại Quad PSRAM và
gặp reboot loop.

------------------------------------------------------------------------

## 26. Definition of Success --- FriendOS v1

``` text
[ ] Mộc boots reliably
[ ] Animated face/UI
[ ] Microphone capture
[ ] Speaker playback
[ ] Voice conversation with AI
[ ] Emotion-driven UI
[ ] Friend identity
[ ] Friend discovery
[ ] Capability discovery
[ ] FriendLink RPC
[ ] Zhengchen communicates with XIAO
[ ] Camera becomes an AI-callable tool
[ ] CYD can observe/control Friend network
```

Khi đạt các mục trên, project đã chuyển từ "ESP32 AI chatbot" thành một
**multi-device embodied AI Friend platform**.

------------------------------------------------------------------------

# CURRENT CHECKPOINT --- READ THIS FIRST

Nếu một developer hoặc AI khác tiếp tục project, bắt đầu tại đây.

**Verified state:**

``` text
ESP-IDF      5.5.5
Target       ESP32-S3
Flash        16 MB
PSRAM        8 MB Octal @ 40 MHz tested
LCD          ST7789 240×240 SPI RGB565
LCD invert   ON / REQUIRED
```

**LCD pins:**

``` text
MOSI 41
SCLK 42
CS   21
DC   40
RST  45
BL   20
```

LCD color + geometry tests đã PASS trên phần cứng thật.

LVGL 9 integration cũng đã PASS. Mộc Face UI đã chạy ổn định với natural
blink và emotion switching. Display bridge hiện dùng partial double
buffer, RGB565 byte swap và ESP-LCD DMA completion callback trước khi
báo `lv_display_flush_ready()`.

**Do not redo basic hardware/LCD/LVGL discovery unless hardware or
configuration changes.**

## NEXT TASK

**Validate physical buttons: GPIO0, GPIO10, GPIO39.**

Thứ tự tiếp theo:

1.  Buttons
2.  Microphone
3.  Speaker
4.  Battery
5.  Wi-Fi
6.  Basic audio pipeline
7.  AI
8.  FriendLink
9.  Multi-board demo

**Do not jump directly to OpenAI/FriendLink before the underlying
hardware layers are validated.**

------------------------------------------------------------------------

# 27. UPDATE --- 2026-10-01 CURRENT AUTHORITATIVE CHECKPOINT

> **This section supersedes older `Current Bring-up Status`, `Immediate Next Task`, and `CURRENT CHECKPOINT` sections above where they conflict.** Older sections are retained as development history.

## 27.1 Hardware / board status

``` text
ESP32-S3                         PASS
Flash 16 MB                      PASS
PSRAM 8 MB Octal @ 40 MHz        PASS
ST7789 240×240                   PASS
LVGL 9                           PASS
Buttons GPIO0/10/39              PASS
Microphone I2S                   PASS
Speaker hardware/source pinout   KNOWN; FriendOS TX driver deferred
Battery ADC                      DEFERRED / NOT VALIDATED
```

Confirmed buttons are active LOW. FriendOS uses `espressif/button` 4.2.1. BOOT has normal long-press handling plus a dedicated ~5000 ms hold event used to enter Manual Wi-Fi Setup. Holding BOOT does **not** erase saved Wi-Fi networks.

Confirmed microphone configuration:

``` c
#define MIC_WS   4
#define MIC_SCK  5
#define MIC_SD   6
```

I2S RX was validated at 16 kHz, 32-bit mono Philips, left slot. Raw PCM responds correctly to sound.

Speaker pinout confirmed from the Zhengchen/Xiaozhi board source:

``` c
#define SPK_DOUT 7
#define SPK_BCLK 15
#define SPK_LRCK 16
```

FriendOS speaker TX driver is intentionally deferred.

## 27.2 Current source structure

``` text
main/
├── CMakeLists.txt
├── main.c
├── board/
│   ├── board_config.h
│   ├── board_display.h
│   ├── board_display.c
│   ├── board_buttons.h
│   ├── board_buttons.c
│   ├── board_audio.h
│   └── board_audio.c
├── ui/
│   ├── friend_ui.h
│   └── friend_ui.c
├── network/
│   ├── friend_wifi.h
│   ├── friend_wifi.c
│   ├── friend_http.h
│   ├── friend_http.c
│   ├── friend_wifi_store.h
│   └── friend_wifi_store.c
└── web/
    ├── index.html
    ├── style.css
    └── app.js
```

Responsibilities:

- `board_config.h`: board-specific pins/config only.
- `board_display.c`: SPI/ST7789/backlight/LVGL display bridge.
- `board_buttons.c`: physical buttons via `espressif/button`.
- `board_audio.c`: microphone bring-up.
- `friend_ui.c`: Mộc face/emotion/action UI.
- `friend_wifi.c`: Wi-Fi driver + Wi-Fi manager/state machine.
- `friend_wifi_store.c`: FriendOS-owned persistent multi-network credentials in NVS.
- `friend_http.c`: Manual Setup HTTP server/API.
- `web/*`: embedded provisioning frontend.
- `main.c`: orchestration only.

Do not over-abstract into separate ESP-IDF components yet.

## 27.3 Partition table

Project now uses ESP-IDF preset **Two large size OTA partitions**:

``` text
nvs       data nvs    0x9000    24K
otadata   data ota    0xf000     8K
phy_init  data phy    0x11000    4K
ota_0     app ota_0  0x20000    0x1a9000
ota_1     app ota_1  0x1d0000   0x1a9000
```

Board has 16 MB flash, so significant flash remains unused by this preset. A custom partition table may later allocate larger OTA/filesystem/assets, but **do not change it during the current Wi-Fi work**.

## 27.4 Saved Wi-Fi store

FriendOS now owns Wi-Fi persistence; Wi-Fi driver storage is RAM-only. Saved networks are stored in NVS namespace `friend_wifi`.

Current store supports up to 8 networks:

``` c
#define FRIEND_WIFI_MAX_SAVED_NETWORKS 8
#define FRIEND_WIFI_SSID_MAX_LEN       32
#define FRIEND_WIFI_PASSWORD_MAX_LEN   64
```

Public operations include save, get by SSID, forget, get all, is-saved, count, and clear. Legacy single-network `ssid`/`password` keys are migrated to the indexed format on initialization.

Important: do not place an array of 8 `friend_wifi_saved_network_t` structures on `app_main` stack. This previously contributed to a main-task stack overflow.

## 27.5 Wi-Fi manager --- validated normal mode

Normal behavior is now:

``` text
BOOT
 ↓
scan visible Wi-Fi
 ↓
find visible saved SSIDs
 ↓
choose strongest saved SSID
 ↓
connect
 ↓
ONLINE
```

If no saved network is visible, FriendOS stays OFFLINE and scans again every 5 seconds. It does **not** automatically open provisioning just because the router disappears.

When ONLINE, FriendOS does not periodically scan/roam. It waits for disconnect. On disconnect it returns to OFFLINE scanning and reconnects when a saved network becomes visible again.

Validated:

``` text
Normal boot scan                 PASS
Saved SSID matching              PASS
Auto connect                     PASS
GOT_IP                           PASS
ONLINE no periodic scan          PASS
Runtime disconnect detection     PASS
OFFLINE scan every 5 s           PASS
Failed connection recovery       PASS
Network returns -> reconnect     PASS
No reconnect collision           PASS
```

For same-SSID multiple BSSIDs, STA configuration uses `WIFI_ALL_CHANNEL_SCAN` and `WIFI_CONNECT_AP_BY_SIGNAL`.

## 27.6 Manual Wi-Fi Setup --- validated infrastructure

Holding BOOT for about 5 seconds requests Manual Setup.

Validated flow:

``` text
BOOT held 5 s
 ↓
FRIEND_WIFI_STATE_MANUAL
 ↓
Wi-Fi APSTA
 ↓
SoftAP: Moc-Setup
 ↓
AP IP: 192.168.4.1
 ↓
HTTP provisioning server
 ↓
embedded HTML/CSS/JS
 ↓
manual Wi-Fi scan
```

Validated:

``` text
BOOT long press 5 s              PASS
Enter MANUAL state               PASS
STA -> APSTA                      PASS
Moc-Setup SoftAP                 PASS
DHCP 192.168.4.1                 PASS
Phone joins AP                   PASS
HTTP provisioning                PASS
Embedded HTML/CSS/JS             PASS
Manual Wi-Fi scan                PASS
```

Current provisioning frontend/API is **not finished**. Specifically, it still does not expose whether a scanned SSID is saved, and clicking an already-saved network still asks the browser for a password. This is the immediate next task.

Required final Manual Setup behavior:

``` text
Saved SSID
  -> display as "Đã lưu"
  -> user taps it
  -> ESP32 retrieves password from NVS server-side
  -> connect without exposing/requesting password in browser

Unsaved SSID
  -> user taps it
  -> browser asks password
  -> connect
  -> only after GOT_IP save credential to NVS

Saved SSID
  -> provide Forget/Delete action
  -> remove credential from NVS
```

Passwords stored in NVS must never be returned to HTML/JavaScript/API responses.

A current edge case is also confirmed: if Manual Setup is entered while STA is already connected to the same SSID, the old HTTP connect flow calls `esp_wifi_connect()` again and ESP-IDF logs that STA is already connected. The new Manual Setup flow must handle the already-connected SSID cleanly instead of treating it as bad credentials.

## 27.7 HTTP provisioning implementation notes

Current HTTP server uses official ESP-IDF `esp_http_server` with an 8192-byte server stack.

Embedded files use `EMBED_TXTFILES`. ESP-IDF appends a trailing NUL; responses must exclude that trailing `\0` or JavaScript may fail to execute.

POST request bodies must be received in a loop until the complete body is read.

`/favicon.ico` 404 is harmless. `httpd_sock_err: error in recv : 104` can occur when the client closes/resets the socket and is not itself a firmware crash if the server continues running.

Existing API foundation:

``` text
GET  /
GET  /style.css
GET  /app.js
GET  /api/wifi/scan
POST /api/wifi/connect
```

Next API work should add saved-state information to scan results and a Forget/Delete endpoint. SSIDs must be JSON-escaped correctly.

## 27.8 Main-task stack overflow --- fixed architecture

A real crash was observed:

``` text
***ERROR*** A stack overflow in task main has been detected.
```

The old architecture kept `app_main()` alive forever and serviced `lv_timer_handler()` from the ESP-IDF main task. FriendOS now starts a dedicated UI task and allows `app_main()` to return.

Current design:

``` text
app_main
  -> initialize display/UI/buttons/audio/Wi-Fi
  -> start Wi-Fi manager
  -> start dedicated friend_ui task
  -> return

friend_ui task
  -> lv_timer_handler()
  -> delay
```

Runtime validation after the change:

``` text
Main task minimum free stack before UI task: 1524 bytes
main_task: Returned from app_main()
UI task stack size: 8192 bytes
UI task minimum free stack: ~4264 bytes after >200 s
```

The device remained stable through normal Wi-Fi connection, BOOT 5-second Manual Setup, SoftAP, HTTP requests, Wi-Fi scans, and repeated UI processing. No reboot occurred during the >200-second validation run.

**Checkpoint: main-task stack overflow / reboot FIXED.**

Important LVGL rule going forward: LVGL is not to be called directly from arbitrary Wi-Fi/AI tasks. Future cross-task UI changes should be dispatched to the dedicated UI context/queue.

## 27.9 Current `main.c` lifecycle

`app_main()` is now initialization/orchestration only. It creates a dedicated `friend_ui` task (8192-byte stack) that periodically calls `lv_timer_handler()`, then `app_main()` returns.

Do not reintroduce an infinite LVGL loop into `app_main()`.

## 27.10 Development rules established during bring-up

Before implementing a new subsystem/hardware feature:

1. Check official **ESP-IoT-Solution** documentation first.
2. If it does not cover the required detail, use official **ESP-IDF / Espressif** documentation/source.
3. Prefer established Espressif/expert solutions for generic subsystem problems before inventing a custom implementation.
4. Custom-design only the policy/behavior specific to FriendOS/Mộc.
5. Develop incrementally: one change -> test -> checkpoint -> continue.

Code style currently expected:

- compact C style;
- function signatures/calls stay on one line when practical;
- assignments stay on one line;
- braces on their own lines;
- one blank line after a closing brace before the next logical block;
- avoid vertically splitting arguments unless necessary.

## 27.11 Git state

Repository:

``` text
H:\HISU\Esp32Project-AI\FriendOS
branch: main
remote: private GitHub repository
```

`build/`, `managed_components/`, `sdkconfig.old`, `.vscode/`, etc. are ignored. `sdkconfig` is intentionally tracked.

**Do not commit the current Wi-Fi/manual-setup work yet.** Wait until the complete new Wi-Fi manager + Manual Setup saved-network flow passes.

## 27.12 Immediate next task

Do **not** redo display, buttons, microphone, normal Wi-Fi manager, or stack debugging unless a new regression appears.

Current next task:

``` text
Manual Wi-Fi Setup UX/API
  1. /api/wifi/scan marks saved networks
  2. frontend displays "Đã lưu"
  3. tapping saved SSID connects with NVS password
  4. unsaved SSID asks for password
  5. successful unsaved connection saves after GOT_IP
  6. Forget/Delete saved SSID
  7. handle already-connected SSID
  8. cleanly leave MANUAL mode after successful connection
```

After this passes, make a checkpoint before moving to the next FriendOS subsystem.

## 28. Persistent display media (2026-10-02)

The last uploaded RGB565 still image or GIF is kept in the media partition; whether the display currently shows that media or the Moc face is kept separately in NVS (`friend_media/show_image`). A short BOOT click switches between them without rewriting the image. If no saved image exists, a brief notice appears on the TFT. The 5-second BOOT hold still enters Manual Wi-Fi Setup. GIF playback never writes flash. The media store uses two alternating slots with payload and header checksums, so an interrupted media write can fall back to the older valid image. On boot, unavailable saved media falls back to the Moc face.

`partitions.csv` preserves the prior NVS and two OTA offsets/sizes and adds a 0x220000-byte `media` data partition at 0x380000 on the 16 MB flash. The first installation of this feature must flash the partition table as well as the app (`idf.py flash`), not only `FriendOS.bin` or an OTA app update. Existing Wi-Fi credentials remain in the unchanged NVS partition. The old note in section 16 about *not* changing the partition table applied to the earlier Wi-Fi work and is now superseded.

Source: `main/ui/friend_media_store.c`; boot restore: `main/main.c`; button/UI switching: `main/board/board_buttons.c` and `main/ui/friend_media.c`; web save/selection: `main/network/friend_http.c`. Hardware button and power-cycle validation is still needed.

