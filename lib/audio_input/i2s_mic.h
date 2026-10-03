// =============================================================================
// i2s_mic.h - Module thu âm từ Microphone INMP441 qua I2S + DMA
//
// Viết theo phong cách C cơ bản:
//  - Không dùng class, không dùng kế thừa giả lập.
//  - Chức năng: Khởi tạo I2S/DMA, đọc mẫu âm thanh 16kHz đưa vào Ring Buffer.
// =============================================================================

#ifndef I2S_MIC_H
#define I2S_MIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ring_buffer.h"

// Cấu hình chân kết nối INMP441
typedef struct {
    int bck_pin;   // Chân SCK (Bit Clock)
    int ws_pin;    // Chân WS  (Word Select / L-R Clock)
    int sd_pin;    // Chân SD  (Serial Data IN)
} I2SMicPins;

// Khởi tạo I2S driver, cấp phát bộ nhớ Ring Buffer và tạo Task đọc I2S
// Tham số:
//   pins: Cấu hình chân nối mic
//   ai_task_to_notify: Handle của Task AI (để bắn xTaskNotify khi đủ 1 trang)
bool i2s_mic_init(const I2SMicPins *pins, TaskHandle_t ai_task_to_notify);

// Lấy con trỏ đọc từ Ring Buffer để Task AI tua ngược lại 1 giây âm thanh
RingBufferAccessor i2s_mic_get_reader(void);

#ifdef __cplusplus
}
#endif

#endif // I2S_MIC_H
