#pragma once

#include <Arduino.h>

// LEDC PWM configuration
static constexpr int   MOTOR_PWM_FREQ     = 5000; // Hz
static constexpr int   MOTOR_PWM_BITS     = 8;    // 8-bit → 0–255
static constexpr int   MOTOR_CH_LEFT      = 0;
static constexpr int   MOTOR_CH_RIGHT     = 1;

class MotorDriver {
public:
    // in1/in2/ena = left motor; in3/in4/enb = right motor
    MotorDriver(int in1, int in2, int ena,
                int in3, int in4, int enb);

    // Configure GPIO modes and attach LEDC PWM channels
    void begin();

    // speed: -255 (full backward) … 255 (full forward)
    void setLeft(int speed);
    void setRight(int speed);
    int getLeftSpeed() const { return _leftSpeed; }
    int getRightSpeed() const { return _rightSpeed; }

    // Slew-rate limited setters — call each loop (50ms)
    void setLeftSlew(int target, int maxStep = 30);
    void setRightSlew(int target, int maxStep = 30);

    void stop();
    void forward(int speed);   // both wheels forward at same speed
    void backward(int speed);  // both wheels backward at same speed

private:
    int _in1, _in2, _ena;
    int _in3, _in4, _enb;
    int _leftSpeed;
    int _rightSpeed;

    // Apply direction pins + PWM duty for one side
    void _applyLeft(int speed);
    void _applyRight(int speed);

    static int _clamp(int v, int lo, int hi);
};
