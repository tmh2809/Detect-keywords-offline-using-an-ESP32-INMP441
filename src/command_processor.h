// =============================================================================
// command_processor.h - Điều khiển xe theo lệnh giọng nói
//
// Nhiệm vụ:
//  - Nhận mã lệnh (0: tiến, 1: lùi, 2: trái, 3: phải) qua FreeRTOS Queue
//  - Xuất xung PWM 50Hz điều khiển 2 servo motor
//  - Dùng Task riêng để chạy servo (vì có delay 500-1000ms)
// =============================================================================

#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Khởi tạo module: cấu hình GPIO, PWM, tạo hàng đợi và task điều khiển
void cmd_processor_init(void);

// Gửi lệnh vào hàng đợi (gọi từ command_detector khi nhận diện được lệnh)
void cmd_processor_send(uint16_t cmd_index, float score);

#ifdef __cplusplus
}
#endif

#endif // COMMAND_PROCESSOR_H
