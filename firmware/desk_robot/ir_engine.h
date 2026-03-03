#ifndef IR_ENGINE_H
#define IR_ENGINE_H

#include <driver/rmt.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <Preferences.h>
#include "config.h"

#define IR_RMT_CHANNEL RMT_CHANNEL_0
#define IR_RX_PIN_OPTIONAL 11 // Optional for learning

class IREngine {
public:
  IREngine() : _irrecv(IR_RX_PIN_OPTIONAL) {}

  void begin() {
    rmt_config_t rmt_tx = RMT_DEFAULT_CONFIG_TX((gpio_num_t)IR_TX_PIN, IR_RMT_CHANNEL);
    rmt_tx.clk_div = 80; // 1us tick
    
    rmt_config(&rmt_tx);
    rmt_driver_install(rmt_tx.channel, 0, 0);

    _prefs.begin("ir_codes", false);
  }

  void transmitRaw(const uint16_t* timings, size_t count) {
    size_t rmt_item_count = count;
    rmt_item32_t* items = (rmt_item32_t*)malloc(sizeof(rmt_item32_t) * rmt_item_count);

    for (size_t i = 0; i < count; i++) {
      items[i].level = (i % 2 == 0) ? 1 : 0; // Alternating High/Low
      items[i].duration = timings[i];
    }

    rmt_write_items(IR_RMT_CHANNEL, items, rmt_item_count, true);
    free(items);
  }

  bool saveCode(const char* name, const uint16_t* timings, size_t count) {
    char key[16];
    snprintf(key, sizeof(key), "c_%s", name);
    return _prefs.putBytes(key, timings, count * sizeof(uint16_t)) > 0;
  }

  size_t loadCode(const char* name, uint16_t* buffer, size_t maxCount) {
    char key[16];
    snprintf(key, sizeof(key), "c_%s", name);
    return _prefs.getBytes(key, buffer, maxCount * sizeof(uint16_t)) / sizeof(uint16_t);
  }

  void startLearning() {
    _irrecv.enableIRIn();
  }

  bool captureCode(decode_results& results) {
    if (_irrecv.decode(&results)) {
      _irrecv.resume();
      return true;
    }
    return false;
  }

private:
  Preferences _prefs;
  IRrecv _irrecv;
};

#endif
