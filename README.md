# Do An Tot Nghiep - Autonomous Robot Car

Firmware ESP32 va ung dung Flutter dieu khien xe tu hanh nho. He thong cho phep nguoi dung chon diem dich tren ban do, tinh duong bang A*, gui waypoint qua UDP den ESP32 va nhan telemetry de hien thi vi tri xe theo thoi gian thuc.

## Highlights

- Tap-to-navigate tren ban do waypoint.
- A* path planning voi kha nang block/unblock edge de mo phong vat can.
- ESP32 WiFi Access Point, giao tiep UDP truc tiep voi app.
- Firmware module hoa: motor driver, node navigator (nudge rotation), odometry, encoder, IMU, calibration, ultrasonic.
- Flutter app co 4 man hinh: Map, Test, Control, Wheels/Telemetry.
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
- `obstacle_detected`
- `emergency_stop`
- `arrived`
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
- Rotation uses nudge step-stop-measure algorithm: fixed PWM burst, stop, read IMU when stationary, repeat until within 4 degree tolerance. Works consistently across different surfaces.
- Straight driving uses proportional heading correction (encoder 65% + IMU 35%) with slew rate limiting.
- Flutter app is implemented for map navigation, connection testing, manual control and telemetry.
- Simulator service is available for app testing without hardware.
- Remaining real-world work: hardware calibration, nudge parameter tuning, experimental error measurement and demo recording.

## Notes

- Keep generated files out of git: `.pio/`, `.dart_tool/`, `build/`, Gradle cache and local IDE state are ignored.
- Keep `map_data/environment.json` and `mobile/assets/environment.json` in sync when changing the map.
- Do not commit local WiFi credentials, keystores or environment files.
