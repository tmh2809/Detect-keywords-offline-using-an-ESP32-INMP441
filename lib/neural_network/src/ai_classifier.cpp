// =============================================================================
// ai_classifier.cpp - Triển khai suy luận TFLite Micro cho C
// =============================================================================

#include "ai_classifier.h"
#include "NeuralNetwork.h"
#include <Arduino.h>

static NeuralNetwork *g_neural_network = NULL;

bool ai_init(void)
{
    if (g_neural_network != NULL) {
        return true;
    }

    g_neural_network = new NeuralNetwork();
    if (!g_neural_network) {
        Serial.println("Loi: Khong the khoi tao Neural Network!");
        return false;
    }

    Serial.println("Neural Network TFLite khoi tao thanh cong!");
    return true;
}

float *ai_get_input_buffer(void)
{
    if (!g_neural_network) return NULL;
    return g_neural_network->getInputBuffer();
}

void ai_predict(float *output_scores)
{
    if (!g_neural_network || !output_scores) return;

    // Chạy suy luận mạng nơ-ron
    g_neural_network->predict();

    // Copy kết quả xác suất ra ngoài
    float *out = g_neural_network->getOutputBuffer();
    for (int i = 0; i < AI_NUM_COMMANDS; i++) {
        output_scores[i] = out[i];
    }
}
