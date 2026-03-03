#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

#include <driver/i2s.h>
#include "config.h"

class AudioPipeline {
public:
  void begin() {
    // Configure I2S for Mic (IN)
    i2s_config_t i2s_mic_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false
    };

    i2s_pin_config_t mic_pins = {
        .bck_io_num = I2S_MIC_SCK,
        .ws_io_num = I2S_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MIC_SD
    };

    i2s_driver_install(I2S_NUM_0, &i2s_mic_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &mic_pins);

    // Configure I2S for Amp (OUT)
    i2s_config_t i2s_amp_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false
    };

    i2s_pin_config_t amp_pins = {
        .bck_io_num = I2S_AMP_BCLK,
        .ws_io_num = I2S_AMP_LRCK,
        .data_out_num = I2S_AMP_DIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM_1, &i2s_amp_config, 0, NULL);
    i2s_set_pin(I2S_NUM_1, &amp_pins);
  }

  void loopback() {
    int16_t sample[64];
    size_t bytes_read;
    i2s_read(I2S_NUM_0, &sample, sizeof(sample), &bytes_read, portMAX_DELAY);
    
    size_t bytes_written;
    i2s_write(I2S_NUM_1, &sample, bytes_read, &bytes_written, portMAX_DELAY);
  }

  size_t readChunk(int16_t* buffer, size_t count) {
    size_t bytes_read;
    i2s_read(I2S_NUM_0, buffer, count * sizeof(int16_t), &bytes_read, 0);
    return bytes_read / sizeof(int16_t);
  }
};

#endif
