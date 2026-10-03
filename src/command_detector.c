// =============================================================================
// command_detector.c - Triển khai pipeline phát hiện lệnh giọng nói
// =============================================================================

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <esp_timer.h>
#include "command_detector.h"
#include "command_processor.h"
#include "i2s_mic.h"
#include "ai_classifier.h"
#include "audio_processor.h"

// Tham số xử lý âm thanh
#define AUDIO_LENGTH        16000   // 1 giây âm thanh @ 16kHz
#define WINDOW_SIZE         320     // Cửa sổ 20ms
#define STEP_SIZE           160     // Bước nhảy 10ms (50% overlap)
#define POOLING_SIZE        6       // Gom 6 bin tần số

#define COMMAND_WINDOW      3       // Số lần chạy gần nhất để cộng dồn điểm xác suất
#define DETECTION_THRESHOLD -3.0f   // Ngưỡng phát hiện lệnh (log scale)

// Lấy thời gian hiện tại theo mili-giây
static inline long get_millis(void) {
    return (long)(esp_timer_get_time() / 1000);
}

// Biến quản lý nội bộ module
static AudioProcessor    g_audio_processor;
static float             g_scores[COMMAND_WINDOW][AI_NUM_COMMANDS];
static int               g_score_index = 0;
static long              g_last_detect_ms = 0;

// =============================================================================
// Khởi tạo detector
// =============================================================================
bool cmd_detector_init(void)
{
    g_score_index = 0;
    g_last_detect_ms = 0;
    memset(g_scores, 0, sizeof(g_scores));

    // 1. Khởi tạo mạng nơ-ron AI
    if (!ai_init()) {
        return false;
    }

    // 2. Khởi tạo bộ xử lý âm thanh FFT
    audio_processor_init(&g_audio_processor, AUDIO_LENGTH, WINDOW_SIZE, STEP_SIZE, POOLING_SIZE);

    printf("Command Detector da san sang!\n");
    return true;
}

// =============================================================================
// Chạy 1 chu kỳ nhận diện (được gọi mỗi 0.1s khi có 1 trang mới trong Ring Buffer)
// =============================================================================
void cmd_detector_run(void)
{
    long now = get_millis();

    // Bước 1: Lấy con trỏ đọc từ Ring Buffer và tua ngược lại đúng 1 giây (16000 mẫu)
    RingBufferAccessor reader = i2s_mic_get_reader();
    ring_buffer_rewind(&reader, AUDIO_LENGTH);

    // Bước 2: Tính ảnh phổ Spectrogram đưa vào bộ đệm đầu vào của AI
    float *input_buffer = ai_get_input_buffer();
    bool is_valid_audio = audio_processor_get_spectrogram(&g_audio_processor, &reader, input_buffer);

    // Bước 3: Chạy mạng nơ-ron suy luận
    float output_probs[AI_NUM_COMMANDS];
    ai_predict(output_probs);

    // Bước 4: Lưu log(xác suất) vào bộ đệm lịch sử
    for (int i = 0; i < AI_NUM_COMMANDS; i++) {
        float prob = is_valid_audio ? output_probs[i] : 1e-6f;
        if (prob < 1e-6f) prob = 1e-6f; // Tránh lỗi log(0)
        g_scores[g_score_index][i] = logf(prob);
    }
    g_score_index = (g_score_index + 1) % COMMAND_WINDOW;

    // Bước 5: Tổng hợp điểm của 3 lần quét gần nhất
    float total_scores[AI_NUM_COMMANDS] = {0};
    for (int w = 0; w < COMMAND_WINDOW; w++) {
        for (int c = 0; c < AI_NUM_COMMANDS; c++) {
            total_scores[c] += g_scores[w][c];
        }
    }

    // Tìm lệnh có điểm cao nhất
    float best_score = total_scores[0];
    int   best_index = 0;
    for (int i = 1; i < AI_NUM_COMMANDS; i++) {
        if (total_scores[i] > best_score) {
            best_score = total_scores[i];
            best_index = i;
        }
    }

    // Bước 6: Kiểm tra điều kiện chấp nhận lệnh
    //  - Điểm vượt ngưỡng DETECTION_THRESHOLD
    //  - Không phải là lệnh vô nghĩa (index 4: _nonsense)
    //  - Đã cách lệnh trước ít nhất 1 giây (cooldown tránh kích hoạt lặp)
    bool score_ok    = (best_score > DETECTION_THRESHOLD);
    bool not_junk    = (best_index != 4);
    bool cooldown_ok = ((now - g_last_detect_ms) > 1000);

    if (score_ok && not_junk && cooldown_ok) {
        g_last_detect_ms = now;
        // Đẩy lệnh hợp lệ vào hàng đợi để Task điều khiển xe thực thi
        cmd_processor_send((uint16_t)best_index, best_score);
    }
}
