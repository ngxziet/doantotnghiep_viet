#include "node_navigator.h"
#include <math.h>
#include <string.h>

void NodeNavigator::begin() {
    pinMode(RobotConfig::BUZZER_PIN, OUTPUT);
    _setBuzzer(false);
}

void NodeNavigator::setRoute(const float xs[], const float ys[], int count,
                             const Pose& currentPose,
                             Encoder& leftEncoder, Encoder& rightEncoder) {
    clear();
    _rotateOnly = false;
    int limit = constrain(count, 0, RobotConfig::ROUTE_MAX_WAYPOINTS);

    for (int i = 0; i < limit; i++) {
        float dx = xs[i] - currentPose.x;
        float dy = ys[i] - currentPose.y;
        if (_count == 0 && sqrtf(dx * dx + dy * dy) < 0.05f) {
            continue;
        }
        _xs[_count] = xs[i];
        _ys[_count] = ys[i];
        _count++;
    }

    if (_count == 0) {
        _state = State::Arrived;
        _currentIndex = 0;
        _scheduleBeeps(3);
        return;
    }

    _currentIndex = 0;
    _beginRotateOrDrive(currentPose, leftEncoder, rightEncoder);
}

void NodeNavigator::setStepDrive(const Pose& currentPose,
                                 Encoder& leftEncoder,
                                 Encoder& rightEncoder) {
    clear();
    _rotateOnly = false;
    _isStep = true;
    _count = 1;
    _currentIndex = 0;
    _targetHeading = _normalizeAngle(currentPose.theta);
    _targetPulses = RobotConfig::PULSES_PER_NODE;
    _xs[0] = currentPose.x + RobotConfig::NODE_DISTANCE_M * cosf(_targetHeading);
    _ys[0] = currentPose.y - RobotConfig::NODE_DISTANCE_M * sinf(_targetHeading);
    _beginDrive(leftEncoder, rightEncoder);
}

void NodeNavigator::setStepTurn(float targetHeading) {
    clear();
    _rotateOnly = true;
    _isStep = true;
    _count = 1;
    _currentIndex = 0;
    _targetHeading = _normalizeAngle(targetHeading);
    _targetPulses = 0;
    _stateStartMs = millis();
    _nudgeBurstStartMs = millis();
    _nudgePwm = RobotConfig::NUDGE_ROTATE_PWM;
    _state = State::Rotate;
}

void NodeNavigator::clear() {
    _state = State::Idle;
    _count = 0;
    _currentIndex = 0;
    _rotateOnly = false;
    _isStep = false;
    _snapPending = false;
    _nudgeBurstStartMs = 0;
    _faultStatus[0] = '\0';
}

void NodeNavigator::update(const Pose& pose, MotorDriver& motors,
                           Encoder& leftEncoder, Encoder& rightEncoder,
                           CalibrationManager& calibration, bool imuAvailable,
                           float imuHeadingRad) {
    _updateBuzzer();

    switch (_state) {
        case State::Idle:
        case State::Arrived:
        case State::Fault:
            return;
        case State::Rotate:
            _handleRotate(pose, motors);
            return;
        case State::RotateSettle:
            _handleRotateSettle(pose, motors, leftEncoder, rightEncoder);
            return;
        case State::Drive:
            _handleDrive(pose, motors, leftEncoder, rightEncoder,
                         calibration, imuAvailable, imuHeadingRad);
            return;
        case State::NodePause:
            _handleNodePause(pose, motors, leftEncoder, rightEncoder);
            return;
    }
}

bool NodeNavigator::isActive() const {
    return _state == State::Rotate || _state == State::RotateSettle ||
           _state == State::Drive || _state == State::NodePause;
}

bool NodeNavigator::isDrivingStraight() const {
    return _state == State::Drive;
}

bool NodeNavigator::isRotating() const {
    return _state == State::Rotate || _state == State::RotateSettle;
}

const char* NodeNavigator::status() const {
    switch (_state) {
        case State::Idle: return "idle";
        case State::Arrived: return "arrived";
        case State::Fault: return _faultStatus[0] ? _faultStatus : "fault";
        case State::Rotate:
        case State::RotateSettle:
        case State::Drive:
        case State::NodePause:
            return "moving";
    }
    return "idle";
}

void NodeNavigator::_beginRotateOrDrive(const Pose& pose,
                                        Encoder& leftEncoder,
                                        Encoder& rightEncoder) {
    if (_currentIndex >= _count) {
        _state = State::Arrived;
        _scheduleBeeps(3);
        return;
    }

    _targetHeading = _targetHeadingBetween(pose.x, pose.y,
                                           _xs[_currentIndex], _ys[_currentIndex]);
    _targetPulses = _segmentPulses(pose.x, pose.y,
                                   _xs[_currentIndex], _ys[_currentIndex]);

    // Lệch < 10°: đi thẳng luôn, heading correction tự điều chỉnh
    float headingError = fabsf(_normalizeAngle(_targetHeading - pose.theta));
    if (headingError < RobotConfig::SKIP_ROTATE_THRESHOLD_RAD) {
        _beginDrive(leftEncoder, rightEncoder);
        return;
    }

    _stateStartMs = millis();
    _nudgeBurstStartMs = millis();
    _nudgePwm = RobotConfig::NUDGE_ROTATE_PWM;
    _state = State::Rotate;
}

void NodeNavigator::_beginDrive(Encoder& leftEncoder, Encoder& rightEncoder) {
    _driveStartLeft = leftEncoder.getPulseCount();
    _driveStartRight = rightEncoder.getPulseCount();
    _stateStartMs = millis();
    _state = State::Drive;
}

// Nudge rotation: fixed burst → stop → measure → repeat (all rotation)
void NodeNavigator::_handleRotate(const Pose& pose, MotorDriver& motors) {
    float error = _normalizeAngle(_targetHeading - pose.theta);
    float absError = fabsf(error);

    // Within tolerance — stop and confirm via settle
    if (absError <= RobotConfig::NUDGE_DONE_TOL_RAD) {
        motors.stop();
        _state = State::RotateSettle;
        _pauseUntilMs = millis() + RobotConfig::NUDGE_SETTLE_MS;
        return;
    }

    // Overall timeout
    if (millis() - _stateStartMs > RobotConfig::ROTATE_TIMEOUT_MS) {
        _setFault("rotation_timeout", motors);
        return;
    }

    // Nudge burst done — stop and settle before measuring
    if (millis() - _nudgeBurstStartMs >= RobotConfig::NUDGE_BURST_MS) {
        motors.stop();
        _state = State::RotateSettle;
        _pauseUntilMs = millis() + RobotConfig::NUDGE_SETTLE_MS;
        return;
    }

    // Record heading at burst start for adaptive PWM
    if (millis() - _nudgeBurstStartMs < RobotConfig::LOOP_INTERVAL_MS + 5) {
        _headingBeforeNudge = pose.theta;
    }

    // During burst: apply adaptive PWM in correct direction
    int dir = (error > 0.0f ? 1 : -1) * RobotConfig::ROTATE_DIRECTION_SIGN;
    int pwm = _nudgePwm;
    if (dir > 0) {
        motors.setLeft(-pwm);
        motors.setRight(pwm);
    } else {
        motors.setLeft(pwm);
        motors.setRight(-pwm);
    }
}

void NodeNavigator::_handleRotateSettle(const Pose& pose, MotorDriver& motors,
                                        Encoder& leftEncoder, Encoder& rightEncoder) {
    motors.stop();
    if (millis() < _pauseUntilMs) return;

    // Settled — measure heading error
    float error = fabsf(_normalizeAngle(_targetHeading - pose.theta));

    if (error <= RobotConfig::NUDGE_DONE_TOL_RAD) {
        // Within tolerance — rotation complete
        if (_rotateOnly) {
            _snapX = pose.x;
            _snapY = pose.y;
            _snapTheta = _targetHeading;
            _snapPending = true;
            _currentIndex = 1;
            _state = State::Arrived;
            _scheduleBeeps(1);
        } else {
            _beginDrive(leftEncoder, rightEncoder);
        }
        return;
    }

    // Adaptive PWM: if heading barely changed, increase power
    float progress = fabsf(_normalizeAngle(pose.theta - _headingBeforeNudge));
    if (progress < RobotConfig::NUDGE_MIN_PROGRESS_RAD) {
        _nudgePwm = min(_nudgePwm + RobotConfig::NUDGE_PWM_STEP,
                        (int)RobotConfig::NUDGE_MAX_PWM);
    }

    // Still off — nudge again
    _nudgeBurstStartMs = millis();
    _state = State::Rotate;
}

// Drive straight toward current waypoint using encoder distance + IMU heading
void NodeNavigator::_handleDrive(const Pose& pose, MotorDriver& motors,
                                 Encoder& leftEncoder, Encoder& rightEncoder,
                                 CalibrationManager& calibration, bool imuAvailable,
                                 float imuHeadingRad) {
    // --- 1. Tinh tien trinh tu encoder ---
    long leftProgress = _absDelta(leftEncoder.getPulseCount(), _driveStartLeft);
    long rightProgress = _absDelta(rightEncoder.getPulseCount(), _driveStartRight);
    long avgProgress = (leftProgress + rightProgress) / 2;

    // --- 2. Kiem tra da den dich chua ---
    if (avgProgress >= _targetPulses) {
        calibration.recordStraightSegment(leftProgress, rightProgress);
        _snapX = _xs[_currentIndex];
        _snapY = _ys[_currentIndex];
        _snapTheta = _targetHeading;
        _snapPending = true;
        _currentIndex++;

        // Node tiếp thẳng hàng (< 10°) → tiếp tục đi, không dừng
        if (_currentIndex < _count) {
            float nextHeading = _targetHeadingBetween(
                _xs[_currentIndex - 1], _ys[_currentIndex - 1],
                _xs[_currentIndex], _ys[_currentIndex]);
            float headingDiff = fabsf(_normalizeAngle(nextHeading - _targetHeading));

            if (headingDiff < RobotConfig::SKIP_ROTATE_THRESHOLD_RAD) {
                _targetHeading = nextHeading;
                _targetPulses = _segmentPulses(
                    _xs[_currentIndex - 1], _ys[_currentIndex - 1],
                    _xs[_currentIndex], _ys[_currentIndex]);
                _driveStartLeft = leftEncoder.getPulseCount();
                _driveStartRight = rightEncoder.getPulseCount();
                _scheduleBeeps(1);
                return;  // giữ State::Drive, không dừng motor
            }
        }

        // Lệch hướng lớn hoặc node cuối → dừng, pause, xoay
        motors.stop();
        _pauseUntilMs = millis() + RobotConfig::NODE_PAUSE_MS;
        _state = State::NodePause;
        _scheduleBeeps(_currentIndex >= _count ? 3 : 1);
        return;
    }

    // --- 3. Tinh toc do co so (giam dan khi gan dich) ---
    int remaining = _targetPulses - avgProgress;
    int base = RobotConfig::DRIVE_SPEED_PWM;
    if (remaining < RobotConfig::DRIVE_SLOWDOWN_PULSES) {
        base = map(remaining, 0, RobotConfig::DRIVE_SLOWDOWN_PULSES,
                   RobotConfig::DRIVE_MIN_PWM, RobotConfig::DRIVE_SPEED_PWM);
    }

    // --- 4. Heading correction: giu huong bang IMU truc tiep ---
    // Dung raw IMU thay vi odometry de tranh feedback loop
    // (encoder lech -> odometry sai -> correction sai them)
    // Deadband 3 do: bo qua nhieu IMU nho do rung motor
    float headingError = _normalizeAngle(_targetHeading - imuHeadingRad);
    int headingCorrection = 0;
    if (imuAvailable && fabsf(headingError) > RobotConfig::DRIVE_HEADING_DEADBAND_RAD) {
        headingCorrection = constrain(
            (int)(headingError * RobotConfig::DRIVE_HEADING_GAIN),
            -RobotConfig::DRIVE_STEER_MAX,
            RobotConfig::DRIVE_STEER_MAX);
    }

    // --- 5. Balance correction: can bang encoder 2 banh ---
    // Neu 1 banh quay nhieu hon -> giam toc banh do, tang banh kia
    int balanceCorrection = constrain((int)((rightProgress - leftProgress) *
                                      RobotConfig::DRIVE_ENCODER_BALANCE_GAIN),
                                      -RobotConfig::DRIVE_STEER_MAX,
                                      RobotConfig::DRIVE_STEER_MAX);

    // --- 6. Ap dung ca 2 correction vao motor ---
    // heading > 0: xe lech phai, can re trai -> giam trai, tang phai
    // balance > 0: phai nhieu hon -> tang trai, giam phai -> can bang
    int leftTarget = constrain(base + balanceCorrection - headingCorrection,
                               RobotConfig::DRIVE_MIN_PWM, RobotConfig::DRIVE_MAX_PWM);
    int rightTarget = constrain(base - balanceCorrection + headingCorrection,
                                RobotConfig::DRIVE_MIN_PWM, RobotConfig::DRIVE_MAX_PWM);
    calibration.applyMotorTrim(&leftTarget, &rightTarget);

    motors.setLeftSlew(leftTarget, RobotConfig::DRIVE_SLEW_STEP);
    motors.setRightSlew(rightTarget, RobotConfig::DRIVE_SLEW_STEP);
}

void NodeNavigator::_handleNodePause(const Pose& pose, MotorDriver& motors,
                                     Encoder& leftEncoder, Encoder& rightEncoder) {
    motors.stop();
    if (millis() < _pauseUntilMs) return;

    if (_currentIndex >= _count) {
        _state = State::Arrived;
        return;
    }

    _beginRotateOrDrive(pose, leftEncoder, rightEncoder);
}

void NodeNavigator::_scheduleBeeps(int count) {
    _beepsRemaining = max(_beepsRemaining, count);
    _setBuzzer(false);
    _nextBuzzerMs = millis();
}

void NodeNavigator::_updateBuzzer() {
    unsigned long now = millis();
    if (now < _nextBuzzerMs) return;

    if (_buzzerOn) {
        _setBuzzer(false);
        _beepsRemaining--;
        _nextBuzzerMs = now + RobotConfig::BUZZER_GAP_MS;
    } else if (_beepsRemaining > 0) {
        _setBuzzer(true);
        _nextBuzzerMs = now + RobotConfig::BUZZER_ON_MS;
    }
}

void NodeNavigator::_setBuzzer(bool on) {
    _buzzerOn = on;
    digitalWrite(RobotConfig::BUZZER_PIN, on ? HIGH : LOW);
}

void NodeNavigator::_setFault(const char* status, MotorDriver& motors) {
    motors.stop();
    strncpy(_faultStatus, status, sizeof(_faultStatus) - 1);
    _faultStatus[sizeof(_faultStatus) - 1] = '\0';
    _state = State::Fault;
}

float NodeNavigator::_normalizeAngle(float angle) {
    if (!isfinite(angle)) return 0.0f;
    while (angle > PI) angle -= 2.0f * PI;
    while (angle < -PI) angle += 2.0f * PI;
    return angle;
}

float NodeNavigator::_targetHeadingBetween(float x0, float y0, float x1, float y1) {
    return _normalizeAngle(atan2f(-(y1 - y0), x1 - x0));
}

int NodeNavigator::_segmentPulses(float x0, float y0, float x1, float y1) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float dist = sqrtf(dx * dx + dy * dy);
    int nodeSteps = max(1, (int)lroundf(dist / RobotConfig::NODE_DISTANCE_M));
    return nodeSteps * RobotConfig::PULSES_PER_NODE;
}

long NodeNavigator::_absDelta(long current, long start) {
    long delta = current - start;
    return delta < 0 ? -delta : delta;
}