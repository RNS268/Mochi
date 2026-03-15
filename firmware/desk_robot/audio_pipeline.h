#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

#include <ESP_I2S.h>
#include <string.h>
#include "config.h"

// 16 kHz, 16-bit mono. 30ms chunks = 480 samples (per paper).
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_CHUNK_30MS  480

extern I2SClass I2S0;
extern I2SClass I2S1;

class AudioPipeline {
public:
  void begin() {
    // Note: I2S0 (Mic) is configured directly in desk_robot.ino to share with ESP_SR.
    // Configure I2S1 for Amp (OUT)
    I2S1.setPins(I2S_AMP_BCLK, I2S_AMP_LRCK, I2S_AMP_DIN, -1, -1);
    I2S1.begin(I2S_MODE_STD, AUDIO_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  }

  void loopback() {
    int16_t sample[64];
    size_t bytes_read = I2S0.readBytes((char*)sample, sizeof(sample));
    if (bytes_read > 0) {
      I2S1.write((const uint8_t*)sample, bytes_read);
    }
  }

  size_t readChunk(int16_t* buffer, size_t count) {
    size_t bytes_read = I2S0.readBytes((char*)buffer, count * sizeof(int16_t));
    return bytes_read / sizeof(int16_t);
  }

  // Write samples to speaker (for acknowledgements)
  void writeSamples(const int16_t* samples, size_t count) {
    I2S1.write((const uint8_t*)samples, count * sizeof(int16_t));
  }

  // Play WAV from buffer (supports 16-bit mono/stereo, parses header)
  bool playWav(uint8_t* wavData, size_t wavLen) {
    if (wavLen < 44 || memcmp(wavData, "RIFF", 4) != 0 || memcmp(wavData + 8, "WAVE", 4) != 0)
      return false;
    uint16_t channels = wavData[22] | (wavData[23] << 8);
    uint32_t sampleRate = wavData[24] | (wavData[25] << 8) | (wavData[26] << 16) | (wavData[27] << 24);
    uint16_t bitsPerSample = wavData[34] | (wavData[35] << 8);
    if (bitsPerSample != 16) return false;

    size_t dataOffset = 44;
    for (size_t i = 12; i < wavLen - 8; i++) {
      if (memcmp(wavData + i, "data", 4) == 0) {
        dataOffset = i + 8;
        break;
      }
    }
    size_t dataLen = wavLen - dataOffset;
    int16_t* pcm = (int16_t*)(wavData + dataOffset);

    // FIX: Only reinit I2S1 if WAV sample rate differs from native rate
    if (sampleRate != AUDIO_SAMPLE_RATE) {
      I2S1.end();
      I2S1.begin(I2S_MODE_STD, sampleRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    }

    if (channels == 1) {
      int16_t stereo[512];
      size_t idx = 0;
      while (idx < dataLen / 2) {
        size_t n = (dataLen / 2 - idx) > 256 ? 256 : (dataLen / 2 - idx);
        for (size_t i = 0; i < n; i++) {
          stereo[i * 2] = pcm[idx + i];
          stereo[i * 2 + 1] = pcm[idx + i];
        }
        writeSamples(stereo, n * 2);
        idx += n;
      }
    } else {
      writeSamples(pcm, dataLen / 2);
    }
    
    // FIX: Only restore sample rate if we changed it
    if (sampleRate != AUDIO_SAMPLE_RATE) {
      I2S1.end();
      I2S1.begin(I2S_MODE_STD, AUDIO_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    }
    return true;
  }

  // Play a short beep (frequency Hz, duration ms) safely using chunks
  void playBeep(int freqHz, int durationMs) {
    const size_t numSamples = (size_t)(AUDIO_SAMPLE_RATE * durationMs / 1000);
    float omega = 2.0f * 3.14159f * freqHz / AUDIO_SAMPLE_RATE;
    
    int16_t stereo[512]; // Small chunk: 256 stereo frames (1KB)
    size_t generated = 0;
    
    while (generated < numSamples) {
      size_t chunk = (numSamples - generated > 256) ? 256 : (numSamples - generated);
      for (size_t i = 0; i < chunk; i++) {
        float t = (float)(generated + i);
        int16_t s = (int16_t)(8000.0f * sinf(omega * t) * (1.0f - (float)(generated + i) / numSamples));
        stereo[i * 2] = s;     // L
        stereo[i * 2 + 1] = s; // R
      }
      writeSamples(stereo, chunk * 2);
      generated += chunk;
    }
  }
};

#endif
