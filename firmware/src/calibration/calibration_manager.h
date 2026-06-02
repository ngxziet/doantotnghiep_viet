#pragma once

#include <Arduino.h>
#include <Preferences.h>

class CalibrationManager {
public:
    void begin();
    void update();

    void resetMotorBalance();
    void resetEncoderScale();
    void markImuCalibrated(bool calibrated);

    void applyMotorTrim(int* leftPwm, int* rightPwm) const;
    void recordStraightSegment(long leftPulses, long rightPulses);

    bool isCalibrated() const;
    float leftTrim() const { return _leftTrim; }
    float rightTrim() const { return _rightTrim; }
    float encoderScale() const { return _encoderScale; }

private:
    Preferences _prefs;
    float _leftTrim = 1.0f;
    float _rightTrim = 1.0f;
    float _encoderScale = 1.0f;
    bool _imuCalibrated = false;
    bool _dirty = false;
    unsigned long _lastSaveMs = 0;

    void _save();
    static float _clampTrim(float value);
};