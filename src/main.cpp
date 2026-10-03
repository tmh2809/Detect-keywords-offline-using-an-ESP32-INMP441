#include <Arduino.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "i2s_mic.h"
#include "command_detector.h"
#include "command_processor.h"

static TaskHandle_t g_ai_task_handle = NULL;

// =============================================================================
// Task AI: Chạy ngầm trên Core 0
//
// Nhiệm vụ:
//  - Chờ tín hiệu xTaskNotify từ module I2S Mic (mỗi 0.1s khi đầy 1 trang)
//  - Thức dậy và chạy pipeline nhận diện giọng nói
// =============================================================================
static void voice_ai_task(void *param)
{
    (void)param;
    const TickType_t max_wait = pdMS_TO_TICKS(100);

    while (1) {
        // Chờ thông báo từ i2s_mic (khi thu đủ 1600 mẫu mới)
        uint32_t notify_val = ulTaskNotifyTake(pdTRUE, max_wait);
        if (notify_val > 0) {
            // Có dữ liệu mới -> chạy nhận diện
            cmd_detector_run();
        }
    }
}

// =============================================================================
// setup: Chạy một lần khi khởi động ESP32-S3
// =============================================================================
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== XE ROBOT DIEU KHIEN GIONG NOI (ESP32-S3) ===");

    // Tăng thời gian Watchdog Timer lên 10s (tránh reset khi AI tính toán lâu)
    esp_task_wdt_init(10, false);

    // 1. Khởi tạo bộ điều khiển động cơ servo (tạo Task xử lý PWM)
    cmd_processor_init();

    // 2. Khởi tạo bộ nhận diện lệnh (chuẩn bị FFT và mô hình AI)
    cmd_detector_init();

    // 3. Tạo Task AI chạy trên Core 0
    xTaskCreatePinnedToCore(
        voice_ai_task,      // Hàm thực thi task
        "VoiceAI",          // Tên task (dùng debug)
        8192,               // Kích thước Stack (8KB cho FFT)
        NULL,               // Tham số truyền vào
        1,                  // Mức ưu tiên (Priority 1)
        &g_ai_task_handle,  // Lưu handle của task
        0                   // Ghim vào Core 0
    );

    // 4. Khởi tạo module mic I2S INMP441 (truyền handle Task AI để nó gửi thông báo)
    I2SMicPins mic_pins = {
        .bck_pin = I2S_MIC_SERIAL_CLOCK,
        .ws_pin  = I2S_MIC_LEFT_RIGHT_CLOCK,
        .sd_pin  = I2S_MIC_SERIAL_DATA,
    };
    i2s_mic_init(&mic_pins, g_ai_task_handle);

    Serial.println("He thong da san sang! Hay noi: forward / backward / left / right");
}

// =============================================================================
// loop: Nhường CPU cho các task FreeRTOS hoạt động
// =============================================================================
void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}