// =============================================================================
// command_processor.c - Điều khiển servo theo lệnh giọng nói
// =============================================================================

#include <stdio.h>
#include <esp32-hal-ledc.h>
#include <esp32-hal-gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "command_processor.h"
#include "config.h"

// Tên các lệnh (dùng để in log)
static const char *COMMAND_NAMES[] = {
    "forward",   // 0 - tiến
    "backward",  // 1 - lùi
    "left",      // 2 - trái
    "right",     // 3 - phải
    "_nonsense", // 4 - không phải lệnh hợp lệ
};

// --- Cấu hình kênh PWM cho LEDC ---
#define PWM_CHANNEL_LEFT  0
#define PWM_CHANNEL_RIGHT 1
#define PWM_FREQ_HZ       50    // 50Hz = chu kỳ 20ms (chuẩn servo)
#define PWM_RESOLUTION    14    // 14-bit resolution

// --- Thời gian xung servo (micro-giây) ---
// 1500µs = dừng, >1500 = quay tiến, <1500 = quay lùi
#define LEFT_FORWARD   1600
#define LEFT_BACKWARD  1400
#define LEFT_STOP      1500
#define RIGHT_BACKWARD 1600
#define RIGHT_FORWARD  1445
#define RIGHT_STOP     1500

// Biến toàn cục nội bộ module
static QueueHandle_t g_cmd_queue = NULL;

// Chuyển thời gian xung (µs) sang giá trị duty cho thanh ghi LEDC
static int calc_duty(int pulse_us) {
    return ((1UL << PWM_RESOLUTION) * pulse_us) / 20000;
}

// =============================================================================
// Thực thi lệnh: xuất xung PWM điều khiển 2 servo
// =============================================================================
static void execute_command(uint16_t cmd_index)
{
    digitalWrite(PIN_LED, HIGH);  // Bật LED báo đang chạy

    switch (cmd_index) {
        case 0:  // tiến thẳng
            ledcWrite(PWM_CHANNEL_LEFT,  calc_duty(LEFT_FORWARD));
            ledcWrite(PWM_CHANNEL_RIGHT, calc_duty(RIGHT_FORWARD));
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;

        case 1:  // lùi
            ledcWrite(PWM_CHANNEL_LEFT,  calc_duty(LEFT_BACKWARD));
            ledcWrite(PWM_CHANNEL_RIGHT, calc_duty(RIGHT_BACKWARD));
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;

        case 2:  // rẽ trái
            ledcWrite(PWM_CHANNEL_LEFT,  calc_duty(LEFT_BACKWARD));
            ledcWrite(PWM_CHANNEL_RIGHT, calc_duty(RIGHT_FORWARD));
            vTaskDelay(pdMS_TO_TICKS(500));
            break;

        case 3:  // rẽ phải
            ledcWrite(PWM_CHANNEL_LEFT,  calc_duty(LEFT_FORWARD));
            ledcWrite(PWM_CHANNEL_RIGHT, calc_duty(RIGHT_BACKWARD));
            vTaskDelay(pdMS_TO_TICKS(500));
            break;

        default:
            break;
    }

    digitalWrite(PIN_LED, LOW);  // Tắt LED

    // Dừng cả hai servo
    ledcWrite(PWM_CHANNEL_LEFT,  calc_duty(LEFT_STOP));
    ledcWrite(PWM_CHANNEL_RIGHT, calc_duty(RIGHT_STOP));
}

// =============================================================================
// Task chờ lệnh từ hàng đợi (chạy ngầm)
// =============================================================================
static void cmd_queue_task(void *param)
{
    (void)param;
    while (1) {
        uint16_t cmd_index = 0;
        if (xQueueReceive(g_cmd_queue, &cmd_index, portMAX_DELAY) == pdTRUE) {
            execute_command(cmd_index);
        }
    }
}

// =============================================================================
// Khởi tạo module
// =============================================================================
void cmd_processor_init(void)
{
    // Cấu hình LED
    pinMode(PIN_LED, OUTPUT);

    // Cấu hình PWM cho servo trái
    ledcSetup(PWM_CHANNEL_LEFT, PWM_FREQ_HZ, PWM_RESOLUTION);
    ledcAttachPin(PIN_MOTOR_LEFT, PWM_CHANNEL_LEFT);
    ledcWrite(PWM_CHANNEL_LEFT, calc_duty(LEFT_STOP));

    // Cấu hình PWM cho servo phải
    ledcSetup(PWM_CHANNEL_RIGHT, PWM_FREQ_HZ, PWM_RESOLUTION);
    ledcAttachPin(PIN_MOTOR_RIGHT, PWM_CHANNEL_RIGHT);
    ledcWrite(PWM_CHANNEL_RIGHT, calc_duty(RIGHT_STOP));

    // Tạo hàng đợi chứa tối đa 5 lệnh
    g_cmd_queue = xQueueCreate(5, sizeof(uint16_t));

    // Tạo task xử lý lệnh
    TaskHandle_t task_handle;
    xTaskCreate(cmd_queue_task, "CmdQueue", 1024, NULL, 1, &task_handle);

    printf("Command Processor da san sang.\n");
}

// =============================================================================
// Gửi lệnh vào hàng đợi
// =============================================================================
void cmd_processor_send(uint16_t cmd_index, float score)
{
    if (cmd_index >= 4) return;

    printf(">>> Nhan lenh: %s (score=%.2f)\n", COMMAND_NAMES[cmd_index], score);

    if (xQueueSendToBack(g_cmd_queue, &cmd_index, 0) != pdTRUE) {
        printf("Hang doi lenh da day, bo qua.\n");
    }
}
