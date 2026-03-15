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
      // Paper: ~15 blinks per minute = 4s average interval
      if (currentMillis - _lastBlinkTime > _blinkInterval) {
        _isBlinking = true;
        if (currentMillis - _lastBlinkTime > _blinkInterval + _blinkDuration) {
          _lastBlinkTime = currentMillis;
          _blinkInterval = 3600 + random(800); // 3.6–4.4 s
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
  unsigned long _blinkInterval = 3800; // ~15/min
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
        // Paper: wide-open, still eyes
        _display.fillCircle(leftEyeX, eyeY, eyeRadius, SSD1306_WHITE);
        _display.fillCircle(rightEyeX, eyeY, eyeRadius, SSD1306_WHITE);
        _display.fillCircle(leftEyeX + 2, eyeY - 2, 2, SSD1306_BLACK);
        _display.fillCircle(rightEyeX + 2, eyeY - 2, 2, SSD1306_BLACK);
        break;
      }

      case EYES_THINKING: {
        // Paper: eyes shifting side to side
        float t = millis() / 200.0f;
        int shift = (int)(sin(t) * 8);
        _display.fillCircle(leftEyeX + shift, eyeY, eyeRadius, SSD1306_WHITE);
        _display.fillCircle(rightEyeX + shift, eyeY, eyeRadius, SSD1306_WHITE);
        _display.fillCircle(leftEyeX + shift + 2, eyeY - 2, 2, SSD1306_BLACK);
        _display.fillCircle(rightEyeX + shift + 2, eyeY - 2, 2, SSD1306_BLACK);
        break;
      }

      case EYES_HAPPY: {
        // Paper: upward-arching eyes (engaged and positive)
        int arcY = eyeY + 4;
        _display.drawCircle(leftEyeX, arcY, eyeRadius, SSD1306_WHITE);
        _display.drawCircle(rightEyeX, arcY, eyeRadius, SSD1306_WHITE);
        _display.fillRect(leftEyeX - eyeRadius, arcY, eyeRadius * 2 + 1, eyeRadius + 1, SSD1306_BLACK);
        _display.fillRect(rightEyeX - eyeRadius, arcY, eyeRadius * 2 + 1, eyeRadius + 1, SSD1306_BLACK);
        break;
      }
    }
    
    _display.display();
  }
};

#endif
