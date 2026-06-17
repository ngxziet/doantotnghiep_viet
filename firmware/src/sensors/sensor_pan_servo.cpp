#include "sensor_pan_servo.h"

SensorPanServo::SensorPanServo(int pin)
    : _pin(pin), _angle(RobotConfig::SERVO_CENTER_DEG)
{}

void SensorPanServo::begin() {
    ledcSetup(RobotConfig::SERVO_PWM_CH,
              RobotConfig::SERVO_PWM_FREQ,
              RobotConfig::SERVO_PWM_BITS);
    ledcAttachPin(_pin, RobotConfig::SERVO_PWM_CH);
    center();
}

void SensorPanServo::_writeAngle(int deg) {
    deg = constrain(deg, 0, 180);
    _angle = deg;

    // Góc → độ rộng xung (µs) trong dải [MIN_PULSE, MAX_PULSE]
    long span = RobotConfig::SERVO_MAX_PULSE_US - RobotConfig::SERVO_MIN_PULSE_US;
    long pulseUs = RobotConfig::SERVO_MIN_PULSE_US + (span * deg) / 180;

    // Độ rộng xung → duty: duty = pulseUs / chu_kỳ_µs * (2^bits - 1)
    long maxDuty = (1L << RobotConfig::SERVO_PWM_BITS) - 1;
    long periodUs = 1000000L / RobotConfig::SERVO_PWM_FREQ;  // 50Hz → 20000µs
    long duty = (pulseUs * maxDuty) / periodUs;

    ledcWrite(RobotConfig::SERVO_PWM_CH, (uint32_t)duty);
}

void SensorPanServo::setAngle(int deg) { _writeAngle(deg); }
void SensorPanServo::center()    { _writeAngle(RobotConfig::SERVO_CENTER_DEG); }
void SensorPanServo::lookLeft()  { _writeAngle(RobotConfig::SERVO_LEFT_DEG); }
void SensorPanServo::lookRight() { _writeAngle(RobotConfig::SERVO_RIGHT_DEG); }
