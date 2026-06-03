#pragma once

#include <Arduino.h>
#include "../config.h"

struct Pose {
    float x;     // meters
    float y;     // meters
    float theta; // radians, normalized to [-π, π]
};

// Trọng số bộ lọc bù: 0 = chỉ encoder, 1 = chỉ IMU.
// Khi đi thẳng, heading bị ghi đè bằng IMU thuần trong main.cpp,
// nên alpha này chỉ áp dụng cho trạng thái không lái (thủ công, chờ).
static constexpr float ODOMETRY_ALPHA = 0.35f;

// Phải khớp với hằng số encoder
static constexpr int   ODO_PULSES_PER_REV  = RobotConfig::ENCODER_PULSES_PER_REV;
static constexpr float ODO_WHEEL_DIAM_CM   = RobotConfig::WHEEL_DIAM_CM;
static constexpr float ODO_WHEEL_BASE_CM   = RobotConfig::WHEEL_BASE_CM;

class Odometry {
public:
    explicit Odometry(float wheelBaseCm = ODO_WHEEL_BASE_CM);

    // leftPulses/rightPulses: số xung tuyệt đối từ encoder
    // imuYawRad: góc yaw hiện tại từ IMU (radian)
    // useImuHeading: false khi IMU không khả dụng, dùng heading encoder thay thế
    // suppressTranslation: true khi xoay tại chỗ, chỉ cập nhật theta
    void update(long leftPulses, long rightPulses, float imuYawRad,
                bool useImuHeading = true, bool suppressTranslation = false);

    Pose getPose() const { return _pose; }

    void reset(long leftPulses = 0, long rightPulses = 0);
    void syncPulses(long leftPulses, long rightPulses);
    void setPose(float x, float y, float theta);

private:
    float _wheelBaseCm;
    Pose  _pose;

    long  _prevLeftPulses  = 0;
    long  _prevRightPulses = 0;

    static float _normalizeAngle(float a);

    // Chuyển đổi số xung sang cm quãng đường bánh xe
    static float _pulsesToCm(long pulses);
};
