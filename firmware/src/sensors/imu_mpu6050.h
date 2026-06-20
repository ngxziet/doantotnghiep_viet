#pragma once

#include <Arduino.h>
#include "../config.h"

class ImuMpu6050 {
public:
    ImuMpu6050() = default;

    // Khởi tạo I2C + MPU6050. Trả về false nếu không tìm thấy thiết bị.
    bool begin();

    // Đo bias gyro Z qua `samples` lần đọc khi xe đứng yên.
    void calibrate(int samples = 500);

    // Tích phân gyro Z để cập nhật yaw. Trả về false khi đọc thất bại.
    bool update();

    float getYawRad() const { return _yawRad; }
    float getYawDeg() const { return _yawRad * (180.0f / PI); }
    float getAbsoluteYawRad() const;
    float getAbsoluteYawDeg() const { return getAbsoluteYawRad() * (180.0f / PI); }
    bool isHealthy() const { return _healthy; }

    void setReferenceAngle();

    // Đồng bộ IMU: sau snap, getAbsoluteYawRad() trả về targetRad
    void setAbsoluteYaw(float targetRad);

    // Reset yaw tích lũy về 0
    void resetYaw();

    // --- Chẩn đoán nhiễu/bão hòa (debug) ---
    // Gọi lúc bắt đầu một cú quay để mở cửa sổ đo.
    void beginDiagWindow();
    // |gz| raw đỉnh trong cửa sổ (≥ ~32000 LSB = chạm trần ±250°/s → bão hòa).
    int peakAbsGzRaw() const { return _peakAbsGzRaw; }
    // Số lần đọc I2C lỗi kể từ beginDiagWindow().
    unsigned long readErrorsInWindow() const { return _totalReadErrors - _readErrAtWindowStart; }
    // IMU có tụt khỏi trạng thái healthy trong cửa sổ không.
    bool healthyDroppedInWindow() const { return _healthyDroppedInWindow; }

private:
    uint8_t _address = 0x68;

    float _gyroBiasZ  = 0.0f;  // Độ lệch bias gyro (đơn vị raw)
    float _yawRad     = 0.0f;  // Hướng tích lũy (radian)
    float _referenceYawRad = 0.0f;

    unsigned long _lastUpdateMs = 0;
    unsigned long _nextRetryMs = 0;
    int _consecutiveReadErrors = 0;
    bool _healthy = false;

    // --- Bộ đếm chẩn đoán (debug) ---
    int _peakAbsGzRaw = 0;                  // |gz| raw đỉnh trong cửa sổ đo
    unsigned long _totalReadErrors = 0;     // tổng số lần đọc I2C lỗi (tích lũy)
    unsigned long _readErrAtWindowStart = 0;
    bool _healthyDroppedInWindow = false;

    bool _writeByte(uint8_t reg, uint8_t value);
    bool _readBytes(uint8_t reg, uint8_t* data, uint8_t len);
    bool _readMotion6(int16_t* ax, int16_t* ay, int16_t* az,
                      int16_t* gx, int16_t* gy, int16_t* gz);

    // Hệ số chuyển đổi: gyro raw LSB → rad/s (±250°/s range)
    // 131 LSB mỗi °/s → 131 × (180/π) LSB mỗi rad/s
    static constexpr float GYRO_SCALE_RAD_S = 1.0f / (131.0f * (180.0f / PI));
    static constexpr int MAX_READ_ERRORS = 5;
    static constexpr unsigned long RETRY_INTERVAL_MS = 1000;
    static constexpr float MAX_DT_S = 0.2f;
    static constexpr float GYRO_DEADBAND_RAD_S = 0.01f;

    static float _normalizeAngle(float a);
};
