#ifndef WAKE_WORD_ENGINE_H
#define WAKE_WORD_ENGINE_H

/* 
 * NOTE: Local wake-word detection on ESP32-S3 uses the ESP-SR / WakeNet library.
 * In a standard Arduino environment, this typically requires the ESP32-S3 
 * to be configured with the "ESP-Skainet" components.
 */

#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "config.h"

class WakeWordEngine {
public:
  void begin() {
    // Initialize the Audio Front-End (AFE) for Speech Recognition
    // This handles noise reduction and AEC (Acoustic Echo Cancellation)
    const esp_afe_sr_iface_t *afe_handle = &ESP_AFE_SR_HANDLE;
    afe_config_t afe_config = AFE_CONFIG_DEFAULT();
    afe_config.wakenet_init = true;
    
    _afe_data = afe_handle->create_from_config(&afe_config);
    Serial.println("WakeNet Initialized locally on S3.");
  }

  bool listen(int16_t* audioChunk, size_t count) {
    // Feed the audio chunk to the AFE
    // In a real implementation, this would return a signal when the wake-word is detected
    // This is a simplified interface for the Mochi project
    
    const esp_afe_sr_iface_t *afe_handle = &ESP_AFE_SR_HANDLE;
    afe_handle->feed(_afe_data, audioChunk);
    
    int res = afe_handle->get_wake_state(_afe_data);
    if (res == WAKENET_DETECTED) {
      Serial.println(">> WAKE WORD DETECTED: Hey Mochi!");
      return true;
    }
    return false;
  }

private:
  void* _afe_data;
};

#endif
