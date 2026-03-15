/*
 * Mochi - ESP32-S3 Tabletop Assistant Robot
 * State machine coordinates IDLE → LISTENING → THINKING → SPEAKING
 * Paper: "Mochi: An ESP32-S3 Based Intelligent Tabletop Assistant Robot"
 */

#include <WiFi.h>
#include <math.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "wake_word_engine.h"
#include "audio_pipeline.h"
#include "cloud_client.h"
#include "ir_engine.h"
#include "oled_engine.h"
#include <ESP_I2S.h>
I2SClass I2S0;
I2SClass I2S1;

// ---- State Machine ----
enum State {
  IDLE,       // Waiting for wake word
  LISTENING,  // Recording (VAD: ends on pause)
  THINKING,   // Cloud API processing
  SPEAKING,   // Playing TTS / acknowledgement
  LEARN_IR    // Waiting for IR from remote to store
};
volatile State g_state = IDLE;

// ---- Hardware ----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

TwoWire oledWire = TwoWire(0);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &oledWire, OLED_RESET);

WakeWordEngine wakeWord;
AudioPipeline audio;
CloudClient cloud;
IREngine irEngine;
OLEDEngine oled(display);

// ---- Buffers ----
#define MAX_WAKE_CHUNK 512
#define TTS_BUF_SIZE 32768           // 32KB for TTS audio
#define RECORD_BUF_MAX_SAMPLES 40000 // 2.5 sec max @ 16kHz (single definition)

// ---- Heap Monitoring ----
#define HEAP_LOG_INTERVAL 5000          // Print heap every 5 seconds
unsigned long lastHeapLogMs = 0;

// ---- VAD Thresholds ----
#ifndef VAD_SILENCE_THRESHOLD
#define VAD_SILENCE_THRESHOLD 500
#endif
#ifndef VAD_PAUSE_MS
#define VAD_PAUSE_MS 800
#endif
#ifndef VAD_MIN_SPEECH_MS
#define VAD_MIN_SPEECH_MS 300
#endif
int16_t wakeChunk[MAX_WAKE_CHUNK];
uint8_t* ttsBuffer = nullptr;
int16_t* recordBuf = nullptr;
size_t recordLen = 0;

char _learnIrName[32] = {0};

// ---- VAD (Voice Activity Detection) ----
unsigned long lastSpeechMs = 0;   // last time we saw speech
unsigned long speechStartMs = 0;  // when speech began
bool speechStarted = false;

// ---- FreeRTOS ----
#define OLED_TASK_STACK 4096  // sinf() + FPU save + SSD1306 draw calls need headroom
#define OLED_TASK_PRIO  1
TaskHandle_t oledTaskHandle = nullptr;

void oledTask(void* arg) {
  const int fps = 15;
  const TickType_t period = pdMS_TO_TICKS(1000 / fps);
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    oled.update();
    vTaskDelayUntil(&lastWake, period);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Mochi starting...");

  oledWire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("FATAL: SSD1306 not found! Check wiring (SDA=8, SCL=9) and I2C address (0x3C or 0x3D).");
    // Blink onboard LED forever so you know display init failed
    while (true) { delay(500); }
  }
  display.clearDisplay();
  display.display();

  // Set up the ESP_SR wrapper passing the standard Arduino I2S0 object
  I2S0.setPins(I2S_MIC_SCK, I2S_MIC_WS, -1, I2S_MIC_SD);
  I2S0.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  wakeWord.begin(I2S0);
  audio.begin();  // FIX: Initialize I2S1 (amplifier) — was missing

  // FIX: Allocate audio buffers (prefer PSRAM, fall back to internal RAM)
  recordBuf = (int16_t*)heap_caps_malloc(RECORD_BUF_MAX_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!recordBuf) recordBuf = (int16_t*)malloc(RECORD_BUF_MAX_SAMPLES * sizeof(int16_t));
  if (!recordBuf) Serial.println("FATAL: recordBuf allocation failed!");

  ttsBuffer = (uint8_t*)heap_caps_malloc(TTS_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ttsBuffer) ttsBuffer = (uint8_t*)malloc(TTS_BUF_SIZE);
  if (!ttsBuffer) Serial.println("FATAL: ttsBuffer allocation failed!");

  irEngine.begin();
  oled.setState(OLEDEngine::EYES_IDLE);

  xTaskCreatePinnedToCore(oledTask, "oled", OLED_TASK_STACK, nullptr, OLED_TASK_PRIO, &oledTaskHandle, 0);

  WiFi.mode(WIFI_OFF); // Start with WiFi disabled
  g_state = IDLE;
}

void enableWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 50) {
    delay(200);
    Serial.print(".");
    wifiAttempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected.");
  } else {
    Serial.println("\nWiFi connection failed! Proceeding in offline mode.");
  }
}

void disableWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi disabled.");
}

void loop() {
  // Periodic heap monitoring
  unsigned long now = millis();
  if (now - lastHeapLogMs >= HEAP_LOG_INTERVAL) {
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    lastHeapLogMs = now;
  }

  switch (g_state) {
    case IDLE:
      // Process wake-word in AFE chunks (~16–30ms) without blocking
      {
        size_t n = audio.readChunk(wakeChunk, wakeWord.getFeedChunkSize());
        if (n > 0 && wakeWord.listen(wakeChunk, n)) {
          g_state = LISTENING;
          recordLen = 0;
          speechStarted = false;
          lastSpeechMs = millis();
          speechStartMs = millis();
          oled.setState(OLEDEngine::EYES_LISTENING);
          audio.playBeep(880, 80);  // acknowledgement: "listening"
          Serial.println(">> LISTENING");
        }
      }
      break;

    case LISTENING:
      {
        size_t n = audio.readChunk(wakeChunk, MAX_WAKE_CHUNK);
        unsigned long now = millis();
        if (n > 0) {
          int64_t sumSq = 0;
          for (size_t i = 0; i < n; i++) {
            int32_t s = (int32_t)wakeChunk[i];
            sumSq += (int64_t)s * s;
          }
          int32_t rms = (sumSq > 0) ? (int32_t)sqrt((double)sumSq / n) : 0;

          if (rms > VAD_SILENCE_THRESHOLD) {
            if (!speechStarted) speechStartMs = now;
            speechStarted = true;
            lastSpeechMs = now;
            if (recordLen + n <= RECORD_BUF_MAX_SAMPLES) {
              memcpy(recordBuf + recordLen, wakeChunk, n * sizeof(int16_t));
              recordLen += n;
            }
          }

          bool minSpeechMet = speechStarted && (now - speechStartMs >= VAD_MIN_SPEECH_MS);
          bool pauseLongEnough = (now - lastSpeechMs >= VAD_PAUSE_MS);
          bool bufferFull = (recordLen >= RECORD_BUF_MAX_SAMPLES);

          if ((minSpeechMet && pauseLongEnough) || bufferFull) {
            if (recordLen < 1600) {  // < 100ms - likely noise
              g_state = IDLE;
              oled.setState(OLEDEngine::EYES_IDLE);
            } else {
              g_state = THINKING;
              oled.setState(OLEDEngine::EYES_THINKING);
              Serial.println(">> THINKING");
            }
          }
        }
      }
      break;

    case THINKING:
      {
        // Build WAV header and send to STT
        size_t wavSize = 44 + recordLen * 2;
        uint8_t* wavData = (uint8_t*)heap_caps_malloc(wavSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!wavData) wavData = (uint8_t*)malloc(wavSize); // Fallback

        if (wavData) {
          // Minimal WAV header for 16kHz mono 16-bit
          uint8_t hdr[] = {
            'R','I','F','F', 0,0,0,0, 'W','A','V','E', 'f','m','t',' ',
            16,0,0,0, 1,0, 1,0, 0x80,0x3E,0,0, 0,0x7D,0,0, 2,0, 16,0,
            'd','a','t','a', 0,0,0,0
          };
          uint32_t dataLen = recordLen * 2;
          uint32_t riffSize = 36 + dataLen;  // file size - 8
          memcpy(&hdr[4], &riffSize, 4);
          memcpy(&hdr[40], &dataLen, 4);
          memcpy(wavData, hdr, 44);
          memcpy(wavData + 44, recordBuf, recordLen * 2);

          String transcript = cloud.sendAudioToSTT(wavData, wavSize);
          free(wavData);

          if (transcript.length() > 0) {
            String learnName = parseLearnCommand(transcript);
            if (learnName.length() > 0) {
              // IR learning path — state is LEARN_IR, do NOT fall through to SPEAKING
              g_state = LEARN_IR;
              strncpy(_learnIrName, learnName.c_str(), 31);
              _learnIrName[31] = '\0';
              irEngine.startLearning();
              oled.setState(OLEDEngine::EYES_LISTENING);
              audio.playBeep(523, 100);
              audio.playBeep(659, 100);  // "ready to learn"
              Serial.println(">> LEARN_IR: Point remote at Mochi, press button for: " + learnName);
            } else {
              String cmd = parseIRCommand(transcript);
              String reply = cloud.getAIResponse(transcript);
              Serial.println("AI: " + reply);

              if (cmd.length() > 0) {
                uint16_t timings[256];
                size_t tc = irEngine.loadCode(cmd.c_str(), timings, 256);
                if (tc > 0) {
                  irEngine.transmitRaw(timings, tc);
                  // Custom spoken acknowledgement (e.g. "Your TV is on")
                  if (ttsBuffer && WiFi.status() == WL_CONNECTED && reply.length() > 0 && reply.length() < 100) {
                    size_t n = cloud.getTTS(reply, ttsBuffer, TTS_BUF_SIZE);
                    if (n > 0) audio.playWav(ttsBuffer, n);
                    else audio.playBeep(659, 60);
                  } else {
                    audio.playBeep(659, 60);
                  }
                  Serial.println("IR: " + cmd);
                } else {
                  if (ttsBuffer && WiFi.status() == WL_CONNECTED && reply.length() > 0 && reply.length() < 120) {
                    size_t n = cloud.getTTS(reply, ttsBuffer, TTS_BUF_SIZE);
                    if (n > 0) audio.playWav(ttsBuffer, n);
                    else audio.playBeep(200, 150);
                  } else {
                    audio.playBeep(200, 150);
                  }
                }
              }
              // FIX: SPEAKING only after AI/IR response, not when going to LEARN_IR
              g_state = SPEAKING;
              oled.setState(OLEDEngine::EYES_HAPPY);
              Serial.println(">> SPEAKING");
            }
          } else {
            // FIX: Empty transcript — STT found no speech, return to IDLE
            audio.playBeep(200, 100);
            g_state = IDLE;
            oled.setState(OLEDEngine::EYES_IDLE);
            Serial.println("STT: No speech detected, returning to IDLE.");
          }
        } else {
          Serial.println("Failed to allocate WAV buffer.");
          g_state = IDLE;
          oled.setState(OLEDEngine::EYES_IDLE);
        }
      }
      break;

    case SPEAKING:
      delay(2000);
      audio.playBeep(440, 50);  // done
      g_state = IDLE;
      oled.setState(OLEDEngine::EYES_IDLE);
      Serial.println(">> IDLE");
      break;

    case LEARN_IR: {
      static unsigned long learnStartMs = 0;
      if (learnStartMs == 0) learnStartMs = millis();

      if (irEngine.learnFromRemote(_learnIrName)) {
        audio.playBeep(880, 80);
        audio.playBeep(1100, 80);
        Serial.println("IR saved: " + String(_learnIrName));
        g_state = IDLE;
        oled.setState(OLEDEngine::EYES_IDLE);
        learnStartMs = 0;
      } else if (millis() - learnStartMs > 15000) {
        audio.playBeep(200, 200);
        Serial.println("Learn IR timeout.");
        g_state = IDLE;
        oled.setState(OLEDEngine::EYES_IDLE);
        learnStartMs = 0;
      }
      break;
    }
  }
  yield();
}

// "learn X" or "remember X" -> name for IR learning (e.g. "tv_on")
String parseLearnCommand(String utterance) {
  utterance.toLowerCase();
  int learnIdx    = utterance.indexOf("learn");
  int rememberIdx = utterance.indexOf("remember");
  if (learnIdx < 0 && rememberIdx < 0) return "";
  // FIX: was double-calling indexOf and had off-by-one (9 vs 8 for "remember")
  // "learn" = 5 chars, "remember" = 8 chars
  int start = (learnIdx >= 0) ? learnIdx + 5 : rememberIdx + 8;
  while (start < (int)utterance.length() && utterance[start] == ' ') start++;
  String name = utterance.substring(start);
  name.replace(" ", "_");
  name.replace(".", "");
  if (name.length() > 0 && name.length() < 32) return name;
  return "";
}

// "turn on tv" -> "tv_on", "volume up" -> "volume_up", or custom learned (e.g. "bedroom light" -> "bedroom_light")
String parseIRCommand(String utterance) {
  utterance.toLowerCase();
  utterance.trim();
  if (utterance.indexOf("tv") >= 0 || utterance.indexOf("television") >= 0) {
    if (utterance.indexOf("on") >= 0) return "tv_on";
    if (utterance.indexOf("off") >= 0) return "tv_off";
  }
  if (utterance.indexOf("volume up") >= 0) return "volume_up";
  if (utterance.indexOf("volume down") >= 0) return "volume_down";
  if (utterance.indexOf("channel") >= 0) return "channel_up";
  // Short phrase, likely device command - try as learned name (skip questions)
  if (utterance.startsWith("what ") || utterance.startsWith("how ") ||
      utterance.startsWith("why ") || utterance.startsWith("when ") ||
      utterance.startsWith("where ")) return "";
  int spaces = 0;
  for (unsigned int i = 0; i < utterance.length(); i++) if (utterance[i] == ' ') spaces++;
  if (spaces <= 3 && utterance.length() < 28) {
    String custom = utterance;
    custom.replace(" ", "_");
    custom.replace(".", "");
    if (custom.length() > 0) return custom;
  }
  return "";
}
