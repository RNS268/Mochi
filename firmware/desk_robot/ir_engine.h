#ifndef IR_ENGINE_H
#define IR_ENGINE_H

#include <driver/rmt.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <Preferences.h>
#include "config.h"

#define IR_RMT_CHANNEL RMT_CHANNEL_0
#define IR_RAW_MAX 256

class IREngine {
public:
  IREngine() : _irrecv(IR_RX_PIN) {}

  void begin() {
    rmt_config_t rmt_tx = RMT_DEFAULT_CONFIG_TX((gpio_num_t)IR_TX_PIN, IR_RMT_CHANNEL);
    rmt_tx.clk_div = 80; // 1us tick
    
    rmt_config(&rmt_tx);
    rmt_driver_install(rmt_tx.channel, 0, 0);

    _prefs.begin("ir_codes", false);
  }

  void transmitRaw(const uint16_t* timings, size_t count) {
    size_t rmt_item_count = (count + 1) / 2;
    rmt_item32_t* items = (rmt_item32_t*)malloc(sizeof(rmt_item32_t) * rmt_item_count);
    if (!items) return;

    for (size_t i = 0; i < rmt_item_count; i++) {
      items[i].level0 = 1; // Mark
      items[i].duration0 = timings[i * 2];
      
      if (i * 2 + 1 < count) {
        items[i].level1 = 0; // Space
        items[i].duration1 = timings[i * 2 + 1];
      } else {
        items[i].level1 = 0;
        items[i].duration1 = 0;
      }
    }

    rmt_write_items(IR_RMT_CHANNEL, items, rmt_item_count, true);
    free(items);
  }

  bool saveCode(const char* name, const uint16_t* timings, size_t count) {
    char key[36];  // FIX: was 16 — "c_" + up to 31-char name needs 34 bytes minimum
    snprintf(key, sizeof(key), "c_%s", name);
    return _prefs.putBytes(key, timings, count * sizeof(uint16_t)) > 0;
  }

  size_t loadCode(const char* name, uint16_t* buffer, size_t maxCount) {
    char key[36];  // FIX: was 16 — must match saveCode key buffer size
    snprintf(key, sizeof(key), "c_%s", name);
    return _prefs.getBytes(key, buffer, maxCount * sizeof(uint16_t)) / sizeof(uint16_t);
  }

  void startLearning() {
    _irrecv.enableIRIn();
  }

  // Receive IR from remote, convert to raw timings, save with name. Returns true when saved.
  bool learnFromRemote(const char* name) {
    decode_results results;
    if (!_irrecv.decode(&results)) return false;
    _irrecv.resume();

    if (results.overflow || results.rawlen < 10) return false;

    uint16_t timings[IR_RAW_MAX];
    size_t n = (results.rawlen < IR_RAW_MAX) ? results.rawlen : IR_RAW_MAX;
    for (size_t i = 0; i < n; i++) {
      uint32_t us = results.rawbuf[i] * (uint32_t)kRawTick;
      timings[i] = (us > 65535) ? 65535 : (uint16_t)us;
    }
    bool ok = saveCode(name, timings, n);
    _irrecv.enableIRIn();
    return ok;
  }

  bool captureCode(decode_results& results) {
    if (_irrecv.decode(&results)) {
      _irrecv.resume();
      return true;
    }
    return false;
  }

  static const uint8_t kRawTick = 1;  // rawbuf already in microseconds

private:
  Preferences _prefs;
  IRrecv _irrecv;
};

#endif
