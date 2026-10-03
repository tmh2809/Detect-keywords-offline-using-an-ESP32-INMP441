#ifndef CONFIG_H
#define CONFIG_H

#include <driver/gpio.h>

// --- 1. Microphone INMP441 (Giao tiếp I2S) ---
// Chân L/R trên module mic nối GND để chọn Kênh Trái
#define I2S_MIC_SERIAL_CLOCK        GPIO_NUM_14   // Chân SCK (Bit Clock)
#define I2S_MIC_LEFT_RIGHT_CLOCK    GPIO_NUM_15   // Chân WS  (Word Select / LRCK)
#define I2S_MIC_SERIAL_DATA         GPIO_NUM_16   // Chân SD  (Serial Data IN)

// --- 2. Động cơ Servo điều khiển bánh xe (PWM 50Hz) ---
#define PIN_MOTOR_LEFT              GPIO_NUM_13   // Servo bánh trái
#define PIN_MOTOR_RIGHT             GPIO_NUM_12   // Servo bánh phải

// --- 3. Đèn LED báo hiệu trạng thái ---
#define PIN_LED                     GPIO_NUM_2    // LED báo nhận lệnh

#endif // CONFIG_H