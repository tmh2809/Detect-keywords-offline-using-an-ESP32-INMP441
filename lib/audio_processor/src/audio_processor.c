// =============================================================================
// audio_processor.c - Xử lý âm thanh: tính spectrogram
// =============================================================================

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "audio_processor.h"
#include "./kissfft/tools/kiss_fftr.h"

#define EPSILON 1e-6f   // giá trị nhỏ để tránh log(0)

// =============================================================================
// audio_processor_init: khởi tạo và cấp phát bộ nhớ
// =============================================================================
void audio_processor_init(AudioProcessor *ap,
                           int audio_length,
                           int window_size,
                           int step_size,
                           int pooling_size)
{
    ap->audio_length = audio_length;
    ap->window_size  = window_size;
    ap->step_size    = step_size;
    ap->pooling_size = pooling_size;

    // Tìm fft_size = 2^n nhỏ nhất >= window_size
    // Ví dụ: window_size=320 → fft_size=512 (vì 2^9=512 >= 320)
    ap->fft_size = 1;
    while (ap->fft_size < window_size) {
        ap->fft_size <<= 1;  // nhân đôi cho đến khi đủ lớn
    }

    // Số bin tần số = fft_size/2 + 1 (FFT thực → chỉ nửa phổ)
    ap->energy_size = ap->fft_size / 2 + 1;

    // Sau average pooling: ceil(energy_size / pooling_size)
    ap->pooled_energy_size = (int)ceilf((float)ap->energy_size / pooling_size);
    printf("Pooled energy size = %d\n", ap->pooled_energy_size);

    // Cấp phát bộ nhớ làm việc
    ap->fft_input  = (float *)malloc(sizeof(float) * ap->fft_size);
    ap->fft_output = (kiss_fft_cpx *)malloc(sizeof(kiss_fft_cpx) * ap->energy_size);
    ap->energy     = (float *)malloc(sizeof(float) * ap->energy_size);

    // Khởi tạo kissfft (tính toán sẵn "twiddle factors")
    ap->fft_cfg = kiss_fftr_alloc(ap->fft_size, 0, NULL, NULL);

    // Khởi tạo Hamming window
    hamming_window_init(&ap->hamming, window_size);

    ap->smoothed_noise_floor = 0.0f;
}

// =============================================================================
// audio_processor_deinit: giải phóng bộ nhớ
// =============================================================================
void audio_processor_deinit(AudioProcessor *ap)
{
    free(ap->fft_cfg);
    free(ap->fft_input);
    free(ap->fft_output);
    free(ap->energy);
    hamming_window_deinit(&ap->hamming);
}

// =============================================================================
// get_spectrogram_segment: tính một hàng của spectrogram từ fft_input[]
//
// Các bước:
//  1. Áp dụng Hamming window lên fft_input[]
//  2. FFT: fft_input[] → fft_output[] (phức)
//  3. Tính năng lượng: |X|² = real² + imag²
//  4. Average pooling: gom nhóm 6 bin → lấy trung bình
//  5. log10(): scale về dải [-6, 0] để phù hợp với mạng neural
// =============================================================================
static void get_spectrogram_segment(AudioProcessor *ap, float *output_row)
{
    // Bước 1: Hamming window
    hamming_window_apply(&ap->hamming, ap->fft_input);

    // Bước 2: FFT thực (real FFT) - đầu ra là phổ phức nửa trên
    kiss_fftr(ap->fft_cfg, ap->fft_input, (kiss_fft_cpx *)ap->fft_output);

    // Bước 3: Tính năng lượng từng bin |X[k]|² = Re² + Im²
    for (int i = 0; i < ap->energy_size; i++) {
        float re = ap->fft_output[i].r;
        float im = ap->fft_output[i].i;
        ap->energy[i] = re * re + im * im;
    }

    // Bước 4: Average pooling - gom nhóm pooling_size bin → trung bình
    float *src = ap->energy;
    float *dst = output_row;
    for (int i = 0; i < ap->energy_size; i += ap->pooling_size) {
        float avg = 0.0f;
        for (int j = 0; j < ap->pooling_size; j++) {
            if (i + j < ap->energy_size) {
                avg += *src;
                src++;
            }
        }
        *dst = avg / ap->pooling_size;
        dst++;
    }

    // Bước 5: log10 để scale
    for (int i = 0; i < ap->pooled_energy_size; i++) {
        output_row[i] = log10f(output_row[i] + EPSILON);
    }
}

// =============================================================================
// audio_processor_get_spectrogram: tính toàn bộ spectrogram từ 1 giây âm thanh
// =============================================================================
bool audio_processor_get_spectrogram(AudioProcessor *ap,
                                      RingBufferAccessor *reader,
                                      float *output)
{
    int start_index = ring_buffer_get_index(reader);

    // --- Pass 1: tính mean (giá trị trung bình) để trừ DC offset ---
    float mean = 0.0f;
    for (int i = 0; i < ap->audio_length; i++) {
        mean += ring_buffer_get_sample(reader);
        ring_buffer_advance(reader);
    }
    mean /= ap->audio_length;

    // --- Pass 2: tính max và noise floor ---
    ring_buffer_set_index(reader, start_index);
    float max_value    = 0.0f;
    float noise_floor  = 0.0f;
    int   samples_over = 0;  // số mẫu vượt quá ngưỡng nhiễu nền × 5

    for (int i = 0; i < ap->audio_length; i++) {
        float val = fabsf((float)ring_buffer_get_sample(reader) - mean);
        if (val > max_value)   max_value = val;
        noise_floor += val;
        if (val > 5.0f * ap->smoothed_noise_floor) {
            samples_over++;
        }
        ring_buffer_advance(reader);
    }
    noise_floor /= ap->audio_length;

    // Cập nhật nhiễu nền bằng exponential smoothing (học từ từ)
    if (noise_floor < ap->smoothed_noise_floor) {
        // Tiếng ồn giảm → cập nhật nhanh hơn
        ap->smoothed_noise_floor = 0.7f * ap->smoothed_noise_floor
                                 + 0.3f * noise_floor;
    } else {
        // Tiếng ồn tăng → cập nhật chậm (để không bị đánh lừa bởi tiếng nói)
        ap->smoothed_noise_floor = 0.99f * ap->smoothed_noise_floor
                                 + 0.01f * noise_floor;
    }

    // --- Pass 3: tính spectrogram từng cửa sổ ---
    // Trượt cửa sổ qua toàn bộ 1 giây âm thanh, bước = step_size
    for (int window_start = start_index;
         window_start < start_index + ap->audio_length - ap->window_size;
         window_start += ap->step_size)
    {
        // Di chuyển đến đầu cửa sổ
        ring_buffer_set_index(reader, window_start);

        // Đọc window_size mẫu, chuẩn hoá về [-1.0, 1.0]
        for (int i = 0; i < ap->window_size; i++) {
            ap->fft_input[i] = ((float)ring_buffer_get_sample(reader) - mean)
                                / (max_value + EPSILON);
            ring_buffer_advance(reader);
        }
        // Zero-pad phần còn lại lên fft_size
        for (int i = ap->window_size; i < ap->fft_size; i++) {
            ap->fft_input[i] = 0.0f;
        }

        // Tính một hàng của spectrogram
        get_spectrogram_segment(ap, output);

        // Tiến đến hàng kế tiếp trong ma trận spectrogram
        output += ap->pooled_energy_size;
    }

    // Trả về true nếu có đủ tín hiệu vượt ngưỡng nhiễu
    // (ít nhất 5% số mẫu = 800 mẫu phải là tiếng động)
    return samples_over > ap->audio_length / 20;
}
