// =============================================================================
// hamming_window.c - Triển khai Hamming Window
// =============================================================================

#include <stdlib.h>
#include <math.h>
#include "hamming_window.h"

void hamming_window_init(HammingWindow *hw, int window_size)
{
    hw->window_size  = window_size;
    hw->coefficients = (float *)malloc(sizeof(float) * window_size);

    // Tính hệ số Hamming cho từng vị trí n = 0..window_size-1
    // w(n) = 0.5 - 0.5 × cos(2π × (n + 0.5) / N)
    float arg = (float)(M_PI * 2.0 / window_size);
    for (int i = 0; i < window_size; i++) {
        hw->coefficients[i] = 0.5f - 0.5f * cosf(arg * (i + 0.5f));
    }
}

void hamming_window_deinit(HammingWindow *hw)
{
    free(hw->coefficients);
    hw->coefficients = NULL;
}

void hamming_window_apply(const HammingWindow *hw, float *input)
{
    // Nhân từng mẫu với hệ số tương ứng
    for (int i = 0; i < hw->window_size; i++) {
        input[i] = input[i] * hw->coefficients[i];
    }
}
