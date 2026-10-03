# Xe Robot Điều Khiển Bằng Giọng Nói (ESP32-S3 + INMP441)

Firmware nhận diện giọng nói thời gian thực (Keyword Spotting) sử dụng mô hình TensorFlow Lite Micro trên vi điều khiển **ESP32-S3**.

---

## 1. Sơ đồ nối dây phần cứng (Hardware Pinout)

Tất cả cấu hình chân được định nghĩa tập trung trong file `src/config.h`:

| Thiết bị | Chân thiết bị | Chân ESP32-S3 | Ghi chú |
| :--- | :--- | :--- | :--- |
| **Microphone INMP441** | `SCK` | **GPIO 14** | Bit Clock (I2S BCLK) |
| | `WS` | **GPIO 15** | Word Select (I2S LRCK) |
| | `SD` | **GPIO 16** | Serial Data IN (I2S DATA) |
| | `L/R` | **GND** | Chọn kênh Trái (Left Channel) |
| | `VDD` / `GND` | **3.3V / GND** | Nguồn nuôi mic |
| **Servo Bánh Trái** | Signal | **GPIO 13** | PWM 50Hz (LEDC Channel 0) |
| **Servo Bánh Phải** | Signal | **GPIO 12** | PWM 50Hz (LEDC Channel 1) |
| **LED Chỉ thị** | Anode (+) | **GPIO 2** | Sáng khi xe đang thực thi lệnh |

---

## 2. Kiến trúc Firmware (3 FreeRTOS Tasks)

1. **Task 1: `I2SReader` (Core 0)**
   * Đọc dữ liệu từ DMA I2S (4 bộ đệm $\times$ 64 mẫu) khi có ngắt phần cứng.
   * Chuyển đổi dữ liệu 32-bit thành 16-bit và nạp vào **Ring Buffer** (11 trang $\times$ 1600 mẫu, cấp phát tĩnh trong BSS).
   * Mỗi 1600 mẫu (0.1 giây), gửi thông báo `xTaskNotify` kích hoạt Task 2.

2. **Task 2: `VoiceAI` (Core 0)**
   * Nhận tín hiệu `ulTaskNotifyTake`, tua lại 16.000 mẫu (1 giây âm thanh gần nhất).
   * Chạy FFT (KissFFT) kết hợp cửa sổ Hamming để tạo ảnh phổ Spectrogram 2D.
   * Đưa ảnh phổ vào mạng nơ-ron **TensorFlow Lite Micro** (`ai_classifier`).
   * Nếu xác suất vượt ngưỡng tin cậy, gửi mã lệnh sang `cmd_processor` qua **FreeRTOS Queue**.

3. **Task 3: `CmdQueue` (Core 1)**
   * Chờ lệnh từ Queue bằng `xQueueReceive`.
   * Nhận lệnh $\rightarrow$ Bật LED $\rightarrow$ Xuất xung PWM 50Hz điều khiển 2 Servo:
     * `forward`: Tiến thẳng 1000ms.
     * `backward`: Lùi 1000ms.
     * `left`: Rẽ trái 500ms.
     * `right`: Rẽ phải 500ms.
   * Hết thời gian thì dừng servo và tắt LED.

---

## 3. Cấu trúc thư mục mã nguồn

```text
├── src/
│   ├── config.h               # Định nghĩa toàn bộ chân GPIO phần cứng
│   ├── main.cpp               # Điểm khởi đầu, khởi tạo FreeRTOS tasks
│   ├── command_detector.c/.h  # Pipeline tạo Spectrogram và lọc xác suất AI
│   └── command_processor.c/.h # Task điều khiển Servo qua PWM và Queue
├── lib/
│   ├── audio_input/           # Module thu âm i2s_mic.c/.h và ring_buffer.h
│   ├── audio_processor/       # Thuật toán FFT (KissFFT) và Hamming window
│   ├── neural_network/        # Bộ suy luận AI (ai_classifier.cpp/.h, model.cc)
│   └── tfmicro/               # Thư viện lõi TensorFlow Lite Micro
└── platformio.ini             # Cấu hình biên dịch PlatformIO
```
