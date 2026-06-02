#pragma once

#include <Arduino.h>

// Distance thresholds (cm)
static constexpr float EMERGENCY_STOP_CM  = 10.0f;
static constexpr float WARN_THRESHOLD_CM  = 20.0f;
static constexpr float MAX_RANGE_CM       = 200.0f;

class UltrasonicHcsr04 {
public:
    UltrasonicHcsr04(int trigPin, int echoPin);

    void begin();

    // Returns median of 5 single readings (cm). Capped at MAX_RANGE_CM.
    float readDistanceCm();

    // true if last read distance < WARN_THRESHOLD_CM
    bool isObstacleNear();

    // true if last read distance < EMERGENCY_STOP_CM
    bool isEmergency();

private:
    int _trigPin;
    int _echoPin;
    float _lastDistCm = MAX_RANGE_CM;

    // Trigger one pulse and return raw distance in cm
    float _singleReadCm();

    // Sort 5-element float array in place; return element at index 2
    static float _median5(float arr[5]);
};
