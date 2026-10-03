// =============================================================================
// i2s_mic.c - Triển khai thu âm INMP441 qua I2S + DMA
//
// Tối ưu hóa:
//  - Dùng cấp phát tĩnh (Static Allocation trong BSS) cho Ring Buffer
//  - Không dùng malloc -> Không bao giờ lo phân mảnh bộ nhớ (Heap Fragmentation)
// =============================================================================

#include <stdio.h>
#include <driver/i2s.h>
#include "i2s_mic.h"

#define I2S_PORT            I2S_NUM_0
#define SAMPLE_RATE         16000     // 16kHz
#define DMA_BUFFER_COUNT    4         // 4 bộ đệm DMA
#define DMA_BUFFER_LEN      64        // 64 mẫu mỗi bộ đệm

// Biến quản lý bộ đệm vòng (Ring Buffer) - Cấp phát tĩnh 100% trong BSS
static AudioBuffer        g_audio_storage[AUDIO_BUFFER_COUNT];
static AudioBuffer       *g_audio_buffers[AUDIO_BUFFER_COUNT];
static RingBufferAccessor  g_write_accessor;
static TaskHandle_t        g_ai_task = NULL;
static TaskHandle_t        g_reader_task = NULL;
static QueueHandle_t       g_i2s_queue = NULL;

// =============================================================================
// Chuyển đổi dữ liệu 32-bit thô từ INMP441 thành mẫu 16-bit
//
// Mic INMP441 xuất 24-bit căn lề trái trong khung 32-bit:
//  - Ta chia cho 2^31 để đưa về khoảng [-1.0, 1.0]
//  - Nhân với 32767 để ép về kiểu số nguyên có dấu int16_t
// =============================================================================
static void process_mic_data(uint8_t *raw_data, size_t bytes_read)
{
    int32_t *samples = (int32_t *)raw_data;
    int num_samples  = (int)(bytes_read / 4); // Mỗi mẫu chiếm 4 bytes (32-bit)

    for (int i = 0; i < num_samples; i++) {
        // Chuẩn hóa [-1.0, 1.0]
        float normalised = (float)samples[i] / 2147483648.0f; // 2^31
        int16_t sample_16 = (int16_t)(32767.0f * normalised);

        // Ghi mẫu vào vị trí hiện tại của Ring Buffer
        ring_buffer_set_sample(&g_write_accessor, sample_16);

        // Tiến con trỏ ghi lên 1 bước.
        // Nếu vừa đầy 1 trang (1600 mẫu = 0.1s) -> Bắn xTaskNotify báo Task AI
        if (ring_buffer_advance(&g_write_accessor)) {
            if (g_ai_task != NULL) {
                xTaskNotify(g_ai_task, 1, eSetBits);
            }
        }
    }
}

// =============================================================================
// Task đọc dữ liệu từ DMA I2S (chạy ngầm, ưu tiên cao)
// =============================================================================
static void i2s_reader_task(void *param)
{
    (void)param;
    while (1) {
        i2s_event_t evt;
        // Chờ sự kiện ngắt từ phần cứng DMA (khi 1 bộ đệm DMA 64 mẫu đã đầy)
        if (xQueueReceive(g_i2s_queue, &evt, portMAX_DELAY) == pdPASS) {
            if (evt.type == I2S_EVENT_RX_DONE) {
                size_t bytes_read = 0;
                // Đọc sạch dữ liệu trong bộ đệm DMA ra mảng tạm rồi chuyển vào Ring Buffer
                do {
                    uint8_t temp_buf[1024];
                    i2s_read(I2S_PORT, temp_buf, sizeof(temp_buf), &bytes_read, 10);
                    if (bytes_read > 0) {
                        process_mic_data(temp_buf, bytes_read);
                    }
                } while (bytes_read > 0);
            }
        }
    }
}

// =============================================================================
// Khởi tạo toàn bộ module thu âm I2S + Ring Buffer
// =============================================================================
bool i2s_mic_init(const I2SMicPins *pins, TaskHandle_t ai_task_to_notify)
{
    g_ai_task = ai_task_to_notify;

    // 1. Cấp phát tĩnh bộ nhớ cho 11 trang Ring Buffer (vùng BSS, không dùng malloc)
    for (int i = 0; i < AUDIO_BUFFER_COUNT; i++) {
        g_audio_buffers[i] = &g_audio_storage[i];
        audio_buffer_init(g_audio_buffers[i]);
    }
    ring_buffer_init(&g_write_accessor, g_audio_buffers, AUDIO_BUFFER_COUNT);

    // 2. Cấu hình phần cứng I2S cho mic INMP441
    i2s_config_t i2s_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT, // INMP441 nối L/R xuống GND -> Kênh trái
        .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = DMA_BUFFER_COUNT,
        .dma_buf_len          = DMA_BUFFER_LEN,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0,
    };

    // Cài đặt driver I2S và tạo queue sự kiện DMA (sâu 4 phần tử)
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, DMA_BUFFER_COUNT, &g_i2s_queue);
    if (err != ESP_OK) {
        printf("Loi i2s_driver_install: %d\n", err);
        return false;
    }

    // 3. Cấu hình chân GPIO cho mic INMP441
    i2s_pin_config_t pin_config = {
        .bck_io_num   = pins->bck_pin,
        .ws_io_num    = pins->ws_pin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = pins->sd_pin,
    };
    i2s_set_pin(I2S_PORT, &pin_config);

    // 4. Tạo task đọc I2S ngầm trên Core 0
    xTaskCreatePinnedToCore(i2s_reader_task, "I2SReader", 4096, NULL, 1, &g_reader_task, 0);

    printf("I2S Mic INMP441 khoi tao thanh cong!\n");
    return true;
}

// =============================================================================
// Tạo một con trỏ đọc tại vị trí ghi hiện tại (để tua ngược 1 giây)
// =============================================================================
RingBufferAccessor i2s_mic_get_reader(void)
{
    RingBufferAccessor reader;
    ring_buffer_init(&reader, g_audio_buffers, AUDIO_BUFFER_COUNT);
    ring_buffer_set_index(&reader, ring_buffer_get_index(&g_write_accessor));
    return reader;
}
