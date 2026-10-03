// =============================================================================
// ring_buffer.h - Ring buffer lưu mẫu âm thanh
//
// Thay thế class AudioBuffer + class RingBufferAccessor (C++) bằng struct C.
//
// Ý tưởng hoạt động:
//   - Có AUDIO_BUFFER_COUNT "trang" bộ nhớ, mỗi trang chứa SAMPLE_BUFFER_SIZE mẫu
//   - Con trỏ ghi liên tục tiến về phía trước, khi hết trang cuối thì quay lại đầu
//     → gọi là "ring buffer" (vòng tròn)
//   - Khi đọc lại, có thể "tua lại" (rewind) để lấy 1 giây âm thanh vừa qua
// =============================================================================

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Số mẫu trong mỗi trang bộ nhớ (1600 mẫu × 11 trang = 17600 mẫu ≈ 1.1 giây ở 16kHz)
#define SAMPLE_BUFFER_SIZE  1600
#define AUDIO_BUFFER_COUNT  11

// =============================================================================
// AudioBuffer: một "trang" lưu SAMPLE_BUFFER_SIZE mẫu âm thanh 16-bit
// =============================================================================
typedef struct {
    int16_t samples[SAMPLE_BUFFER_SIZE];
} AudioBuffer;

// Khởi tạo một AudioBuffer (đặt tất cả mẫu về 0)
static inline void audio_buffer_init(AudioBuffer *buf) {
    memset(buf->samples, 0, sizeof(buf->samples));
}

// =============================================================================
// RingBufferAccessor: con trỏ trỏ vào một vị trí trong ring buffer
//
// Cấu trúc bộ nhớ:
//   [Trang 0: 1600 mẫu][Trang 1: 1600 mẫu]...[Trang 10: 1600 mẫu]
//    buffer_idx=0        buffer_idx=1           buffer_idx=10
//    buffer_pos: 0..1599
// =============================================================================
typedef struct {
    AudioBuffer **audio_buffers;    // mảng con trỏ đến các trang bộ nhớ
    int           num_buffers;      // số trang (= AUDIO_BUFFER_COUNT)
    int           buffer_idx;       // đang ở trang nào
    int           buffer_pos;       // đang ở vị trí nào trong trang
    int           total_size;       // tổng số mẫu = num_buffers × SAMPLE_BUFFER_SIZE
} RingBufferAccessor;

// --- Khởi tạo ---
static inline void ring_buffer_init(RingBufferAccessor *rb,
                                    AudioBuffer **bufs, int count) {
    rb->audio_buffers = bufs;
    rb->num_buffers   = count;
    rb->buffer_idx    = 0;
    rb->buffer_pos    = 0;
    rb->total_size    = count * SAMPLE_BUFFER_SIZE;
}

// --- Lấy vị trí tuyến tính hiện tại (0 đến total_size-1) ---
static inline int ring_buffer_get_index(const RingBufferAccessor *rb) {
    return rb->buffer_idx * SAMPLE_BUFFER_SIZE + rb->buffer_pos;
}

// --- Di chuyển đến vị trí tuyến tính bất kỳ (tự động xử lý âm / vượt biên) ---
static inline void ring_buffer_set_index(RingBufferAccessor *rb, int index) {
    // Đưa về phạm vi [0, total_size) - hỗ trợ index âm (ví dụ: tua lại)
    index = ((index % rb->total_size) + rb->total_size) % rb->total_size;
    rb->buffer_idx     = index / SAMPLE_BUFFER_SIZE;
    rb->buffer_pos     = index % SAMPLE_BUFFER_SIZE;
}

// --- Đọc mẫu tại vị trí hiện tại ---
static inline int16_t ring_buffer_get_sample(const RingBufferAccessor *rb) {
    return rb->audio_buffers[rb->buffer_idx]->samples[rb->buffer_pos];
}

// --- Ghi mẫu tại vị trí hiện tại ---
static inline void ring_buffer_set_sample(RingBufferAccessor *rb, int16_t sample) {
    rb->audio_buffers[rb->buffer_idx]->samples[rb->buffer_pos] = sample;
}

// --- Tua lại n mẫu so với vị trí hiện tại ---
static inline void ring_buffer_rewind(RingBufferAccessor *rb, int samples) {
    ring_buffer_set_index(rb, ring_buffer_get_index(rb) - samples);
}

// --- Tiến đến mẫu kế tiếp ---
// Trả về true nếu vừa chuyển sang trang mới (dùng để báo hiệu "đầy một trang")
static inline bool ring_buffer_advance(RingBufferAccessor *rb) {
    rb->buffer_pos++;
    if (rb->buffer_pos == SAMPLE_BUFFER_SIZE) {
        rb->buffer_pos = 0;
        rb->buffer_idx = (rb->buffer_idx + 1) % rb->num_buffers;
        return true;   // vừa chuyển trang → báo hiệu để kích hoạt task xử lý
    }
    return false;
}

#ifdef __cplusplus
}
#endif

#endif // RING_BUFFER_H
