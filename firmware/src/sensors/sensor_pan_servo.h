#pragma once

#include <Arduino.h>
#include "../config.h"

// Điều khiển servo quay cảm biến HC-SR04 sang trái/phải.
// Dùng LEDC thô 50Hz trên kênh riêng (SERVO_PWM_CH) để không đụng kênh PWM motor (0,1).
class SensorPanServo {
public:
    explicit SensorPanServo(int pin);

    // Cấu hình kênh LEDC + gắn chân, đưa servo về vị trí nhìn thẳng.
    void begin();

    void center();    // nhìn thẳng
    void lookLeft();  // nhìn sang trái
    void lookRight(); // nhìn sang phải

    void setAngle(int deg);             // 0..180°
    int currentAngle() const { return _angle; }

private:
    int _pin;
    int _angle;

    // Quy đổi góc → độ rộng xung → duty LEDC và xuất ra chân
    void _writeAngle(int deg);
};
