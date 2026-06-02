#include "node_navigator.h"
#include <math.h>
#include <string.h>

void NodeNavigator::begin() {
    pinMode(RobotConfig::BUZZER_PIN, OUTPUT);
    _setBuzzer(false);
}

void NodeNavigator::setRoute(const float xs[], const float ys[], int count,
                             const Pose& currentPose) {
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
    _beginRotateOrDrive(currentPose);
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
    _turnStableSinceMs = 0;
    _stateStartMs = millis();
    _state = State::Rotate;
}

void NodeNavigator::clear() {
    _state = State::Idle;
    _count = 0;
    _currentIndex = 0;
    _rotateOnly = false;
    _isStep = false;
    _snapPending = false;
    _turnStableSinceMs = 0;
    _faultStatus[0] = '\0';
}

void NodeNavigator::update(const Pose& pose, MotorDriver& motors,
                           Encoder& leftEncoder, Encoder& rightEncoder,
                           CalibrationManager& calibration, bool imuAvailable) {
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
                         calibration, imuAvailable);
            return;
        case State::NodePause:
            _handleNodePause(pose, motors);
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

void NodeNavigator::_beginRotateOrDrive(const Pose& pose) {
    if (_currentIndex >= _count) {
        _state = State::Arrived;
        _scheduleBeeps(3);
        return;
    }

    _targetHeading = _targetHeadingBetween(pose.x, pose.y,
                                           _xs[_currentIndex], _ys[_currentIndex]);
    _targetPulses = _segmentPulses(pose.x, pose.y,
                                   _xs[_currentIndex], _ys[_currentIndex]);
    _rotateMinPwm = RobotConfig::ROTATE_MIN_PWM;
    _headingBeforeRotate = pose.theta;
    _stateStartMs = millis();
    _state = State::Rotate;
}

void NodeNavigator::_beginDrive(Encoder& leftEncoder, Encoder& rightEncoder) {
    _driveStartLeft = leftEncoder.getPulseCount();
    _driveStartRight = rightEncoder.getPulseCount();
    _stateStartMs = millis();
    _state = State::Drive;
}

// 3-speed step turn for test mode
void NodeNavigator::_handleStepTurn(const Pose& pose, MotorDriver& motors) {
    unsigned long now = millis();
    float error = _normalizeAngle(_targetHeading - pose.theta);
    float absError = fabsf(error);

    if (absError <= RobotConfig::STEP_TURN_DONE_TOL_RAD) {
        motors.stop();
        if (_turnStableSinceMs == 0) {
            _turnStableSinceMs = now;
            return;
        }
        if (now - _turnStableSinceMs < RobotConfig::STEP_TURN_STABLE_MS) {
            return;
        }

        _snapX = pose.x;
        _snapY = pose.y;
        _snapTheta = _targetHeading;
        _snapPending = true;
        _currentIndex = 1;
        _state = State::Arrived;
        _scheduleBeeps(1);
        return;
    }

    _turnStableSinceMs = 0;

    if (now - _stateStartMs > RobotConfig::ROTATE_TIMEOUT_MS) {
        _setFault("rotation_timeout", motors);
        return;
    }

    int speed = RobotConfig::STEP_TURN_FAST_PWM;
    if (absError <= RobotConfig::STEP_TURN_SLOW_REMAIN_RAD) {
        speed = RobotConfig::STEP_TURN_FINE_PWM;
    } else if (absError <= RobotConfig::STEP_TURN_FAST_REMAIN_RAD) {
        speed = RobotConfig::STEP_TURN_SLOW_PWM;
    }

    int turnDirection = error > 0.0f ? 1 : -1;
    turnDirection *= RobotConfig::ROTATE_DIRECTION_SIGN;
    if (turnDirection > 0) {
        motors.setLeftSlew(-speed, RobotConfig::ROTATE_SLEW_STEP);
        motors.setRightSlew(speed, RobotConfig::ROTATE_SLEW_STEP);
    } else {
        motors.setLeftSlew(speed, RobotConfig::ROTATE_SLEW_STEP);
        motors.setRightSlew(-speed, RobotConfig::ROTATE_SLEW_STEP);
    }
}

// Proportional rotation for route navigation
void NodeNavigator::_handleRotate(const Pose& pose, MotorDriver& motors) {
    if (_rotateOnly) {
        _handleStepTurn(pose, motors);
        return;
    }

    float error = _normalizeAngle(_targetHeading - pose.theta);
    float absError = fabsf(error);

    if (absError <= RobotConfig::ROTATE_START_TOL_RAD) {
        motors.stop();
        _state = State::RotateSettle;
        _pauseUntilMs = millis() + RobotConfig::ROTATE_SETTLE_MS;
        return;
    }

    if (millis() - _stateStartMs > RobotConfig::ROTATE_TIMEOUT_MS) {
        _setFault("rotation_timeout", motors);
        return;
    }

    int speed = constrain((int)(absError * RobotConfig::ROTATE_GAIN),
                          _rotateMinPwm,
                          RobotConfig::ROTATE_MAX_PWM);
    int turnDirection = error > 0.0f ? 1 : -1;
    turnDirection *= RobotConfig::ROTATE_DIRECTION_SIGN;
    if (turnDirection > 0) {
        motors.setLeftSlew(-speed, RobotConfig::ROTATE_SLEW_STEP);
        motors.setRightSlew(speed, RobotConfig::ROTATE_SLEW_STEP);
    } else {
        motors.setLeftSlew(speed, RobotConfig::ROTATE_SLEW_STEP);
        motors.setRightSlew(-speed, RobotConfig::ROTATE_SLEW_STEP);
    }
}

void NodeNavigator::_handleRotateSettle(const Pose& pose, MotorDriver& motors,
                                        Encoder& leftEncoder, Encoder& rightEncoder) {
    motors.stop();
    if (millis() < _pauseUntilMs) return;

    float error = fabsf(_normalizeAngle(_targetHeading - pose.theta));
    if (_rotateOnly) {
        _snapX = pose.x;
        _snapY = pose.y;
        _snapTheta = error <= RobotConfig::ROTATE_SETTLE_TOL_RAD
            ? _targetHeading
            : _normalizeAngle(pose.theta);
        _snapPending = true;
        _currentIndex = 1;
        _state = State::Arrived;
        _scheduleBeeps(1);
        return;
    }

    if (error <= RobotConfig::ROTATE_SETTLE_TOL_RAD) {
        _beginDrive(leftEncoder, rightEncoder);
    } else {
        // Check if rotation made enough progress, if not increase PWM
        float progress = fabsf(_normalizeAngle(pose.theta - _headingBeforeRotate));
        if (progress < RobotConfig::ROTATE_MIN_PROGRESS_RAD) {
            _rotateMinPwm = min(_rotateMinPwm + RobotConfig::ROTATE_PWM_STEP,
                                (int)RobotConfig::ROTATE_MAX_PWM);
        }
        _headingBeforeRotate = pose.theta;
        _state = State::Rotate;
    }
}

void NodeNavigator::_handleDrive(const Pose& pose, MotorDriver& motors,
                                 Encoder& leftEncoder, Encoder& rightEncoder,
                                 CalibrationManager& calibration, bool imuAvailable) {
    long leftProgress = _absDelta(leftEncoder.getPulseCount(), _driveStartLeft);
    long rightProgress = _absDelta(rightEncoder.getPulseCount(), _driveStartRight);
    long avgProgress = (leftProgress + rightProgress) / 2;

    if (avgProgress >= _targetPulses) {
        motors.stop();
        calibration.recordStraightSegment(leftProgress, rightProgress);

        _snapX = _xs[_currentIndex];
        _snapY = _ys[_currentIndex];
        _snapTheta = _targetHeading;
        _snapPending = true;
        _currentIndex++;
        _pauseUntilMs = millis() + RobotConfig::NODE_PAUSE_MS;
        _state = State::NodePause;
        _scheduleBeeps(_currentIndex >= _count ? 3 : 1);
        return;
    }

    int remaining = _targetPulses - avgProgress;
    int base = RobotConfig::DRIVE_SPEED_PWM;
    if (remaining < RobotConfig::DRIVE_SLOWDOWN_PULSES) {
        base = map(remaining, 0, RobotConfig::DRIVE_SLOWDOWN_PULSES,
                   RobotConfig::DRIVE_MIN_PWM, RobotConfig::DRIVE_SPEED_PWM);
    }

    float headingError = _normalizeAngle(_targetHeading - pose.theta);
    int headingCorrection = imuAvailable
        ? constrain((int)(headingError * RobotConfig::DRIVE_HEADING_GAIN),
                    -RobotConfig::DRIVE_STEER_MAX,
                    RobotConfig::DRIVE_STEER_MAX)
        : 0;
    int balanceCorrection = constrain((int)((rightProgress - leftProgress) *
                                      RobotConfig::DRIVE_ENCODER_BALANCE_GAIN),
                                      -RobotConfig::DRIVE_STEER_MAX,
                                      RobotConfig::DRIVE_STEER_MAX);

    int leftTarget = constrain(base + balanceCorrection - headingCorrection,
                               RobotConfig::DRIVE_MIN_PWM, RobotConfig::DRIVE_MAX_PWM);
    int rightTarget = constrain(base - balanceCorrection + headingCorrection,
                                RobotConfig::DRIVE_MIN_PWM, RobotConfig::DRIVE_MAX_PWM);
    calibration.applyMotorTrim(&leftTarget, &rightTarget);

    motors.setLeftSlew(leftTarget, RobotConfig::DRIVE_SLEW_STEP);
    motors.setRightSlew(rightTarget, RobotConfig::DRIVE_SLEW_STEP);
}

void NodeNavigator::_handleNodePause(const Pose& pose, MotorDriver& motors) {
    motors.stop();
    if (millis() < _pauseUntilMs) return;

    if (_currentIndex >= _count) {
        _state = State::Arrived;
        return;
    }

    _beginRotateOrDrive(pose);
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
