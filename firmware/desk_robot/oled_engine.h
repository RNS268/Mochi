#ifndef OLED_ENGINE_H
#define OLED_ENGINE_H

#include <Adafruit_SSD1306.h>

class OLEDEngine {
public:
  enum AnimationState {
    EYES_IDLE,
    EYES_LISTENING,
    EYES_THINKING,
    EYES_HAPPY
  };

  OLEDEngine(Adafruit_SSD1306& display) : _display(display) {}

  void setState(AnimationState state) {
    _state = state;
  }

  void update() {
    unsigned long currentMillis = millis();
    
    if (_state == EYES_IDLE) {
      // Blink logic
      if (currentMillis - _lastBlinkTime > _blinkInterval) {
        _isBlinking = true;
        if (currentMillis - _lastBlinkTime > _blinkInterval + _blinkDuration) {
          _lastBlinkTime = currentMillis;
          _blinkInterval = random(2000, 6000);
          _isBlinking = false;
        }
      }
    }

    render();
  }

private:
  Adafruit_SSD1306& _display;
  AnimationState _state = EYES_IDLE;
  unsigned long _lastBlinkTime = 0;
  unsigned long _blinkInterval = 3000;
  unsigned long _blinkDuration = 150;
  bool _isBlinking = false;

  void render() {
    _display.clearDisplay();
    
    int eyeY = 32;
    int leftEyeX = 40;
    int rightEyeX = 88;
    int eyeRadius = 12;

    switch (_state) {
      case EYES_IDLE:
        if (_isBlinking) {
          _display.drawFastHLine(leftEyeX - eyeRadius, eyeY, eyeRadius * 2, SSD1306_WHITE);
          _display.drawFastHLine(rightEyeX - eyeRadius, eyeY, eyeRadius * 2, SSD1306_WHITE);
        } else {
          _display.fillCircle(leftEyeX, eyeY, eyeRadius, SSD1306_WHITE);
          _display.fillCircle(rightEyeX, eyeY, eyeRadius, SSD1306_WHITE);
          _display.fillCircle(leftEyeX + 3, eyeY - 3, 3, SSD1306_BLACK);
          _display.fillCircle(rightEyeX + 3, eyeY - 3, 3, SSD1306_BLACK);
        }
        break;

      case EYES_LISTENING: {
        // Pulse effect
        int pulse = (sin(millis() / 200.0) * 4) + 10;
        _display.fillCircle(leftEyeX, eyeY, pulse, SSD1306_WHITE);
        _display.fillCircle(rightEyeX, eyeY, pulse, SSD1306_WHITE);
        break;
      }

      case EYES_THINKING: {
        // Spinning dots
        float angle = millis() / 150.0;
        for (int i=0; i<3; i++) {
          int xOffset = cos(angle + i*2.09) * 15;
          int yOffset = sin(angle + i*2.09) * 15;
          _display.fillCircle(64 + xOffset, 32 + yOffset, 4, SSD1306_WHITE);
        }
        break;
      }

      case EYES_HAPPY:
        // Use semi-circles for happy eyes
        _display.drawCircle(leftEyeX, eyeY + 5, eyeRadius, SSD1306_WHITE);
        _display.drawCircle(rightEyeX, eyeY + 5, eyeRadius, SSD1306_WHITE);
        _display.fillRect(leftEyeX - eyeRadius, eyeY + 5, eyeRadius * 2 + 1, eyeRadius + 1, SSD1306_BLACK);
        break;
    }
    
    _display.display();
  }
};

#endif
