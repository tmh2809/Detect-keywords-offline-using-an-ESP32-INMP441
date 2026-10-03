// =============================================================================
// hamming_window.h - Hàm cửa sổ Hamming
//
// Mục đích: Trước khi thực hiện FFT, ta nhân mỗi mẫu với hệ số Hamming.
// Điều này giảm "spectral leakage" - hiện tượng năng lượng tần số bị rò sang
// các bin lân cận do tín hiệu bị cắt đột ngột ở đầu/cuối cửa sổ.
//
// Công thức: w(n) = 0.5 - 0.5 × cos(2π × (n+0.5) / N)
// Kết quả: hàm hình chuông, đầu và cuối = 0, giữa = 1
// =============================================================================

#ifndef HAMMING_WINDOW_H
#define HAMMING_WINDOW_H

typedef struct {
    float *coefficients;    // mảng hệ số đã tính sẵn (tính một lần, dùng mãi)
    int    window_size;     // số mẫu trong một cửa sổ
} HammingWindow;

// Cấp phát và tính toán hệ số Hamming
void hamming_window_init(HammingWindow *hw, int window_size);

// Giải phóng bộ nhớ
void hamming_window_deinit(HammingWindow *hw);

// Áp dụng cửa sổ Hamming lên mảng input (nhân từng phần tử với hệ số)
void hamming_window_apply(const HammingWindow *hw, float *input);

#endif // HAMMING_WINDOW_H
