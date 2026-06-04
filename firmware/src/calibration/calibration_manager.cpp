#include "calibration_manager.h"
#include "../config.h"
#include <math.h>

void CalibrationManager::begin() {
    _prefs.begin("robot-cal", false);

    int storedVersion = _prefs.getInt("ver", 0);
    if (storedVersion != RobotConfig::CONFIG_VERSION) {
        Serial.printf("Config version changed (%d -> %d), resetting calibration\n",
                      storedVersion, RobotConfig::CONFIG_VERSION);
        _prefs.clear();
        _prefs.putInt("ver", RobotConfig::CONFIG_VERSION);
    }

    _leftTrim = _prefs.getFloat("left", 1.0f);
    _rightTrim = _prefs.getFloat("right", 1.0f);
    _encoderScale = _prefs.getFloat("enc", 1.0f);
    _imuCalibrated = _prefs.getBool("imu", false);
    _leftTrim = _clampTrim(_leftTrim);
    _rightTrim = _clampTrim(_rightTrim);
    if (_encoderScale < 0.80f || _encoderScale > 1.20f) {
        _encoderScale = 1.0f;
    }
}

void CalibrationManager::update() {
    if (_dirty && millis() - _lastSaveMs >= 15000) {
        _save();
    }
}

void CalibrationManager::resetMotorBalance() {
    _leftTrim = 1.0f;
    _rightTrim = 1.0f;
    _dirty = true;
    _save();
}

void CalibrationManager::resetEncoderScale() {
    _encoderScale = 1.0f;
    _dirty = true;
    _save();
}

void CalibrationManager::markImuCalibrated(bool calibrated) {
    _imuCalibrated = calibrated;
    _dirty = true;
    _save();
}

void CalibrationManager::applyMotorTrim(int* leftPwm, int* rightPwm) const {
    if (!leftPwm || !rightPwm) return;

    int leftSign = *leftPwm < 0 ? -1 : 1;
    int rightSign = *rightPwm < 0 ? -1 : 1;
    int left = (int)(fabsf((float)*leftPwm) * _leftTrim);
    int right = (int)(fabsf((float)*rightPwm) * _rightTrim);
    *leftPwm = constrain(left * leftSign, -255, 255);
    *rightPwm = constrain(right * rightSign, -255, 255);
}

void CalibrationManager::recordStraightSegment(long leftPulses, long rightPulses) {
    leftPulses = labs(leftPulses);
    rightPulses = labs(rightPulses);
    if (leftPulses < RobotConfig::PULSES_PER_NODE / 2 ||
        rightPulses < RobotConfig::PULSES_PER_NODE / 2) {
        return;
    }

    float average = (leftPulses + rightPulses) * 0.5f;
    if (average <= 0.0f) return;

    float error = (rightPulses - leftPulses) / average;
    if (fabsf(error) > 0.35f) {
        return;
    }

    // Tốc độ học: 5% mỗi đoạn thẳng (trước đây 1.8%, quá chậm)
    _leftTrim = _clampTrim(_leftTrim + error * 0.05f);
    _rightTrim = _clampTrim(_rightTrim - error * 0.05f);

    float observedScale = RobotConfig::PULSES_PER_NODE / average;
    if (observedScale > 0.80f && observedScale < 1.20f) {
        _encoderScale = _encoderScale * 0.94f + observedScale * 0.06f;
    }

    _dirty = true;
}

bool CalibrationManager::isCalibrated() const {
    return _imuCalibrated;
}

void CalibrationManager::_save() {
    _prefs.putFloat("left", _leftTrim);
    _prefs.putFloat("right", _rightTrim);
    _prefs.putFloat("enc", _encoderScale);
    _prefs.putBool("imu", _imuCalibrated);
    _dirty = false;
    _lastSaveMs = millis();
}

float CalibrationManager::_clampTrim(float value) {
    if (value < 0.72f) return 0.72f;
    if (value > 1.28f) return 1.28f;
    return value;
}