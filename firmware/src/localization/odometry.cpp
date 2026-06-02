#include "odometry.h"
#include <math.h>

Odometry::Odometry(float wheelBaseCm)
    : _wheelBaseCm(wheelBaseCm),
      _pose{0.0f, 0.0f, 0.0f}
{}

float Odometry::_normalizeAngle(float a) {
    if (!isfinite(a)) return 0.0f;
    while (a >  PI) a -= 2.0f * PI;
    while (a < -PI) a += 2.0f * PI;
    return a;
}

float Odometry::_pulsesToCm(long pulses) {
    return pulses * RobotConfig::CM_PER_PULSE;
}

void Odometry::update(long leftPulses, long rightPulses, float imuYawRad,
                      bool useImuHeading, bool suppressTranslation) {
    long deltaLeft  = leftPulses  - _prevLeftPulses;
    long deltaRight = rightPulses - _prevRightPulses;
    _prevLeftPulses  = leftPulses;
    _prevRightPulses = rightPulses;

    float dLeft  = _pulsesToCm(deltaLeft)  / 100.0f;
    float dRight = _pulsesToCm(deltaRight) / 100.0f;
    float dThetaEnc = (dRight - dLeft) / (_wheelBaseCm / 100.0f);

    if (suppressTranslation) {
        if (useImuHeading) {
            _pose.theta = _normalizeAngle(imuYawRad);
        } else if (deltaLeft != 0 || deltaRight != 0) {
            _pose.theta = _normalizeAngle(_pose.theta + dThetaEnc);
        }
        return;
    }

    float thetaEnc = _normalizeAngle(_pose.theta + dThetaEnc);
    float thetaFused = thetaEnc;
    if (useImuHeading) {
        float imuError = _normalizeAngle(imuYawRad - thetaEnc);
        thetaFused = _normalizeAngle(thetaEnc + ODOMETRY_ALPHA * imuError);
    }

    float d = (dLeft + dRight) / 2.0f;
    bool encoderLooksLikeSpin = (fabsf(d) < 0.015f) && (fabsf(dThetaEnc) > 0.03f);

    if (!encoderLooksLikeSpin) {
        float dThetaFused = _normalizeAngle(thetaFused - _pose.theta);
        float thetaMid = _normalizeAngle(_pose.theta + dThetaFused / 2.0f);
        _pose.x += d * cosf(thetaMid);
        _pose.y -= d * sinf(thetaMid);
    }

    _pose.theta = thetaFused;
}

void Odometry::reset(long leftPulses, long rightPulses) {
    _pose = {0.0f, 0.0f, 0.0f};
    syncPulses(leftPulses, rightPulses);
}

void Odometry::syncPulses(long leftPulses, long rightPulses) {
    _prevLeftPulses = leftPulses;
    _prevRightPulses = rightPulses;
}

void Odometry::setPose(float x, float y, float theta) {
    _pose.x = x;
    _pose.y = y;
    _pose.theta = _normalizeAngle(theta);
}
