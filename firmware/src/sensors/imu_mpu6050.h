#pragma once

#include <Arduino.h>
#include "../config.h"

class ImuMpu6050 {
public:
    ImuMpu6050() = default;

    // Initialize Wire + MPU6050. Returns false if device not found.
    bool begin();

    // Measure gyro Z bias over `samples` readings while the vehicle is at rest.
    void calibrate(int samples = 500);

    // Integrate gyro Z to update yaw. Returns false when the latest read failed.
    bool update();

    float getYawRad() const { return _yawRad; }
    float getYawDeg() const { return _yawRad * (180.0f / PI); }
    float getAbsoluteYawRad() const;
    float getAbsoluteYawDeg() const { return getAbsoluteYawRad() * (180.0f / PI); }
    bool isHealthy() const { return _healthy; }

    void setReferenceAngle();

    // Reset accumulated yaw to zero
    void resetYaw();

private:
    uint8_t _address = 0x68;

    float _gyroBiasZ  = 0.0f;  // Bias offset in raw units
    float _yawRad     = 0.0f;  // Accumulated heading in radians
    float _referenceYawRad = 0.0f;

    unsigned long _lastUpdateMs = 0;
    unsigned long _nextRetryMs = 0;
    int _consecutiveReadErrors = 0;
    bool _healthy = false;

    bool _writeByte(uint8_t reg, uint8_t value);
    bool _readBytes(uint8_t reg, uint8_t* data, uint8_t len);
    bool _readMotion6(int16_t* ax, int16_t* ay, int16_t* az,
                      int16_t* gx, int16_t* gy, int16_t* gz);

    // Scale factor: raw gyro LSB → rad/s (MPU6050 default ±250°/s range)
    // 131 LSB per °/s → 131 * (180/π) LSB per rad/s
    static constexpr float GYRO_SCALE_RAD_S = 1.0f / (131.0f * (180.0f / PI));
    static constexpr int MAX_READ_ERRORS = 5;
    static constexpr unsigned long RETRY_INTERVAL_MS = 1000;
    static constexpr float MAX_DT_S = 0.2f;
    static constexpr float GYRO_DEADBAND_RAD_S = 0.01f;

    static float _normalizeAngle(float a);
};
