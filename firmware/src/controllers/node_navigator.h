#pragma once

#include <Arduino.h>
#include "../config.h"
#include "motor_driver.h"
#include "../sensors/encoder.h"
#include "../localization/odometry.h"
#include "../calibration/calibration_manager.h"

class NodeNavigator {
public:
    void begin();
    void setRoute(const float xs[], const float ys[], int count, const Pose& currentPose,
                  Encoder& leftEncoder, Encoder& rightEncoder);
    void setStepDrive(const Pose& currentPose, Encoder& leftEncoder, Encoder& rightEncoder);
    void setStepTurn(float targetHeading, float currentHeading);
    void clear();

    void update(const Pose& pose, MotorDriver& motors,
                Encoder& leftEncoder, Encoder& rightEncoder,
                CalibrationManager& calibration, bool imuAvailable,
                float imuHeadingRad = 0.0f);

    bool isActive() const;
    bool isDrivingStraight() const;
    bool isRotating() const;
    bool isStep() const { return _isStep; }
    bool hasSnap() const { return _snapPending; }
    void acknowledgeSnap() { _snapPending = false; }

    float snapX() const { return _snapX; }
    float snapY() const { return _snapY; }
    float snapTheta() const { return _snapTheta; }

    int currentIndex() const { return _currentIndex; }
    int waypointCount() const { return _count; }
    const char* status() const;

private:
    enum class State {
        Idle,
        FastRotate,   // Xoay nhanh liên tục (ước lượng thời gian)
        Rotate,       // Xoay nudge (burst ngắn → đo → lặp)
        RotateSettle, // Chờ ổn định sau burst/xoay nhanh
        Drive,
        NodePause,
        Arrived,
        Fault
    };

    State _state = State::Idle;
    float _xs[RobotConfig::ROUTE_MAX_WAYPOINTS] = {};
    float _ys[RobotConfig::ROUTE_MAX_WAYPOINTS] = {};
    int _count = 0;
    int _currentIndex = 0;

    float _targetHeading = 0.0f;
    int _targetPulses = RobotConfig::PULSES_PER_NODE;
    bool _rotateOnly = false;
    bool _isStep = false;
    long _driveStartLeft = 0;
    long _driveStartRight = 0;
    unsigned long _stateStartMs = 0;
    unsigned long _pauseUntilMs = 0;
    unsigned long _nudgeBurstStartMs = 0;
    int _nudgePwm = RobotConfig::NUDGE_ROTATE_PWM;
    float _headingBeforeNudge = 0.0f;
    unsigned long _currentBurstMs = 35;
    float _initialRotateError = 0.0f;
    float _totalRotationAccum = 0.0f;
    unsigned long _fastRotateUntilMs = 0;
    int _fastRotateDir = 0;
    int _fastRotatePwm = 0;
    unsigned long _preRotateUntilMs = 0;  // mốc chờ settle trước khi bắt đầu xoay (0 = không chờ)

    bool _snapPending = false;
    float _snapX = 0.0f;
    float _snapY = 0.0f;
    float _snapTheta = 0.0f;
    char _faultStatus[24] = "idle";

    int _beepsRemaining = 0;
    bool _buzzerOn = false;
    unsigned long _nextBuzzerMs = 0;

    void _beginRotateOrDrive(const Pose& pose,
                             Encoder& leftEncoder, Encoder& rightEncoder);
    void _beginRotateTo(float targetHeading, float currentHeading);
    void _beginDrive(Encoder& leftEncoder, Encoder& rightEncoder);
    void _handleFastRotate(const Pose& pose, MotorDriver& motors);
    void _handleRotate(const Pose& pose, MotorDriver& motors);
    void _handleRotateSettle(const Pose& pose, MotorDriver& motors,
                             Encoder& leftEncoder, Encoder& rightEncoder);
    void _handleDrive(const Pose& pose, MotorDriver& motors,
                      Encoder& leftEncoder, Encoder& rightEncoder,
                      CalibrationManager& calibration, bool imuAvailable,
                      float imuHeadingRad);
    void _handleNodePause(const Pose& pose, MotorDriver& motors,
                          Encoder& leftEncoder, Encoder& rightEncoder);

    void _scheduleBeeps(int count);
    void _updateBuzzer();
    void _setBuzzer(bool on);
    void _setFault(const char* status, MotorDriver& motors);

    static float _normalizeAngle(float angle);
    static float _targetHeadingBetween(float x0, float y0, float x1, float y1);
    static int _segmentPulses(float x0, float y0, float x1, float y1);
    static long _absDelta(long current, long start);
};