#include "motor_driver.h"

MotorDriver::MotorDriver(int in1, int in2, int ena,
                         int in3, int in4, int enb)
    : _in1(in1), _in2(in2), _ena(ena),
      _in3(in3), _in4(in4), _enb(enb),
      _leftSpeed(0), _rightSpeed(0)
{}

void MotorDriver::begin() {
    pinMode(_in1, OUTPUT);
    pinMode(_in2, OUTPUT);
    pinMode(_in3, OUTPUT);
    pinMode(_in4, OUTPUT);

    // Cấu hình kênh LEDC PWM cho chân enable
    ledcSetup(MOTOR_CH_LEFT,  MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
    ledcSetup(MOTOR_CH_RIGHT, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
    ledcAttachPin(_ena, MOTOR_CH_LEFT);
    ledcAttachPin(_enb, MOTOR_CH_RIGHT);

    stop();
}

int MotorDriver::_clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void MotorDriver::_applyLeft(int speed) {
    speed = _clamp(speed, -255, 255);
    _leftSpeed = speed;
    if (speed > 0) {
        digitalWrite(_in1, HIGH);
        digitalWrite(_in2, LOW);
    } else if (speed < 0) {
        digitalWrite(_in1, LOW);
        digitalWrite(_in2, HIGH);
    } else {
        digitalWrite(_in1, LOW);
        digitalWrite(_in2, LOW);
    }
    ledcWrite(MOTOR_CH_LEFT, abs(speed));
}

void MotorDriver::_applyRight(int speed) {
    speed = _clamp(speed, -255, 255);
    _rightSpeed = speed;
    if (speed > 0) {
        digitalWrite(_in3, HIGH);
        digitalWrite(_in4, LOW);
    } else if (speed < 0) {
        digitalWrite(_in3, LOW);
        digitalWrite(_in4, HIGH);
    } else {
        digitalWrite(_in3, LOW);
        digitalWrite(_in4, LOW);
    }
    ledcWrite(MOTOR_CH_RIGHT, abs(speed));
}

void MotorDriver::setLeft(int speed)  { _applyLeft(speed);  }
void MotorDriver::setRight(int speed) { _applyRight(speed); }

void MotorDriver::stop() {
    _applyLeft(0);
    _applyRight(0);
}

void MotorDriver::forward(int speed) {
    speed = _clamp(speed, 0, 255);
    _applyLeft(speed);
    _applyRight(speed);
}

void MotorDriver::backward(int speed) {
    speed = _clamp(speed, 0, 255);
    _applyLeft(-speed);
    _applyRight(-speed);
}

void MotorDriver::setLeftSlew(int target, int maxStep) {
    int step = _clamp(target - _leftSpeed, -maxStep, maxStep);
    _applyLeft(_leftSpeed + step);
}

void MotorDriver::setRightSlew(int target, int maxStep) {
    int step = _clamp(target - _rightSpeed, -maxStep, maxStep);
    _applyRight(_rightSpeed + step);
}
