#pragma once

#include <Arduino.h>

// Cấu hình LEDC PWM cho ESP32
static constexpr int   MOTOR_PWM_FREQ     = 5000; // Hz
static constexpr int   MOTOR_PWM_BITS     = 8;    // 8-bit → 0–255
static constexpr int   MOTOR_CH_LEFT      = 0;
static constexpr int   MOTOR_CH_RIGHT     = 1;

class MotorDriver {
public:
    // in1/in2/ena = motor trái; in3/in4/enb = motor phải
    MotorDriver(int in1, int in2, int ena,
                int in3, int in4, int enb);

    // Cấu hình chân GPIO và gắn kênh LEDC PWM
    void begin();

    // speed: -255 (lùi tối đa) … 255 (tiến tối đa)
    void setLeft(int speed);
    void setRight(int speed);
    int getLeftSpeed() const { return _leftSpeed; }
    int getRightSpeed() const { return _rightSpeed; }

    // Tăng/giảm tốc từ từ (slew rate) — gọi mỗi loop (20ms)
    void setLeftSlew(int target, int maxStep = 30);
    void setRightSlew(int target, int maxStep = 30);

    void stop();
    void forward(int speed);   // cả 2 bánh tiến cùng tốc độ
    void backward(int speed);  // cả 2 bánh lùi cùng tốc độ

private:
    int _in1, _in2, _ena;
    int _in3, _in4, _enb;
    int _leftSpeed;
    int _rightSpeed;

    // Áp dụng chân hướng + duty PWM cho 1 bên
    void _applyLeft(int speed);
    void _applyRight(int speed);

    static int _clamp(int v, int lo, int hi);
};
