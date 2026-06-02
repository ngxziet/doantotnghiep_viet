#pragma once

#include <Arduino.h>
#include "../config.h"

struct Pose {
    float x;     // meters
    float y;     // meters
    float theta; // radians, normalized to [-π, π]
};

// Complementary filter weight: 0 = pure encoder, 1 = pure IMU.
// High alpha trusts IMU heading over encoder differential, prevents phantom
// rotation from intentional encoder asymmetry during heading correction.
static constexpr float ODOMETRY_ALPHA = 0.70f;

// Must match encoder constants
static constexpr int   ODO_PULSES_PER_REV  = RobotConfig::ENCODER_PULSES_PER_REV;
static constexpr float ODO_WHEEL_DIAM_CM   = RobotConfig::WHEEL_DIAM_CM;
static constexpr float ODO_WHEEL_BASE_CM   = RobotConfig::WHEEL_BASE_CM;

class Odometry {
public:
    explicit Odometry(float wheelBaseCm = ODO_WHEEL_BASE_CM);

    // leftPulses/rightPulses: absolute pulse counts from encoders
    // imuYawRad: current fused yaw from IMU (radians)
    // useImuHeading: false when IMU is unavailable, so encoder heading is used
    // dt: time step in seconds
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

    // Convert pulse delta to centimeters of wheel travel
    static float _pulsesToCm(long pulses);
};
