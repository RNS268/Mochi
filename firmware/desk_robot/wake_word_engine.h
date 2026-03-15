#ifndef WAKE_WORD_ENGINE_H
#define WAKE_WORD_ENGINE_H

#include <ESP_I2S.h>
#include <ESP_SR.h>
#include <sdkconfig.h>
#include "config.h"

// Hardware partition check
#if !(defined(CONFIG_MODEL_IN_FLASH) || defined(CONFIG_MODEL_IN_SDCARD))
#error "CRITICAL: Mochi needs memory! Please go to Tools -> Partition Scheme and select '8MB with spiffs (ESP Speech Recognition)' or enable 'OPI PSRAM'."
#endif

// The global callback fired when ESP_SR runs on its own background FreeRTOS task
#if defined(CONFIG_MODEL_IN_FLASH) || defined(CONFIG_MODEL_IN_SDCARD)

volatile bool _wakeWordHeard = false;

void global_sr_callback(sr_event_t event, int command_id, int phrase_id) {
  if (event == SR_EVENT_WAKEWORD) {
    _wakeWordHeard = true;
  }
}
#endif

class WakeWordEngine {
public:
  void begin(I2SClass& sharedI2S) {
    #if defined(CONFIG_MODEL_IN_FLASH) || defined(CONFIG_MODEL_IN_SDCARD)
    ESP_SR.onEvent((sr_cb)global_sr_callback);
    // Begin reading natively from the mic
    // Mode MN means "M"ic on one channel, "N"ot used on the other
    bool success = ESP_SR.begin(sharedI2S, NULL, 0, SR_CHANNELS_STEREO, SR_MODE_WAKEWORD, "MN");
    if (!success) {
      Serial.println("FATAL: ESP_SR WakeWord Failed to Init.");
    } else {
      Serial.println("WakeWord Engine Initialized successfully.");
    }
    #endif
  }

  // Not used dynamically with HAL, but kept for legacy stubbing
  size_t getFeedChunkSize() const { return 256; }

  // Overhauled listen function checks the global boolean from the callback thread
  bool listen(int16_t* audioChunk, size_t count) {
    #if defined(CONFIG_MODEL_IN_FLASH) || defined(CONFIG_MODEL_IN_SDCARD)
    if (_wakeWordHeard) {
      _wakeWordHeard = false; // Reset the flag
      Serial.println(">> WAKE WORD DETECTED: Hey Mochi!");
      return true;
    }
    #endif
    return false;
  }
};

#endif
