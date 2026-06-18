#pragma once

#include <Arduino.h>
#include "../config.h"
#include "motor_driver.h"
#include "node_navigator.h"
#include "../sensors/ultrasonic_hcsr04.h"
#include "../sensors/sensor_pan_servo.h"
#include "../sensors/encoder.h"
#include "../localization/odometry.h"
#include "../calibration/calibration_manager.h"

// Tự hành né vật cản dùng HC-SR04 + servo quét:
//   - Chạy thẳng, đo khoảng cách phía trước mỗi chu kỳ.
//   - <30cm phía trước  → dừng ngay, quay servo trái rồi phải để đo 2 bên.
//   - Chọn bên có khoảng cách lớn hơn (bằng nhau → ưu tiên trái), quay 90° rồi chạy tiếp.
//   - Cả 2 bên <30cm:
//       + nếu cả 2 bên ≥25cm (đủ chỗ xoay) → quay đầu 180°.
//       + không thì dừng hẳn (kẹt).
// Việc xoay 90°/180° giao lại cho NodeNavigator::setStepTurn (tái dùng nudge/fast-rotate).
class AutonomousExplorer {
public:
    AutonomousExplorer(MotorDriver& motors, UltrasonicHcsr04& sonar,
                       SensorPanServo& servo, NodeNavigator& navigator);

    void start();   // bật chế độ tự hành, bắt đầu chạy thẳng
    void stop();    // tắt, dừng motor, servo về giữa

    bool isActive() const { return _state != State::Off; }

    // Gọi mỗi chu kỳ vòng lặp khi đang bật. pose/imuHeadingRad dùng cho pha xoay.
    void update(const Pose& pose, Encoder& leftEnc, Encoder& rightEnc,
                CalibrationManager& calibration, bool imuConnected, float imuHeadingRad);

    const char* status() const;             // exploring/scanning/avoiding/stuck/idle
    float lastFrontCm() const { return _frontCm; }

private:
    enum class State {
        Off,        // tắt
        Drive,      // chạy thẳng
        StopSettle, // vừa dừng vì vật cản, chờ L298N spike tan rồi mới quét servo
        ScanLeft,   // servo nhìn trái, chờ ổn định rồi đo
        ScanRight,  // servo nhìn phải, chờ ổn định rồi đo
        Turn,       // đang xoay (giao cho navigator)
        Stuck       // kẹt: 2 bên đều quá hẹp
    };

    MotorDriver& _motors;
    UltrasonicHcsr04& _sonar;
    SensorPanServo& _servo;
    NodeNavigator& _navigator;

    State _state = State::Off;
    float _frontCm = MAX_RANGE_CM;
    float _leftCm = 0.0f;
    float _rightCm = 0.0f;
    unsigned long _scanTs = 0;        // mốc thời gian bắt đầu chờ servo ổn định
    unsigned long _stopSettleUntilMs = 0;  // mốc thời gian chờ spike L298N tan
    unsigned long _lastSampleMs = 0;  // lần đo sonar gần nhất khi chạy thẳng
    bool _driveHeadingValid = false;  // đã chốt hướng giữ thẳng chưa
    float _driveHeading = 0.0f;       // hướng cần giữ khi chạy thẳng

    void _handleDrive(bool imuConnected, float imuHeadingRad);
    void _handleStopSettle();
    void _handleScanLeft();
    void _handleScanRight(float imuHeadingRad);
    void _decideAfterScan(float imuHeadingRad);
    void _startTurn(float targetHeading, float currentHeading);

    static float _normalize(float a);
};
