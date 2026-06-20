#include <Arduino.h>
#include <WiFi.h>
#include <math.h>
#include <string.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "config.h"
#include "calibration/calibration_manager.h"
#include "communication/udp_server.h"
#include "controllers/motor_driver.h"
#include "controllers/node_navigator.h"
#include "controllers/autonomous_explorer.h"
#include "localization/odometry.h"
#include "sensors/encoder.h"
#include "sensors/imu_mpu6050.h"
#include "sensors/ultrasonic_hcsr04.h"
#include "sensors/sensor_pan_servo.h"

MotorDriver motors(RobotConfig::MOTOR_LEFT_IN1,
                   RobotConfig::MOTOR_LEFT_IN2,
                   RobotConfig::MOTOR_LEFT_ENA,
                   RobotConfig::MOTOR_RIGHT_IN3,
                   RobotConfig::MOTOR_RIGHT_IN4,
                   RobotConfig::MOTOR_RIGHT_ENB);
Encoder encLeft(RobotConfig::ENCODER_LEFT_PIN);
Encoder encRight(RobotConfig::ENCODER_RIGHT_PIN);
ImuMpu6050 imu;
UltrasonicHcsr04 ultrasonic(RobotConfig::ULTRASONIC_TRIG_PIN,
                            RobotConfig::ULTRASONIC_ECHO_PIN);
SensorPanServo scanServo(RobotConfig::SERVO_PIN);
Odometry odometry(RobotConfig::WHEEL_BASE_CM);
UdpServer udpServer;
NodeNavigator navigator;
CalibrationManager calibration;
AutonomousExplorer explorer(motors, ultrasonic, scanServo, navigator);

static constexpr bool USE_ULTRASONIC = false;

static float routeXs[UDP_MAX_WAYPOINTS];
static float routeYs[UDP_MAX_WAYPOINTS];

static unsigned long lastLoopMs = 0;
static unsigned long lastTelemetryMs = 0;
static unsigned long lastDebugMs = 0;
static unsigned long lastWifiCheckMs = 0;
static unsigned long manualStartMs = 0;
static unsigned long manualDurationMs = 0;
static int manualLeftSpeed = 0;
static int manualRightSpeed = 0;
static bool hadStation = false;
static bool imuReady = false;
static char overrideStatus[24] = "";
static unsigned long overrideStartMs = 0;
static unsigned long overrideDurationMs = 0;

static float normalizeAngle(float angle) {
    if (!isfinite(angle)) return 0.0f;
    while (angle > PI) angle -= 2.0f * PI;
    while (angle < -PI) angle += 2.0f * PI;
    return angle;
}

static void setTemporaryStatus(const char* status, unsigned long durationMs) {
    strncpy(overrideStatus, status, sizeof(overrideStatus) - 1);
    overrideStatus[sizeof(overrideStatus) - 1] = '\0';
    overrideStartMs = millis();
    overrideDurationMs = durationMs;
}

static bool testStepActive = false;
static float savedStepX = 0.0f;
static float savedStepY = 0.0f;

static void stopManual() {
    if (testStepActive) {
        Pose pose = odometry.getPose();
        odometry.setPose(savedStepX, savedStepY, pose.theta);
    }
    manualDurationMs = 0;
    manualLeftSpeed = 0;
    manualRightSpeed = 0;
    testStepActive = false;
}

static void applyManualCommand(const UdpCommand& command) {
    int requested = constrain(command.manualSpeed, 0, 255);
    int driveSpeed = requested;
    int turnSpeed = requested;
    if (driveSpeed > 0 && driveSpeed < 145) driveSpeed = 145;
    if (turnSpeed > 0 && turnSpeed < 190) turnSpeed = 190;

    manualLeftSpeed = 0;
    manualRightSpeed = 0;

    if (strcmp(command.manualCommand, "forward") == 0) {
        manualLeftSpeed = driveSpeed;
        manualRightSpeed = driveSpeed;
    } else if (strcmp(command.manualCommand, "backward") == 0) {
        manualLeftSpeed = -driveSpeed;
        manualRightSpeed = -driveSpeed;
    } else if (strcmp(command.manualCommand, "left") == 0) {
        manualLeftSpeed = -turnSpeed;
        manualRightSpeed = turnSpeed;
    } else if (strcmp(command.manualCommand, "right") == 0) {
        manualLeftSpeed = turnSpeed;
        manualRightSpeed = -turnSpeed;
    } else if (strcmp(command.manualCommand, "left_forward") == 0) {
        manualLeftSpeed = driveSpeed;
        manualRightSpeed = 0;
    } else if (strcmp(command.manualCommand, "left_backward") == 0) {
        manualLeftSpeed = -driveSpeed;
        manualRightSpeed = 0;
    } else if (strcmp(command.manualCommand, "right_forward") == 0) {
        manualLeftSpeed = 0;
        manualRightSpeed = driveSpeed;
    } else if (strcmp(command.manualCommand, "right_backward") == 0) {
        manualLeftSpeed = 0;
        manualRightSpeed = -driveSpeed;
    }

    navigator.clear();
    manualStartMs = millis();
    manualDurationMs = RobotConfig::MANUAL_TIMEOUT_MS;
}

static void applyStepCommand(const UdpCommand& command) {
    stopManual();
    motors.stop();
    navigator.clear();

    int drive = RobotConfig::TEST_STEP_DRIVE_PWM;
    int turn = RobotConfig::TEST_STEP_TURN_PWM;

    manualLeftSpeed = 0;
    manualRightSpeed = 0;

    if (strcmp(command.stepAction, "forward") == 0) {
        manualLeftSpeed = drive;
        manualRightSpeed = drive;
    } else if (strcmp(command.stepAction, "backward") == 0) {
        manualLeftSpeed = -drive;
        manualRightSpeed = -drive;
    } else if (strcmp(command.stepAction, "left") == 0) {
        manualLeftSpeed = -turn;
        manualRightSpeed = turn;
    } else if (strcmp(command.stepAction, "right") == 0) {
        manualLeftSpeed = turn;
        manualRightSpeed = -turn;
    } else {
        motors.stop();
        return;
    }

    Pose pose = odometry.getPose();
    savedStepX = pose.x;
    savedStepY = pose.y;
    testStepActive = true;
    manualStartMs = millis();
    manualDurationMs = RobotConfig::TEST_STEP_DURATION_MS;
}

static void resetPose() {
    motors.stop();
    navigator.clear();
    stopManual();
    encLeft.reset();
    encRight.reset();
    encLeft.setDirection(0);
    encRight.setDirection(0);
    if (imuReady) {
        imu.resetYaw();
        imu.setReferenceAngle();
    }
    odometry.reset(encLeft.getPulseCount(), encRight.getPulseCount());
}

static void calibrateImu() {
    motors.stop();
    navigator.clear();
    stopManual();
    if (!imuReady) {
        imuReady = imu.begin();
    }
    if (imuReady) {
        setTemporaryStatus("calibrating", 3500);
        Serial.println("IMU calibration requested. Keep robot still...");
        imu.calibrate(1000);
        imu.setReferenceAngle();
        Pose pose = odometry.getPose();
        odometry.setPose(pose.x, pose.y, 0.0f);
        calibration.markImuCalibrated(imu.isHealthy());
        Serial.println("IMU calibration done.");
    } else {
        calibration.markImuCalibrated(false);
        setTemporaryStatus("imu_missing", 3000);
    }
}

static void handleCommand(const UdpCommand& command) {
    switch (command.type) {
        case UdpCommandType::ResetPose:
            explorer.stop();
            resetPose();
            Serial.printf("Pose reset: seq=%d\n", command.seq);
            break;
        case UdpCommandType::Manual:
            explorer.stop();
            applyManualCommand(command);
            Serial.printf("Manual: seq=%d cmd=%s speed=%d left=%d right=%d\n",
                          command.seq, command.manualCommand, command.manualSpeed,
                          manualLeftSpeed, manualRightSpeed);
            break;
        case UdpCommandType::Step:
            explorer.stop();
            applyStepCommand(command);
            Serial.printf("Step: seq=%d action=%s heading=%.1f\n",
                          command.seq, command.stepAction,
                          odometry.getPose().theta * 180.0f / PI);
            break;
        case UdpCommandType::Route: {
            explorer.stop();
            stopManual();
            Pose pose = odometry.getPose();
            navigator.setRoute(routeXs, routeYs, command.waypointCount, pose,
                               encLeft, encRight);
            Serial.printf("Route: seq=%d nodes=%d start=(%.2f,%.2f) heading=%.1f\n",
                          command.seq, command.waypointCount, pose.x, pose.y,
                          pose.theta * 180.0f / PI);
            break;
        }
        case UdpCommandType::Calibrate:
            if (strcmp(command.calibrateMode, "imu") == 0) {
                calibrateImu();
            } else if (strcmp(command.calibrateMode, "motor_balance") == 0) {
                calibration.resetMotorBalance();
                setTemporaryStatus("calibrated", 2000);
            } else if (strcmp(command.calibrateMode, "encoder") == 0) {
                calibration.resetEncoderScale();
                setTemporaryStatus("calibrated", 2000);
            }
            Serial.printf("Calibrate: seq=%d mode=%s\n", command.seq, command.calibrateMode);
            break;
        case UdpCommandType::Autonomous:
            if (strcmp(command.autoCommand, "start") == 0) {
                stopManual();
                motors.stop();
                explorer.start();
            } else {
                explorer.stop();
                // Re-anchor IMU heading reference về hướng hiện tại
                // → sau khi robot tự hành né vật cản (xoay nhiều), reset ref
                //   để start lần sau có heading sạch, không mang drift cũ.
                if (imuReady) {
                    imu.setReferenceAngle();
                }
            }
            Serial.printf("Autonomous: seq=%d cmd=%s\n", command.seq, command.autoCommand);
            break;
        case UdpCommandType::Ping:
            setTemporaryStatus("idle", 700);
            break;
        case UdpCommandType::None:
            break;
    }
}

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    Serial.begin(115200);

    // Ép buzzer về HIGH ngay từ đầu setup() — module buzzer active LOW,
    // HIGH = im lặng, LOW = kêu. Tránh floating INPUT trong khoảng
    // boot → navigator.begin() chạy.
    pinMode(RobotConfig::BUZZER_PIN, OUTPUT);
    digitalWrite(RobotConfig::BUZZER_PIN, HIGH);

    motors.begin();
    encLeft.begin();
    encRight.begin();
    encLeft.setDirection(0);
    encRight.setDirection(0);
    navigator.begin();
    calibration.begin();
    scanServo.begin();

    // HC-SR04 luôn khởi tạo: chế độ tự hành dùng cảm biến dù USE_ULTRASONIC tắt
    ultrasonic.begin();

    udpServer.begin(RobotConfig::WIFI_SSID, RobotConfig::WIFI_PASS);
    Serial.println("WiFi AP ready: RobotCar  IP: 192.168.4.1");

    imuReady = imu.begin();
    if (imuReady) {
        Serial.println("Calibrating IMU at startup. Keep robot still...");
        imu.calibrate(1000);
        imu.setReferenceAngle();
        calibration.markImuCalibrated(imu.isHealthy());
        Serial.println("IMU ready. Heading reference = 0 deg.");
    } else {
        calibration.markImuCalibrated(false);
        Serial.println("MPU6050 missing. Route control will use encoder fallback only.");
    }

    resetPose();
    navigator.clear();
    lastLoopMs = millis();
    lastTelemetryMs = millis();
    lastDebugMs = millis();
    Serial.printf("System ready. WheelBase=%.1fcm pulses/node=%d cm/pulse=%.3f\n",
                  RobotConfig::WHEEL_BASE_CM,
                  RobotConfig::PULSES_PER_NODE,
                  RobotConfig::CM_PER_PULSE);
}

void loop() {
    unsigned long now = millis();

    // Theo dõi trạng thái WiFi station, CHỈ log, KHÔNG restart AP
    // (restart AP sẽ đá client đang cố reconnect → vòng lặp disconnect)
    if (now - lastWifiCheckMs >= RobotConfig::WIFI_CHECK_INTERVAL_MS) {
        lastWifiCheckMs = now;
        int stationCount = WiFi.softAPgetStationNum();
        if (stationCount > 0) {
            if (!hadStation) {
                Serial.println("WiFi station connected.");
            }
            hadStation = true;
        } else if (hadStation) {
            Serial.println("WiFi station lost, waiting for reconnect...");
            hadStation = false;
        }
    }

    UdpCommand command;
    if (udpServer.receiveCommand(routeXs, routeYs, &command)) {
        handleCommand(command);
    }

    if (now - lastLoopMs < RobotConfig::LOOP_INTERVAL_MS) {
        return;
    }
    lastLoopMs = now;

    bool imuConnected = false;
    if (imuReady) {
        imu.update();
        imuConnected = imu.isHealthy();
    }

    long leftPulses = encLeft.getPulseCount();
    long rightPulses = encRight.getPulseCount();
    float imuHeading = imuConnected ? imu.getAbsoluteYawRad() : 0.0f;
    bool suppressTranslation = navigator.isRotating() ||
        navigator.isStep() || testStepActive ||
        (motors.getLeftSpeed() * motors.getRightSpeed() < 0);
    odometry.update(leftPulses, rightPulses, imuHeading,
                    imuConnected, suppressTranslation);

    // Khi đi thẳng, ghi đè heading bằng IMU thuần.
    // Chênh lệch encoder hoàn toàn do heading correction, không phải xoay thật.
    if (navigator.isDrivingStraight() && imuConnected) {
        Pose p = odometry.getPose();
        odometry.setPose(p.x, p.y, imuHeading);
    }

    bool explorerActive = explorer.isActive();
    float obstacleDist = explorerActive ? explorer.lastFrontCm() : MAX_RANGE_CM;
    bool emergency = false;
    if (USE_ULTRASONIC && !explorerActive) {
        obstacleDist = ultrasonic.readDistanceCm();
        emergency = ultrasonic.isEmergency();
    }

    if (emergency) {
        motors.stop();
        navigator.clear();
        stopManual();
        setTemporaryStatus("emergency_stop", 1000);
    } else if (explorerActive) {
        stopManual();
        Pose pose = odometry.getPose();
        explorer.update(pose, encLeft, encRight, calibration, imuConnected, imuHeading);

        // Sau khi né (step turn) hoàn tất, navigator phát snap → đồng bộ heading
        if (navigator.hasSnap()) {
            Pose current = odometry.getPose();
            odometry.setPose(current.x, current.y, navigator.snapTheta());
            if (imuConnected) {
                imu.setAbsoluteYaw(navigator.snapTheta());
            }
            navigator.acknowledgeSnap();
        }
    } else if (manualDurationMs > 0 && (now - manualStartMs) < manualDurationMs) {
        motors.setLeft(manualLeftSpeed);
        motors.setRight(manualRightSpeed);
    } else {
        stopManual();
        Pose pose = odometry.getPose();
        navigator.update(pose, motors, encLeft, encRight, calibration, imuConnected, imuHeading);

        if (navigator.hasSnap()) {
            if (navigator.isStep()) {
                Pose current = odometry.getPose();
                odometry.setPose(current.x, current.y, navigator.snapTheta());
            } else {
                odometry.setPose(navigator.snapX(), navigator.snapY(), navigator.snapTheta());
            }
            // Đồng bộ IMU reference với heading vừa snap
            // → getAbsoluteYawRad() trả về snapTheta, tránh bị ghi đè bởi IMU lệch
            if (imuConnected) {
                imu.setAbsoluteYaw(navigator.snapTheta());
            }
            navigator.acknowledgeSnap();
        }
    }

    encLeft.setDirection(motors.getLeftSpeed());
    encRight.setDirection(motors.getRightSpeed());
    calibration.update();

    // --- Chẩn đoán cú quay: phân định bão hòa gyro (A) vs nhiễu I2C (B) ---
    // Mở cửa sổ đo lúc bắt đầu quay, in kết quả lúc kết thúc.
    static bool diagWasRotating = false;
    static float diagStartHeadingDeg = 0.0f;
    static unsigned long diagStartMs = 0;
    bool diagRotating = navigator.isRotating();
    if (diagRotating && !diagWasRotating) {
        imu.beginDiagWindow();
        diagStartHeadingDeg = imuConnected ? imu.getAbsoluteYawDeg()
                                           : odometry.getPose().theta * 180.0f / PI;
        diagStartMs = now;
    } else if (!diagRotating && diagWasRotating) {
        float endHeadingDeg = imuConnected ? imu.getAbsoluteYawDeg()
                                           : odometry.getPose().theta * 180.0f / PI;
        float rotatedDeg = normalizeAngle((endHeadingDeg - diagStartHeadingDeg) * PI / 180.0f)
                           * 180.0f / PI;
        int peakGz = imu.peakAbsGzRaw();
        Serial.printf("[ROT] turned=%.1fdeg dur=%lums peakGz=%d (~%.0fdeg/s)%s readErr=%lu healthyDrop=%d\n",
                      rotatedDeg, now - diagStartMs,
                      peakGz, peakGz / 131.0f,
                      peakGz >= 32000 ? " <<SATURATED" : "",
                      imu.readErrorsInWindow(),
                      imu.healthyDroppedInWindow() ? 1 : 0);
    }
    diagWasRotating = diagRotating;

    const char* status = navigator.status();
    if (explorer.isActive()) {
        status = explorer.status();
    }
    bool manualActive = manualDurationMs > 0 && (now - manualStartMs) < manualDurationMs;
    if (manualActive) {
        status = (manualLeftSpeed == 0 && manualRightSpeed == 0) ? "idle" : "moving";
    }
    if (overrideStatus[0] && (now - overrideStartMs) < overrideDurationMs) {
        status = overrideStatus;
    } else if (overrideStatus[0]) {
        overrideStatus[0] = '\0';
    }
    if (!imuConnected && navigator.isActive()) {
        status = "imu_missing";
    }

    if (now - lastTelemetryMs >= RobotConfig::TELEMETRY_INTERVAL_MS) {
        lastTelemetryMs = now;
        Pose pose = odometry.getPose();
        udpServer.sendTelemetry(pose.x, pose.y, pose.theta, obstacleDist, status,
                                imuConnected, calibration.isCalibrated(),
                                navigator.currentIndex(), navigator.waypointCount());
    }

    if (now - lastDebugMs >= RobotConfig::DEBUG_INTERVAL_MS) {
        lastDebugMs = now;
        Pose pose = odometry.getPose();
        Serial.printf("pose=(%.2f,%.2f,%.1fdeg) status=%s imu=%d motor=%d/%d enc=%ld/%ld node=%d/%d trim=%.2f/%.2f\n",
                      pose.x, pose.y, normalizeAngle(pose.theta) * 180.0f / PI,
                      status, imuConnected ? 1 : 0,
                      motors.getLeftSpeed(), motors.getRightSpeed(),
                      leftPulses, rightPulses,
                      navigator.currentIndex(), navigator.waypointCount(),
                      calibration.leftTrim(), calibration.rightTrim());
    }
}