#pragma once

#include <Arduino.h>

namespace RobotConfig {

static constexpr float PI_F = 3.14159265358979323846f;

static constexpr int MOTOR_LEFT_IN1 = 12;
static constexpr int MOTOR_LEFT_IN2 = 13;
static constexpr int MOTOR_LEFT_ENA = 14;
static constexpr int MOTOR_RIGHT_IN3 = 26;
static constexpr int MOTOR_RIGHT_IN4 = 27;
static constexpr int MOTOR_RIGHT_ENB = 25;

static constexpr int ENCODER_LEFT_PIN = 34;
static constexpr int ENCODER_RIGHT_PIN = 32;

static constexpr int MPU6050_SDA_PIN = 21;
static constexpr int MPU6050_SCL_PIN = 22;

static constexpr int ULTRASONIC_TRIG_PIN = 5;
static constexpr int ULTRASONIC_ECHO_PIN = 18;

// Servo quét cảm biến HC-SR04 trái/phải (LEDC thô, kênh riêng — không đụng kênh motor 0/1)
static constexpr int SERVO_PIN = 19;

static constexpr int BUZZER_PIN = 4;

// Tăng số này khi thay đổi chân hoặc phần cứng để tự động reset calibration
static constexpr int CONFIG_VERSION = 2;

static constexpr const char* WIFI_SSID = "RobotCar";
static constexpr const char* WIFI_PASS = "12345678";
static constexpr int WIFI_CHANNEL = 6;

static constexpr int ENCODER_PULSES_PER_REV = 20;
static constexpr float WHEEL_DIAM_CM = 6.5f;
static constexpr float WHEEL_CIRCUMFERENCE_CM = PI_F * WHEEL_DIAM_CM;
static constexpr float CM_PER_PULSE = WHEEL_CIRCUMFERENCE_CM / ENCODER_PULSES_PER_REV;

static constexpr float WHEEL_BASE_CM = 10.5f;
static constexpr float NODE_DISTANCE_M = 0.50f;
static constexpr int PULSES_PER_NODE = 52;  // tăng từ 49: bù sai số ~10cm/đoạn (bánh nén nhỏ hơn danh nghĩa)

static constexpr unsigned long LOOP_INTERVAL_MS = 20;
static constexpr unsigned long TELEMETRY_INTERVAL_MS = 100;
static constexpr unsigned long DEBUG_INTERVAL_MS = 500;
static constexpr unsigned long MANUAL_TIMEOUT_MS = 700;
static constexpr unsigned long WIFI_CHECK_INTERVAL_MS = 5000;

static constexpr int DRIVE_SPEED_PWM = 175;
static constexpr int DRIVE_MIN_PWM = 135;
static constexpr int DRIVE_MAX_PWM = 220;
static constexpr int DRIVE_SLEW_STEP = 18;
static constexpr int DRIVE_SLOWDOWN_PULSES = 12;
static constexpr int DRIVE_STEER_MAX = 50;
static constexpr float DRIVE_HEADING_GAIN = 175.0f;
static constexpr float DRIVE_ENCODER_BALANCE_GAIN = 1.2f;
static constexpr float DRIVE_HEADING_DEADBAND_RAD = 1.0f * PI_F / 180.0f;

// Ngưỡng bỏ qua xoay: heading error < 10° thì đi thẳng luôn
static constexpr float SKIP_ROTATE_THRESHOLD_RAD = 10.0f * PI_F / 180.0f;

// Xoay nhanh: ước lượng thời gian xoay liên tục cho góc lớn, rồi nudge phần còn lại
static constexpr float FAST_ROTATE_THRESHOLD_RAD = 25.0f * PI_F / 180.0f;  // >25° dùng xoay nhanh
static constexpr float FAST_ROTATE_SPEED_DPS = 150.0f;  // tốc độ ước lượng (°/s) ở PWM xoay nhanh
static constexpr int FAST_ROTATE_PWM = 200;              // PWM cho xoay trái/phải (góc nhỏ)
static constexpr int FAST_ROTATE_REVERSE_PWM = 210;      // PWM cho quay đầu (giảm 220→210 tránh trượt quá 180°)
static constexpr float FAST_ROTATE_REVERSE_RAD = 120.0f * PI_F / 180.0f;  // ngưỡng chuyển sang PWM thấp
static constexpr float FAST_ROTATE_UNDERSHOOT = 0.85f;  // xoay 85% thời gian ước lượng (tránh trượt quá 180°)

// Chờ robot dừng hẳn + IMU hết rung sau motor.stop() trước khi đo heading lần đầu (tránh drift đầu xoay).
// Fast-rotate RE-ANCHOR _fastRotateUntilMs sau mốc này → settle KHÔNG ăn vào thời gian ước lượng.
static constexpr unsigned long PRE_ROTATE_SETTLE_MS = 300;

// Xoay nudge: burst cố định → dừng → đo → lặp lại cho tinh chỉnh góc nhỏ
static constexpr float NUDGE_DONE_TOL_RAD = 4.0f * PI_F / 180.0f;
static constexpr int ROTATE_DIRECTION_SIGN = 1;
static constexpr int NUDGE_ROTATE_PWM = 175;
static constexpr int NUDGE_MAX_PWM = 220;
static constexpr int NUDGE_PWM_STEP = 10;
static constexpr float NUDGE_MIN_PROGRESS_RAD = 1.0f * PI_F / 180.0f;
static constexpr unsigned long NUDGE_BURST_MIN_MS = 30;   // burst ngắn khi gần đích (≥2 loop ticks)
static constexpr unsigned long NUDGE_BURST_MAX_MS = 55;    // burst dài khi còn xa
static constexpr float NUDGE_OVER_PROGRESS_RAD = 3.0f * PI_F / 180.0f;  // ngưỡng xoay quá nhiều → giảm PWM
static constexpr int NUDGE_PWM_DOWN_STEP = 15;  // giảm nhanh hơn tăng (10 lên, 15 xuống)
static constexpr unsigned long NUDGE_SETTLE_MS = 220;
static constexpr unsigned long ROTATE_TIMEOUT_MS = 60000;  // tăng 30s→60s: đủ buffer cho 180° khi I2C retry hoặc motor yếu
static constexpr float ROTATE_MAX_OVERSHOOT_RAD = 60.0f * PI_F / 180.0f;  // tổng xoay vượt error+60° → dừng (chống xoay vòng)

// Bước test: điều khiển motor trực tiếp, không qua navigator/IMU
static constexpr int TEST_STEP_DRIVE_PWM = 160;
static constexpr int TEST_STEP_TURN_PWM = 180;
static constexpr unsigned long TEST_STEP_DURATION_MS = 400;

static constexpr unsigned long NODE_PAUSE_MS = 180;
static constexpr int ROUTE_MAX_WAYPOINTS = 64;

static constexpr unsigned long BUZZER_ON_MS = 75;
static constexpr unsigned long BUZZER_GAP_MS = 95;

// --- Servo quét cảm biến (điều khiển LEDC thô 50Hz) ---
static constexpr int SERVO_PWM_CH = 2;          // kênh LEDC riêng (motor dùng 0,1 → timer khác)
static constexpr int SERVO_PWM_FREQ = 50;       // tần số chuẩn servo (chu kỳ 20ms)
static constexpr int SERVO_PWM_BITS = 16;       // độ phân giải duty 0..65535
static constexpr int SERVO_MIN_PULSE_US = 1000; // xung ứng với 0° (chuẩn hobby SG90)voi
static constexpr int SERVO_MAX_PULSE_US = 2000; // xung ứng với 180° (chuẩn hobby SG90)
static constexpr int SERVO_CENTER_DEG = 90;     // nhìn thẳng phía trước
static constexpr int SERVO_LEFT_DEG = 180;      // nhìn sang trái  — 90° từ center 
static constexpr int SERVO_RIGHT_DEG = 0;       // nhìn sang phải — 90° từ center 
static constexpr unsigned long SERVO_SETTLE_MS = 350;  // chờ servo quay xong mới đo

// --- Chế độ tự hành né vật cản ---
static constexpr float AUTO_STOP_DISTANCE_CM = 30.0f;    // <30cm phía trước → dừng + quét
static constexpr float AUTO_TURN_CLEARANCE_CM = 25.0f;   // cần ≥25cm cả 2 bên mới quay đầu 180°
static constexpr int AUTO_DRIVE_PWM = 160;               // tốc độ chạy thẳng tự hành (chậm hơn cruise)
static constexpr float AUTO_AVOID_TURN_DEG = 90.0f;      // góc né trái/phải
static constexpr unsigned long AUTO_DRIVE_SAMPLE_MS = 50; // chu kỳ đo sonar khi đang chạy thẳng
static constexpr unsigned long AUTO_STOP_SETTLE_MS = 500;// chờ L298N inductive spike tan trước khi quét servo

}