# Do An Tot Nghiep - Autonomous Robot Car

Firmware ESP32 va ung dung Flutter dieu khien xe tu hanh nho. He thong cho phep nguoi dung chon diem dich tren ban do, tinh duong bang A*, gui waypoint qua UDP den ESP32 va nhan telemetry de hien thi vi tri xe theo thoi gian thuc.

## Highlights

- Tap-to-navigate tren ban do waypoint.
- A* path planning voi kha nang block/unblock edge de mo phong vat can.
- ESP32 WiFi Access Point, giao tiep UDP truc tiep voi app.
- Firmware module hoa: motor driver, node navigator (nudge rotation), autonomous explorer (obstacle avoidance), odometry, encoder, IMU, calibration, ultrasonic, pan servo.
- Che do tu hanh ne vat can: HC-SR04 tren servo quet trai/phai, tu dung va chon huong thong thoang.
- Flutter app co 4 man hinh: Map, Test, Control, Autonomous (Start/Stop tu hanh + telemetry).
- Simulator mode de test app khi chua co hardware.
- Unit tests cho A*, map model, waypoint builder, robot state va odometry math.

## System Architecture

```text
User tap goal node
        |
        v
Flutter MapScreen
        |
        v
A* path planning -> waypoint list
        |
        v
UDP command, port 4211
        |
        v
ESP32 waypoint follower -> PID -> L298N motor driver
        |
        v
Encoder + IMU odometry
        |
        v
UDP telemetry, port 4210 -> Flutter realtime map
```

## Tech Stack

| Layer | Technology | Purpose |
| --- | --- | --- |
| Firmware | ESP32, C++, PlatformIO, Arduino | Motor control, sensors, odometry, UDP server |
| Mobile | Flutter, Dart | Map UI, route planning, manual control, telemetry |
| Communication | UDP JSON | Low-latency command and telemetry exchange |
| Map | JSON graph | Nodes, edges, coordinates, runtime blocked roads |

## Repository Structure

```text
.
+-- firmware/
|   +-- platformio.ini
|   +-- src/
|       +-- communication/      # UDP command and telemetry
|       +-- controllers/        # Motor driver, node navigator, PID (unused)
|       +-- calibration/        # Auto-tune motor trim, NVS persistence
|       +-- localization/       # Odometry (encoder 65% + IMU 35%)
|       +-- sensors/            # Encoder, MPU6050 (raw I2C), HC-SR04
|       +-- main.cpp            # Main firmware loop (50Hz)
|       +-- l298n_test.cpp      # Standalone motor driver test env
+-- mobile/
|   +-- assets/environment.json # Map bundled into app
|   +-- lib/
|   |   +-- models/
|   |   +-- planning/
|   |   +-- screens/
|   |   +-- services/
|   +-- test/
|   +-- pubspec.yaml
+-- map_data/
|   +-- environment.json        # Source map data
+-- docs/
|   +-- codebase-summary.md
```

## Hardware

Core hardware target:

- ESP32 DevKit board
- L298N motor driver
- 2 DC motors with LM393 encoders
- MPU-6050 IMU
- HC-SR04 ultrasonic sensor
- SG90 servo panning the HC-SR04 (GPIO19) for the autonomous mode
- Robot chassis, battery, wiring and common ground

Default ESP32 pin mapping is documented in `firmware/src/config.h`.

## Quick Start

### 1. Clone

```bash
git clone https://github.com/ngxziet/doantotnghiep.git
cd doantotnghiep
```

### 2. Run Mobile App

```bash
cd mobile
flutter pub get
flutter test
flutter run
```

By default, the app uses the real UDP transport. To test without ESP32:

```bash
flutter run --dart-define=USE_SIMULATOR=true
```

### 3. Build And Upload Firmware

```bash
cd firmware
pio run
pio run --target upload
pio device monitor
```

Standalone L298N motor test:

```bash
cd firmware
pio run -e l298n_test --target upload
pio device monitor
```

## ESP32 Network

The firmware starts a WiFi Access Point:

| Field | Value |
| --- | --- |
| SSID | `RobotCar` |
| Password | `12345678` |
| ESP32 IP | `192.168.4.1` |
| Telemetry port | `4210` |
| Command port | `4211` |

Connect the phone or development machine running the Flutter app to `RobotCar` before using real hardware mode.

## UDP Protocol

### Telemetry: ESP32 -> App

```json
{
  "pose": {
    "x": 0.35,
    "y": 0.12,
    "theta": 1.57
  },
  "obstacle_dist_cm": 45.2,
  "status": "moving"
}
```

### Waypoint Command: App -> ESP32

```json
{
  "seq": 1,
  "waypoints": [
    { "x": 0.5, "y": 0.0 },
    { "x": 1.0, "y": 0.0 }
  ]
}
```

### Manual Command

```json
{
  "seq": 2,
  "type": "manual",
  "command": "forward",
  "speed": 180
}
```

Supported manual commands:

- `forward`
- `backward`
- `left`
- `right`
- `left_forward`
- `left_backward`
- `right_forward`
- `right_backward`
- `stop`

### Autonomous Mode

```json
{
  "seq": 4,
  "type": "autonomous",
  "command": "start"
}
```

`command` is `start` (enable obstacle-avoidance autopilot) or `stop`. While active the robot drives straight, stops at obstacles under 30 cm, pans the HC-SR04 servo left/right and picks the clearer side (90 deg turn), does a 180 deg U-turn when both sides are blocked but >=25 cm clear, or stops when boxed in.

### Reset Pose

```json
{
  "seq": 3,
  "type": "reset_pose"
}
```

Supported status values:

- `idle`
- `moving`
- `arrived`
- `exploring` (autonomous: driving straight)
- `scanning` (autonomous: panning servo to measure sides)
- `avoiding` (autonomous: turning to avoid)
- `stuck` (autonomous: boxed in, stopped)
- `obstacle_detected`
- `emergency_stop`
- `imu_missing`
- `calibrating`
- `calibrated`
- `rotation_timeout`
- `encoder_mismatch`

## Coordinate Convention

The app map uses screen-style coordinates:

- `x` increases to the right.
- `y` increases downward on the map.
- Firmware converts map waypoints into robot yaw before steering.
- Telemetry `theta = 0` points right on the map.
- Positive `theta` is standard robot yaw, so the app inverts screen Y when drawing the realtime arrow.

This keeps route planning, firmware steering and app visualization aligned.

## Development Commands

Firmware:

```bash
cd firmware
pio run
pio run --target upload
pio device monitor
```

Mobile:

```bash
cd mobile
flutter pub get
flutter analyze
flutter test
flutter run
```

## Tests

Current mobile tests cover:

- A* shortest path and unreachable route cases
- Road map loading, neighbors and disabled edges
- Waypoint builder
- Robot telemetry parser
- Odometry math
- Basic widget smoke test

Run:

```bash
cd mobile
flutter test
```

## Current Status

- Firmware modules are implemented for motor control, UDP command parsing, odometry, calibration and node-based navigation.
- Three-tier rotation: fast rotate for large angles (PWM 200 for <120°, PWM 220 for >120° with cross-target overshoot detection), proportional nudge burst (30-55ms scaled by remaining angle, adaptive PWM), skip for <10° error. Rotation overshoot guard stops nudging if total rotation exceeds initial error + 60°.
- Heading snap + IMU reference sync after each rotation and at each waypoint, preventing heading error accumulation across segments.
- Straight driving uses proportional heading correction (IMU gain=175, 1° deadband) with encoder balance and slew rate limiting.
- Collinear node optimization: heading error below 10 degrees skips rotation; consecutive straight nodes are driven without stopping.
- Auto-calibration: motor trim learns 5% per segment, encoder scale tracked, persisted to NVS flash.
- Autonomous obstacle-avoidance mode (autonomous_explorer): drive straight, stop under 30 cm, servo-scan both sides, turn 90 deg toward the clearer side (tie -> left), 180 deg U-turn when both sides blocked but >=25 cm clear, stop when boxed in. Pan servo on GPIO19 (raw LEDC channel 2, 50 Hz).
- Flutter app is implemented for map navigation, connection testing, manual control and an Autonomous tab (Start/Stop + live telemetry). The old Wheels test tab was replaced by it.
- Simulator service is available for app testing without hardware.

## Documentation

- [Firmware Algorithm Guide](docs/firmware-algorithm-guide.html) — detailed walkthrough of all firmware algorithms with step-by-step reading order (Vietnamese).

## Notes

- Keep generated files out of git: `.pio/`, `.dart_tool/`, `build/`, Gradle cache and local IDE state are ignored.
- Keep `map_data/environment.json` and `mobile/assets/environment.json` in sync when changing the map.
- Do not commit local WiFi credentials, keystores or environment files.
