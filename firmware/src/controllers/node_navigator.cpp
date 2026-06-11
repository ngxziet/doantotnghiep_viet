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
        case State::FastRotate:
            _handleFastRotate(pose, motors);
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
    return _state == State::FastRotate || _state == State::Rotate ||
           _state == State::RotateSettle ||
           _state == State::Drive || _state == State::NodePause;
}

bool NodeNavigator::isDrivingStraight() const {
    return _state == State::Drive;
}

bool NodeNavigator::isRotating() const {
    return _state == State::FastRotate || _state == State::Rotate ||
           _state == State::RotateSettle;
}

const char* NodeNavigator::status() const {
    switch (_state) {
        case State::Idle: return "idle";
        case State::Arrived: return "arrived";
        case State::Fault: return _faultStatus[0] ? _faultStatus : "fault";
        case State::FastRotate:
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

    float headingErrorRaw = _normalizeAngle(_targetHeading - pose.theta);
    float headingError = fabsf(headingErrorRaw);

    // Lệch < 10°: đi thẳng luôn, heading correction tự điều chỉnh
    if (headingError < RobotConfig::SKIP_ROTATE_THRESHOLD_RAD) {
        _beginDrive(leftEncoder, rightEncoder);
        return;
    }

    _stateStartMs = millis();
    _initialRotateError = headingError;
    _totalRotationAccum = 0.0f;
    _headingBeforeNudge = pose.theta;

    // Lệch > 25°: xoay nhanh liên tục trước, rồi nudge phần còn lại
    if (headingError > RobotConfig::FAST_ROTATE_THRESHOLD_RAD) {
        // Góc lớn (>120°, quay đầu): PWM thấp hơn tránh trượt quá 180°
        // Góc nhỏ (trái/phải): PWM cao hơn để xoay đủ nhanh
        _fastRotatePwm = (headingError > RobotConfig::FAST_ROTATE_REVERSE_RAD)
            ? RobotConfig::FAST_ROTATE_REVERSE_PWM
            : RobotConfig::FAST_ROTATE_PWM;

        float angleDeg = headingError * (180.0f / PI);
        float estimatedMs = (angleDeg / RobotConfig::FAST_ROTATE_SPEED_DPS) * 1000.0f;
        estimatedMs *= RobotConfig::FAST_ROTATE_UNDERSHOOT; // 75% tránh trượt quá
        _fastRotateDir = (headingErrorRaw > 0.0f ? 1 : -1) * RobotConfig::ROTATE_DIRECTION_SIGN;
        _fastRotateUntilMs = millis() + (unsigned long)estimatedMs;
        _state = State::FastRotate;
        return;
    }

    // 10°-25°: nudge, burst time tỉ lệ với góc còn lại (mốc dưới = 4° done tolerance)
    float r = (headingError - RobotConfig::NUDGE_DONE_TOL_RAD) /
              (RobotConfig::FAST_ROTATE_THRESHOLD_RAD - RobotConfig::NUDGE_DONE_TOL_RAD);
    if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
    _currentBurstMs = RobotConfig::NUDGE_BURST_MIN_MS +
        (unsigned long)(r * (float)(RobotConfig::NUDGE_BURST_MAX_MS - RobotConfig::NUDGE_BURST_MIN_MS));
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

// Xoay nhanh: quay liên tục theo thời gian ước lượng, dừng khi gần đích
void NodeNavigator::_handleFastRotate(const Pose& pose, MotorDriver& motors) {
    // Timeout bảo vệ
    if (millis() - _stateStartMs > RobotConfig::ROTATE_TIMEOUT_MS) {
        _setFault("rotation_timeout", motors);
        return;
    }

    float errorRaw = _normalizeAngle(_targetHeading - pose.theta);
    float error = fabsf(errorRaw);

    // Đã gần đích (trong ngưỡng nudge) → dừng sớm, chuyển settle
    if (error <= RobotConfig::NUDGE_DONE_TOL_RAD) {
        motors.stop();
        _state = State::RotateSettle;
        _pauseUntilMs = millis() + RobotConfig::NUDGE_SETTLE_MS;
        return;
    }

    // Phát hiện đã vượt qua target (error đổi dấu so với ban đầu)
    // Fix bug 180°: sau khi vượt 180°, fabsf(error) tăng lại do angle wrapping
    // → điều kiện error <= 4° không bao giờ thỏa → robot quay mãi
    int initialSign = _fastRotateDir * RobotConfig::ROTATE_DIRECTION_SIGN;
    bool crossedTarget = (initialSign > 0 && errorRaw < 0) ||
                         (initialSign < 0 && errorRaw > 0);
    if (crossedTarget) {
        motors.stop();
        _state = State::RotateSettle;
        _pauseUntilMs = millis() + RobotConfig::NUDGE_SETTLE_MS;
        return;
    }

    // Hết thời gian ước lượng → dừng, chờ ổn định rồi nudge phần còn lại
    if (millis() >= _fastRotateUntilMs) {
        motors.stop();
        _state = State::RotateSettle;
        _pauseUntilMs = millis() + RobotConfig::NUDGE_SETTLE_MS;
        return;
    }

    // Quay liên tục ở PWM tùy theo góc xoay
    int pwm = _fastRotatePwm;
    if (_fastRotateDir > 0) {
        motors.setLeft(-pwm);
        motors.setRight(pwm);
    } else {
        motors.setLeft(pwm);
        motors.setRight(-pwm);
    }
}

// Xoay nudge: burst ngắn → dừng → đo → lặp lại (tinh chỉnh góc nhỏ)
void NodeNavigator::_handleRotate(const Pose& pose, MotorDriver& motors) {
    float error = _normalizeAngle(_targetHeading - pose.theta);
    float absError = fabsf(error);

    // Đã nằm trong ngưỡng cho phép → dừng motor, chuyển sang settle để xác nhận
    if (absError <= RobotConfig::NUDGE_DONE_TOL_RAD) {
        motors.stop();
        _state = State::RotateSettle;
        _pauseUntilMs = millis() + RobotConfig::NUDGE_SETTLE_MS;
        return;
    }

    // Timeout bảo vệ: xoay quá lâu → báo lỗi (motor yếu, bánh kẹt, IMU hỏng)
    if (millis() - _stateStartMs > RobotConfig::ROTATE_TIMEOUT_MS) {
        _setFault("rotation_timeout", motors);
        return;
    }

    // Burst đã hết thời gian → dừng motor, chờ ổn định rồi đo heading
    if (millis() - _nudgeBurstStartMs >= _currentBurstMs) {
        motors.stop();
        _state = State::RotateSettle;
        _pauseUntilMs = millis() + RobotConfig::NUDGE_SETTLE_MS;
        return;
    }

    // Ghi lại heading đầu burst để so sánh tiến bộ (adaptive PWM)
    if (millis() - _nudgeBurstStartMs < RobotConfig::LOOP_INTERVAL_MS + 5) {
        _headingBeforeNudge = pose.theta;
    }

    // Trong burst: quay motor theo hướng cần xoay, PWM tự điều chỉnh
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

    // Đã ổn định — đo tiến bộ và sai lệch heading
    float progress = fabsf(_normalizeAngle(pose.theta - _headingBeforeNudge));
    _totalRotationAccum += progress;
    float error = fabsf(_normalizeAngle(_targetHeading - pose.theta));

    if (error <= RobotConfig::NUDGE_DONE_TOL_RAD) {
        // Trong ngưỡng cho phép — xoay hoàn tất
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

    // Guard: tổng xoay vượt quá góc ban đầu + 60° → dừng (chống xoay vòng vòng 360°)
    if (_totalRotationAccum > _initialRotateError + RobotConfig::ROTATE_MAX_OVERSHOOT_RAD) {
        if (_rotateOnly) {
            _state = State::Arrived;
            _scheduleBeeps(1);
        } else {
            _beginDrive(leftEncoder, rightEncoder);
        }
        return;
    }

    // PWM thích ứng: tăng khi không xoay đủ, giảm khi xoay quá nhiều
    if (progress < RobotConfig::NUDGE_MIN_PROGRESS_RAD) {
        _nudgePwm = min(_nudgePwm + RobotConfig::NUDGE_PWM_STEP,
                        (int)RobotConfig::NUDGE_MAX_PWM);
    } else if (progress > RobotConfig::NUDGE_OVER_PROGRESS_RAD) {
        _nudgePwm = max(_nudgePwm - RobotConfig::NUDGE_PWM_DOWN_STEP,
                        (int)RobotConfig::NUDGE_ROTATE_PWM);
    }

    // Burst time tỉ lệ với góc còn lại: nhỏ → burst ngắn (chính xác), lớn → burst dài (nhanh)
    float r = (error - RobotConfig::NUDGE_DONE_TOL_RAD) /
              (RobotConfig::FAST_ROTATE_THRESHOLD_RAD - RobotConfig::NUDGE_DONE_TOL_RAD);
    if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
    _currentBurstMs = RobotConfig::NUDGE_BURST_MIN_MS +
        (unsigned long)(r * (float)(RobotConfig::NUDGE_BURST_MAX_MS - RobotConfig::NUDGE_BURST_MIN_MS));

    // Vẫn còn lệch — thử burst tiếp
    _nudgeBurstStartMs = millis();
    _state = State::Rotate;
}

// Đi thẳng đến waypoint hiện tại, dùng encoder đo quãng đường + IMU giữ hướng
void NodeNavigator::_handleDrive(const Pose& pose, MotorDriver& motors,
                                 Encoder& leftEncoder, Encoder& rightEncoder,
                                 CalibrationManager& calibration, bool imuAvailable,
                                 float imuHeadingRad) {
    // --- 1. Tính tiến trình từ encoder ---
    long leftProgress = _absDelta(leftEncoder.getPulseCount(), _driveStartLeft);
    long rightProgress = _absDelta(rightEncoder.getPulseCount(), _driveStartRight);
    long avgProgress = (leftProgress + rightProgress) / 2;

    // --- 2. Kiểm tra đã đến đích chưa ---
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

    // --- 3. Tính tốc độ cơ sở (giảm dần khi gần đích) ---
    int remaining = _targetPulses - avgProgress;
    int base = RobotConfig::DRIVE_SPEED_PWM;
    if (remaining < RobotConfig::DRIVE_SLOWDOWN_PULSES) {
        base = map(remaining, 0, RobotConfig::DRIVE_SLOWDOWN_PULSES,
                   RobotConfig::DRIVE_MIN_PWM, RobotConfig::DRIVE_SPEED_PWM);
    }

    // --- 4. Chỉnh hướng: giữ hướng bằng IMU trực tiếp ---
    // Dùng raw IMU thay vì odometry để tránh feedback loop
    // (encoder lệch → odometry sai → correction sai thêm)
    // Deadband 2°: bỏ qua nhiễu IMU nhỏ do rung motor
    float headingError = _normalizeAngle(_targetHeading - imuHeadingRad);
    int headingCorrection = 0;
    if (imuAvailable && fabsf(headingError) > RobotConfig::DRIVE_HEADING_DEADBAND_RAD) {
        headingCorrection = constrain(
            (int)(headingError * RobotConfig::DRIVE_HEADING_GAIN),
            -RobotConfig::DRIVE_STEER_MAX,
            RobotConfig::DRIVE_STEER_MAX);
    }

    // --- 5. Cân bằng encoder: bù chênh lệch 2 bánh ---
    // Nếu 1 bánh quay nhiều hơn → giảm tốc bánh đó, tăng bánh kia
    int balanceCorrection = constrain((int)((rightProgress - leftProgress) *
                                      RobotConfig::DRIVE_ENCODER_BALANCE_GAIN),
                                      -RobotConfig::DRIVE_STEER_MAX,
                                      RobotConfig::DRIVE_STEER_MAX);

    // --- 6. Áp dụng cả 2 correction vào motor ---
    // heading > 0: xe lệch phải, cần rẽ trái → giảm trái, tăng phải
    // balance > 0: phải nhiều hơn → tăng trái, giảm phải → cân bằng
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