// =============================================================================
// audio_processor.h - Chuyển đổi âm thanh thô → spectrogram (ảnh tần số)
//
// Pipeline xử lý:
//   Audio samples (PCM 16-bit)
//       ↓  [chia thành các cửa sổ 320 mẫu, bước 160 mẫu]
//   Hamming window (giảm spectral leakage)
//       ↓
//   FFT (Fast Fourier Transform) - kissfft
//       ↓
//   Tính năng lượng (magnitude²) từng bin tần số
//       ↓
//   Average pooling (gom nhóm 6 bin → 1)
//       ↓
//   log10() để scale về dải động hợp lý
//       ↓
//   Spectrogram: mảng 2D [số_cửa_sổ × pooled_energy_size]
//   → Đầu vào của mạng neural
// =============================================================================

#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "./kissfft/tools/kiss_fftr.h"
#include "hamming_window.h"
#include "ring_buffer.h"

typedef struct {
    int   audio_length;         // số mẫu cần xử lý (= 16000 = 1 giây @ 16kHz)
    int   window_size;          // kích thước cửa sổ FFT (= 320 mẫu = 20ms)
    int   step_size;            // bước nhảy giữa các cửa sổ (= 160 mẫu = 10ms)
    int   pooling_size;         // số bin FFT gom lại (= 6)
    int   fft_size;             // kích thước FFT (= 2^n ≥ window_size, = 512)
    int   energy_size;          // số bin năng lượng = fft_size/2 + 1
    int   pooled_energy_size;   // sau pooling = ceil(energy_size / pooling_size)

    float         *fft_input;   // mảng đầu vào FFT [fft_size]
    kiss_fft_cpx  *fft_output;  // mảng đầu ra FFT phức [energy_size]
    float         *energy;      // mảng năng lượng = real² + imag² [energy_size]
    kiss_fftr_cfg  fft_cfg;     // cấu hình kissfft (cấp phát nội bộ)

    HammingWindow  hamming;     // cửa sổ Hamming (nhúng trực tiếp, không cấp phát)
    float          smoothed_noise_floor; // ngưỡng nhiễu nền (exponential smoothing)
} AudioProcessor;

// Khởi tạo và cấp phát tất cả bộ nhớ cần thiết
void audio_processor_init(AudioProcessor *ap,
                           int audio_length,
                           int window_size,
                           int step_size,
                           int pooling_size);

// Giải phóng bộ nhớ
void audio_processor_deinit(AudioProcessor *ap);

// Tính spectrogram từ ring buffer
// reader: con trỏ đọc ring buffer (sẽ được hàm di chuyển nội bộ)
// output: mảng đầu ra spectrogram [số_cửa_sổ × pooled_energy_size]
// Trả về: true nếu âm thanh có đủ tín hiệu (không phải lặng)
bool audio_processor_get_spectrogram(AudioProcessor *ap,
                                      RingBufferAccessor *reader,
                                      float *output);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_PROCESSOR_H
