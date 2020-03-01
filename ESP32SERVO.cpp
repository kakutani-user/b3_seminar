#include "ESP32SERVO.h"
#include <M5Stack.h>

int prevch; // 前に制御したチャネル

int ESP32SERVO::begin(int pin, int ch) {
    _ch = ch;
    ledcSetup(_ch, 50, 10);
    ledcAttachPin(pin, _ch);
    ESP32SERVO::write(0);
    return 1;
}

void ESP32SERVO::write(int angle) {
    if (prevch != _ch) { // 前と違うチャネルを制御するときは
        delay(50);       // 50ms待つ
    }
    prevch = _ch;
    ledcWrite( _ch, map(constrain(angle, -90, 90), -90, 90, _min, _max) );
    _angle = constrain(angle, -90, 90);
    Serial.printf("%dch: %d\n", _ch, _angle);
}

void ESP32SERVO::move(int angle) {
    ESP32SERVO::write(_angle + angle);
}
