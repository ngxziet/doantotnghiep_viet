#pragma once

#include <Arduino.h>

namespace RobotConfig {

static constexpr float PI_F = 3.14159265358979323846f;

static constexpr int MOTOR_LEFT_IN1 = 16;
static constexpr int MOTOR_LEFT_IN2 = 17;
static constexpr int MOTOR_LEFT_ENA = 25;
static constexpr int MOTOR_RIGHT_IN3 = 18;
static constexpr int MOTOR_RIGHT_IN4 = 19;
static constexpr int MOTOR_RIGHT_ENB = 33;

static constexpr int ENCODER_LEFT_PIN = 34;
static constexpr int ENCODER_RIGHT_PIN = 35;

static constexpr int MPU6050_SDA_PIN = 21;
static constexpr int MPU6050_SCL_PIN = 22;

static constexpr int ULTRASONIC_TRIG_PIN = 5;
static constexpr int ULTRASONIC_ECHO_PIN = 26;

static constexpr int BUZZER_PIN = 4;

static constexpr const char* WIFI_SSID = "RobotCar";
static constexpr const char* WIFI_PASS = "12345678";
static constexpr int WIFI_CHANNEL = 6;

static constexpr int ENCODER_PULSES_PER_REV = 20;
static constexpr float WHEEL_DIAM_CM = 6.5f;
static constexpr float WHEEL_CIRCUMFERENCE_CM = PI_F * WHEEL_DIAM_CM;
static constexpr float CM_PER_PULSE = WHEEL_CIRCUMFERENCE_CM / ENCODER_PULSES_PER_REV;

static constexpr float WHEEL_BASE_CM = 10.5f;
static constexpr float NODE_DISTANCE_M = 0.50f;
static constexpr int PULSES_PER_NODE = 49;

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
static constexpr int DRIVE_STEER_MAX = 45;
static constexpr float DRIVE_HEADING_GAIN = 95.0f;
static constexpr float DRIVE_ENCODER_BALANCE_GAIN = 2.2f;

// Nudge rotation: fixed-burst step-stop-measure for all rotation
static constexpr float NUDGE_DONE_TOL_RAD = 4.0f * PI_F / 180.0f;
static constexpr int ROTATE_DIRECTION_SIGN = 1;
static constexpr int NUDGE_ROTATE_PWM = 160;
static constexpr int NUDGE_MAX_PWM = 220;
static constexpr int NUDGE_PWM_STEP = 10;
static constexpr float NUDGE_MIN_PROGRESS_RAD = 1.0f * PI_F / 180.0f;
static constexpr unsigned long NUDGE_BURST_MS = 50;
static constexpr unsigned long NUDGE_SETTLE_MS = 150;
static constexpr unsigned long ROTATE_TIMEOUT_MS = 9000;

// Test step: direct motor control, no navigator/IMU
static constexpr int TEST_STEP_DRIVE_PWM = 160;
static constexpr int TEST_STEP_TURN_PWM = 180;
static constexpr unsigned long TEST_STEP_DURATION_MS = 400;

static constexpr unsigned long NODE_PAUSE_MS = 180;
static constexpr int ROUTE_MAX_WAYPOINTS = 64;

static constexpr unsigned long BUZZER_ON_MS = 75;
static constexpr unsigned long BUZZER_GAP_MS = 95;

}