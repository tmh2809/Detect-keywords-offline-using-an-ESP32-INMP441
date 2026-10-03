// =============================================================================
// ai_classifier.h - Module nhận diện lệnh giọng nói bằng TensorFlow Lite Micro
//
// Viết theo phong cách C thuần:
//  - Che giấu toàn bộ thư viện TFLite C++ phức tạp bên trong file .cpp.
//  - Bên ngoài chỉ cần gọi hàm C đơn giản, không cần biết con trỏ mờ hay class.
// =============================================================================

#ifndef AI_CLASSIFIER_H
#define AI_CLASSIFIER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#define AI_NUM_COMMANDS 5 // forward, backward, left, right, _nonsense

// Khởi tạo mô hình AI (load model từ Flash vào RAM, chuẩn bị bộ nhớ)
bool ai_init(void);

// Lấy con trỏ đến bộ đệm đầu vào (để ghi ảnh phổ Spectrogram vào)
float *ai_get_input_buffer(void);

// Chạy suy luận mạng nơ-ron
// Tham số:
//   output_scores: mảng float có 5 phần tử để nhận xác suất từng lệnh
void ai_predict(float *output_scores);

#ifdef __cplusplus
}
#endif

#endif // AI_CLASSIFIER_H
