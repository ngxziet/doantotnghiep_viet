#include "autonomous_explorer.h"
#include <math.h>

AutonomousExplorer::AutonomousExplorer(MotorDriver& motors, UltrasonicHcsr04& sonar,
                                       SensorPanServo& servo, NodeNavigator& navigator)
    : _motors(motors), _sonar(sonar), _servo(servo), _navigator(navigator)
{}

void AutonomousExplorer::start() {
    _navigator.clear();
    _servo.center();
    _frontCm = MAX_RANGE_CM;
    _driveHeadingValid = false;  // chốt hướng giữ thẳng ở tick đầu tiên
    _lastSampleMs = 0;
    _state = State::Drive;
}

void AutonomousExplorer::stop() {
    _navigator.clear();
    _motors.stop();
    _servo.center();
    _state = State::Off;
}

void AutonomousExplorer::update(const Pose& pose, Encoder& leftEnc, Encoder& rightEnc,
                                CalibrationManager& calibration, bool imuConnected,
                                float imuHeadingRad) {
    switch (_state) {
        case State::Off:
        case State::Stuck:
            _motors.stop();
            return;

        case State::Drive:
            _handleDrive(imuConnected, imuHeadingRad);
            return;

        case State::ScanLeft:
            _handleScanLeft();
            return;

        case State::ScanRight:
            _handleScanRight(imuHeadingRad);
            return;

        case State::Turn:
            // Giao việc xoay cho navigator (tái dùng logic nudge/fast-rotate đã hiệu chỉnh).
            // Main loop sẽ xử lý navigator.hasSnap() để đồng bộ heading sau khi xoay xong.
            _navigator.update(pose, _motors, leftEnc, rightEnc, calibration,
                              imuConnected, imuHeadingRad);
            if (!_navigator.isRotating() && !_navigator.isActive()) {
                // navigator đã về Arrived → xoay xong, quay lại chạy thẳng
                _servo.center();
                _driveHeadingValid = false;  // chốt lại hướng mới ở tick sau
                _lastSampleMs = 0;
                _state = State::Drive;
            }
            return;
    }
}

void AutonomousExplorer::_handleDrive(bool imuConnected, float imuHeadingRad) {
    if (!_driveHeadingValid) {
        // Reset navigator (xoá cờ isStep còn lại từ pha xoay trước) để main loop
        // không suppress translation → odometry tích lũy lại vị trí khi chạy thẳng.
        // Snap của pha xoay đã được main loop tiêu thụ ở tick trước nên xoá an toàn.
        _navigator.clear();
        _driveHeading = imuHeadingRad;
        _driveHeadingValid = true;
    }

    // Đo sonar phía trước theo chu kỳ (không đo mỗi tick để tránh nghẽn vòng lặp)
    unsigned long now = millis();
    if (now - _lastSampleMs >= RobotConfig::AUTO_DRIVE_SAMPLE_MS) {
        _lastSampleMs = now;
        float d = _sonar.readQuickCm();
        if (d < RobotConfig::AUTO_STOP_DISTANCE_CM) {
            // Đo xác nhận lần 2 để loại nhiễu một lần
            float d2 = _sonar.readQuickCm();
            float m = (d < d2) ? d : d2;
            _frontCm = m;
            if (m < RobotConfig::AUTO_STOP_DISTANCE_CM) {
                _motors.stop();
                _servo.lookLeft();
                _scanTs = now;
                _state = State::ScanLeft;
                return;
            }
        } else {
            _frontCm = d;
        }
    }

    // Chạy thẳng — giữ hướng bằng IMU (mô phỏng cách _handleDrive của navigator)
    int base = RobotConfig::AUTO_DRIVE_PWM;
    int corr = 0;
    if (imuConnected) {
        float err = _normalize(_driveHeading - imuHeadingRad);
        if (fabsf(err) > RobotConfig::DRIVE_HEADING_DEADBAND_RAD) {
            corr = constrain((int)(err * RobotConfig::DRIVE_HEADING_GAIN),
                             -RobotConfig::DRIVE_STEER_MAX,
                             RobotConfig::DRIVE_STEER_MAX);
        }
    }
    int l = constrain(base - corr, RobotConfig::DRIVE_MIN_PWM, RobotConfig::DRIVE_MAX_PWM);
    int r = constrain(base + corr, RobotConfig::DRIVE_MIN_PWM, RobotConfig::DRIVE_MAX_PWM);
    _motors.setLeftSlew(l, RobotConfig::DRIVE_SLEW_STEP);
    _motors.setRightSlew(r, RobotConfig::DRIVE_SLEW_STEP);
}

void AutonomousExplorer::_handleScanLeft() {
    _motors.stop();
    if (millis() - _scanTs < RobotConfig::SERVO_SETTLE_MS) return;

    _leftCm = _sonar.readDistanceCm();  // đứng yên → đo kỹ bằng trung vị 5 mẫu
    _servo.lookRight();
    _scanTs = millis();
    _state = State::ScanRight;
}

void AutonomousExplorer::_handleScanRight(float imuHeadingRad) {
    _motors.stop();
    if (millis() - _scanTs < RobotConfig::SERVO_SETTLE_MS) return;

    _rightCm = _sonar.readDistanceCm();
    _servo.center();
    _decideAfterScan(imuHeadingRad);
}

void AutonomousExplorer::_decideAfterScan(float imuHeadingRad) {
    bool leftOpen = _leftCm >= RobotConfig::AUTO_STOP_DISTANCE_CM;
    bool rightOpen = _rightCm >= RobotConfig::AUTO_STOP_DISTANCE_CM;

    if (leftOpen || rightOpen) {
        // Chọn bên rộng hơn; bằng nhau → ưu tiên trái
        bool goRight = _rightCm > _leftCm;
        // Quay trái = +90° (yaw tăng, CCW); quay phải = -90°
        float deltaDeg = goRight ? -RobotConfig::AUTO_AVOID_TURN_DEG
                                 :  RobotConfig::AUTO_AVOID_TURN_DEG;
        _startTurn(imuHeadingRad + deltaDeg * PI / 180.0f);
    } else if (_leftCm >= RobotConfig::AUTO_TURN_CLEARANCE_CM &&
               _rightCm >= RobotConfig::AUTO_TURN_CLEARANCE_CM) {
        // 2 bên đều <30cm nhưng đủ ≥25cm để xoay → quay đầu 180°
        _startTurn(imuHeadingRad + PI);
    } else {
        // 2 bên quá hẹp → dừng hẳn
        _motors.stop();
        _state = State::Stuck;
    }
}

void AutonomousExplorer::_startTurn(float targetHeading) {
    _navigator.setStepTurn(_normalize(targetHeading));
    _state = State::Turn;
}

const char* AutonomousExplorer::status() const {
    switch (_state) {
        case State::Off:       return "idle";
        case State::Drive:     return "exploring";
        case State::ScanLeft:
        case State::ScanRight: return "scanning";
        case State::Turn:      return "avoiding";
        case State::Stuck:     return "stuck";
    }
    return "idle";
}

float AutonomousExplorer::_normalize(float a) {
    if (!isfinite(a)) return 0.0f;
    while (a > PI) a -= 2.0f * PI;
    while (a < -PI) a += 2.0f * PI;
    return a;
}
