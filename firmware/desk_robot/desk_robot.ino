#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "config.h"

#include "cloud_client.h"
#include "wake_word_engine.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
OLEDEngine oled(display);
AudioPipeline audio;
IREngine ir;
CloudClient cloud;
WakeWordEngine wake;

enum State {
  IDLE,
  LISTENING,
  THINKING,
  SPEAKING
};

State currentState = IDLE;

// Buffers and globals
uint8_t audioBuffer[32000]; // 1 second of 16kHz 16bit mono
size_t audioSize = 0;

void setup() {
  Serial.begin(115200);

  // Initialize OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(F("Mochi Booting..."));
  display.display();

  // Initialize WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  display.println(F("WiFi Connected!"));
  display.display();
  
  audio.begin();
  ir.begin();
  wake.begin();
  delay(1000);
}

void loop() {
  oled.update();

  switch(currentState) {
    case IDLE:
      oled.setState(OLEDEngine::EYES_IDLE);
      
      // Local Wake-Word Monitoring
      int16_t chunk[512];
      size_t read = audio.readChunk(chunk, 512);
      if (read > 0) {
        if (wake.listen(chunk, read)) {
          currentState = LISTENING;
        }
      }

      // Keep serial fallback for development
      if (Serial.available()) {
        char c = Serial.read();
        if (c == 'l') currentState = LISTENING;
      }
      break;

    case LISTENING:
      oled.setState(OLEDEngine::EYES_LISTENING);
      Serial.println("Listening...");
      // Fill audio buffer (simplified placeholder for 1s capture)
      // In production, use DMA and DMA interrupt to fill PSRAM
      for (int i=0; i<100; i++) {
        // audio.capture(audioBuffer + audioSize); // hypothetical
      }
      currentState = THINKING;
      break;

    case THINKING:
      oled.setState(OLEDEngine::EYES_THINKING);
      Serial.println("Thinking...");
      {
        String transcript = "Turn on the TV"; // Simplified for demonstration
        String response = cloud.getAIResponse(transcript);
        Serial.println("Mochi: " + response);
        
        // IR Trigger Logic based on AI response
        if (response.indexOf("TV") != -1 && response.indexOf("on") != -1) {
           uint16_t tv_on_timings[] = {4500, 4500, 500, 1600...}; // Simplified raw timings
           ir.transmitRaw(tv_on_timings, 4); 
           oled.setState(OLEDEngine::EYES_HAPPY);
           delay(1000);
        }
      }
      currentState = IDLE;
      break;

    case SPEAKING:
      // Stream audio to I2S
      currentState = IDLE;
      break;
  }
}
