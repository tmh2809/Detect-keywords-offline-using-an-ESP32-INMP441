// =============================================================================
// command_detector.h - Pipeline phát hiện giọng nói
// =============================================================================

#ifndef COMMAND_DETECTOR_H
#define COMMAND_DETECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// Khởi tạo detector (chuẩn bị FFT và AI)
bool cmd_detector_init(void);

// Chạy một chu kỳ phát hiện
void cmd_detector_run(void);

#ifdef __cplusplus
}
#endif

#endif // COMMAND_DETECTOR_H
