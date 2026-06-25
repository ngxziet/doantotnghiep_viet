# Đồ Án Tốt Nghiệp — Xe Tự Hành (Autonomous Robot Car)

Firmware ESP32 + ứng dụng Flutter điều khiển xe tự hành nhỏ. Hệ thống cho phép chọn điểm đích trên bản đồ, tính đường bằng A*, gửi waypoint qua UDP tới ESP32 và nhận telemetry để hiển thị vị trí xe theo thời gian thực. Chế độ tự hành né vật cản dùng HC-SR04 + servo quét trái/phải.

## Highlights

- Tap-to-navigate trên bản đồ waypoint (105 node, 172 cạnh, lưới 5×5 m).
- A* path planning với heuristic Euclidean + phạt rẽ, hỗ trợ block/unblock edge runtime để mô phỏng vật cản.
- ESP32 phát WiFi Access Point, giao tiếp UDP JSON trực tiếp với app.
- Firmware module hoá: motor driver, node navigator (xoay 3 tầng + đi thẳng), autonomous explorer (né vật cản phản ứng), odometry (encoder + IMU), encoder, IMU, calibration, ultrasonic, pan servo.
- Chế độ tự hành né vật cản: HC-SR04 trên servo quét trái/phải, dừng dưới 30 cm, chọn hướng thông thoáng (90°) hoặc quay đầu 180° khi cả hai bên kẹt nhưng còn ≥25 cm.
- 4 màn hình Flutter: **Map** (tap → A* → gửi waypoint), **ESP32 Test** (telemetry trực tiếp + ping/calibrate), **Control** (điều khiển tay bằng step), **Tự hành** (Start/Stop + telemetry).
- Simulator mode để test app khi chưa có hardware (`--dart-define=USE_SIMULATOR=true`).
- 33 unit tests: A*, RoadMap, waypoint builder, robot state, odometry math, smoke test widget.

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
ESP32 node navigator -> PID-like PWM -> L298N motor driver
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
| Communication | UDP JSON | Low-latency command & telemetry exchange |
| Map | JSON graph | Nodes, edges, coordinates, runtime blocked roads |

## Repository Structure

```text
.
+-- firmware/
|   +-- platformio.ini               # 5 envs: esp32dev, l298n_test, servo_test,
|                                    #          encoder_pulse_test, encoder_calib_test
|   +-- src/
|       +-- main.cpp                 # Main firmware loop (50 Hz)
|       +-- config.h                 # All tunables (pins, PWM, thresholds, IMU)
|       +-- communication/           # udp_server (JSON parse/broadcast)
|       +-- controllers/             # motor_driver, node_navigator, autonomous_explorer
|       +-- calibration/             # motor trim, encoder scale, IMU flag (NVS)
|       +-- localization/            # odometry (complementary filter)
|       +-- sensors/                 # encoder, MPU6050, HC-SR04, pan servo
|       +-- l298n_test.cpp           # Standalone motor driver test env
|       +-- servo_test.cpp           # Standalone servo + HC-SR04 test env
|       +-- encoder_pulse_test.cpp   # Standalone raw pulse-count test env
|       +-- encoder_calib_test.cpp   # Standalone PULSES_PER_REV calibration env
+-- mobile/
|   +-- assets/environment.json      # Map bundled into app
|   +-- lib/
|   |   +-- main.dart                # Entry: chọn UdpService/SimulatorService
|   |   +-- models/                  # RobotState, Pose, RoadMap/Node/Edge
|   |   +-- planning/                # A*, waypoint_builder
|   |   +-- services/                # udp_service, simulator_service
|   |   +-- screens/                 # main_shell, map, connection_test,
|   |                                #   manual_control, autonomous, painter, status
|   +-- test/                        # A*, RoadMap, waypoint, robot_state, odometry_calc, widget
|   +-- pubspec.yaml
+-- map_data/
|   +-- environment.json             # Source map data (105 node, 172 cạnh)
+-- docs/
|   +-- codebase-summary.md
|   +-- bao-cao-xe-tu-hanh.{md,html,pdf,docx}   # Báo cáo đồ án
|   +-- ket-qua-thuc-nghiem-5.2.xlsx            # Số liệu thực nghiệm Chương 5.2
```

## Hardware

Core hardware target:

- ESP32 DevKit
- L298N motor driver
- 2 DC motors với encoder LM393 **1 kênh xung** (chỉ D0/OUT, không quadrature A/B — firmware lấy chiều từ lệnh motor)
- MPU-6050 IMU (I2C raw, không dùng lib ngoài — `electroniccats/MPU6050` chỉ là fallback)
- HC-SR04 ultrasonic
- SG90 servo pan HC-SR04 (GPIO19, LEDC channel 2, 50 Hz)
- Buzzer active-LOW (GPIO4)
- Khung xe, pin, dây và **chung GND** với ESP32

Default ESP32 pin mapping is documented in `firmware/src/config.h`. Khi đổi chân / firmware lớn, tăng `CONFIG_VERSION` để reset calibration NVS.

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
flutter run                      # chạy với ESP32 thật (mặc định)
flutter run --dart-define=USE_SIMULATOR=true   # chạy giả lập (không cần ESP32)
```

`USE_SIMULATOR` đọc qua `String.fromEnvironment('USE_SIMULATOR')` trong `lib/main.dart`. Không cần sửa code.

### 3. Build & Upload Firmware

```bash
cd firmware
pio run                          # build mặc định (esp32dev)
pio run --target upload          # nạp qua USB
pio device monitor               # mở Serial 115200
```

Standalone test envs:

```bash
pio run -e l298n_test --target upload            # quay từng bánh theo chu kỳ
pio run -e servo_test --target upload            # servo 0/90/180° + đọc HC-SR04
pio run -e encoder_pulse_test --target upload    # đếm xung thô
pio run -e encoder_calib_test --target upload    # motor tự quay / hand-rotate để hiệu chuẩn
```

## ESP32 Network

Firmware phát WiFi AP:

| Field | Value |
| --- | --- |
| SSID | `RobotCar` |
| Password | `12345678` |
| ESP32 IP | `192.168.4.1` |
| Telemetry port | `4210` (broadcast `192.168.4.255:4210`) |
| Command port | `4211` |

Kết nối điện thoại / máy dev vào `RobotCar` trước khi dùng real hardware mode.

## UDP Protocol

### Telemetry: ESP32 → App (10 Hz, broadcast)

```json
{
  "pose": { "x": 0.35, "y": 0.12, "theta": 1.57 },
  "heading_deg": 90.0,
  "node_index": 2,
  "route_size": 5,
  "obstacle_dist_cm": 45.2,
  "status": "moving",
  "imu_connected": true,
  "calibrated": true
}
```

`pose.theta` chuẩn robot yaw; app inverts screen Y khi vẽ mũi tên.

### Route command: App → ESP32 (port 4211)

```json
{
  "seq": 1,
  "type": "route",
  "waypoints": [
    { "x": 0.5, "y": 0.0 },
    { "x": 1.0, "y": 0.0 }
  ]
}
```

App tự gửi **3 lần** cùng `seq`; firmware dedup bằng `last_seq`. Nếu `waypoints` rỗng → firmware chỉ cập nhật seq (hành vi ping).

### Manual command (hold ~700 ms mỗi lần gửi)

```json
{ "seq": 2, "type": "manual", "command": "forward", "speed": 180 }
```

`command` ∈ `forward | backward | left | right | left_forward | left_backward | right_forward | right_backward`. App Manual Control chủ yếu dùng **step** (dưới) — manual chỉ giữ trong khoảng `MANUAL_TIMEOUT_MS = 700`.

### Step command (discrete test step)

```json
{ "seq": 3, "type": "step", "action": "forward" }
```

`action` ∈ `forward | backward | left | right`. Mỗi step chạy `TEST_STEP_DURATION_MS = 400 ms` rồi dừng.

### Autonomous mode (obstacle-avoidance autopilot)

```json
{ "seq": 4, "type": "autonomous", "command": "start" }
```

`command` ∈ `start | stop`. Khi chạy, robot chạy thẳng, dừng khi vật cản < 30 cm, quét servo trái/phải và chọn hướng thông thoáng (90° turn), quay đầu 180° khi cả hai bên kẹt nhưng còn ≥25 cm, dừng khi bị bao vây.

### Reset pose

```json
{ "seq": 5, "type": "reset_pose" }
```

Đặt lại odometry về (0, 0, 0) và re-anchor IMU reference.

### Calibration

```json
{ "seq": 6, "type": "calibrate", "mode": "imu" }
```

`mode` ∈ `imu | motor_balance | encoder`. `imu` chạy `imu.calibrate(1000)` (samples=1000).

### Ping (giả lập)

App gửi route command với `waypoints=[]` để ping; firmware parse thấy `seq` mới và `!waypoints` → trả về `UdpCommandType::Ping` (chỉ cập nhật seq, không đổi trạng thái).

### Status values

| Status | Nguồn |
| --- | --- |
| `idle` | Navigator idle / explorer off |
| `moving` | Navigator đang chạy thẳng giữa node |
| `arrived` | Hết waypoint |
| `exploring` | Autonomous: đang chạy thẳng |
| `scanning` | Autonomous: servo quét trái/phải |
| `avoiding` | Autonomous: đang xoay né |
| `stuck` | Autonomous: bị bao vây, dừng |
| `obstacle_detected` | Tạm thời (legacy) |
| `emergency_stop` | Vật cản < 10 cm trong khi bám route (khi `USE_ULTRASONIC=true`) |
| `imu_missing` | MPU-6050 không phản hồi |
| `calibrating` | Đang hiệu chuẩn IMU |
| `calibrated` | Báo calibration reset xong |
| `rotation_timeout` | NodeNavigator xoay quá `ROTATE_TIMEOUT_MS = 60s` |

## Coordinate Convention

App map dùng screen-style coordinates:

- `x` tăng sang phải.
- `y` tăng xuống dưới trên bản đồ.
- Firmware chuyển map waypoints → robot yaw trước khi lái.
- Telemetry `theta = 0` chỉ sang phải.
- `theta` dương là standard robot yaw; app inverts screen Y khi vẽ mũi tên realtime.

Giữ route planning, firmware steering và app visualization luôn khớp.

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
flutter run --dart-define=USE_SIMULATOR=true
```

## Tests

Mobile tests (chạy `flutter test`):

- **A*** (`astar_test.dart`, 7 tests): đường ngắn nhất, unreachable, disable edge, equal distance turn penalty, start == goal.
- **RoadMap** (`road_map_test.dart`, 8 tests): load JSON, getNeighbors, disable/enable edge, nearest node.
- **Waypoint builder** (`waypoint_builder_test.dart`, 3 tests): build waypoints từ path node IDs.
- **Robot telemetry parser** (`robot_state_test.dart`, 8 tests): JSON hợp lệ / lỗi → null, defaults, status fallback.
- **Odometry math** (`odometry_calc_test.dart`, 7 tests): position update, complementary filter.
- **Widget smoke** (`widget_test.dart`, 1 test): 4 tab chính hiển thị đúng.

```bash
cd mobile
flutter test
```

## Current Status

- Firmware modules: motor control, UDP command parsing, odometry, calibration, node navigation, autonomous obstacle-avoidance.
- **3-tier rotation**:
  - Skip nếu heading error < 10°.
  - Fast rotate cho > 25°: PWM 200 cho góc < 120°, PWM 210 cho quay đầu (giảm từ 220 để tránh trượt quá 180°). Cross-target detection dừng ngay khi error đổi dấu.
  - Nudge cho góc nhỏ (10–25°) hoặc phần còn lại sau fast rotate. PWM 175 ban đầu, adaptive ±10/15 theo progress; burst 30–55 ms scale theo khoảng cách còn lại.
  - Rotation overshoot guard: dừng nudge nếu tổng xoay vượt initial error + 60°.
- **Heading snap + IMU re-anchor** sau mỗi rotation và tại mỗi waypoint — chống heading error accumulation qua nhiều segment.
- **Straight driving**: proportional heading correction (gain 175, deadband 1°) + encoder balance (gain 1.2) + slew rate (18 PWM/tick). Khi chạy thẳng, heading bị ghi đè bằng IMU thuần để triệt tiêu drift do encoder.
- **Collinear optimization**: heading error < 10° → skip rotate; node thẳng liên tiếp chạy không dừng.
- **Auto-calibration**: motor trim tự học 5%/segment, encoder scale theo dõi, persist NVS flash. Reset bằng `calibrate.mode = "motor_balance"` / `"encoder"`.
- **Autonomous explorer** (reactive, dùng lại nudge/fast-rotate của NodeNavigator): chạy thẳng → dừng < 30 cm → servo scan trái 150° + phải 30° → chọn hướng xa hơn (bằng nhau → ưu tiên trái) → 90° turn. Cả 2 bên < 30 nhưng ≥ 25 → 180° U-turn. Còn lại → STUCK. Servo trên GPIO19, LEDC channel 2, 50 Hz, 16-bit.
- **Flutter app**: 4 màn hình — Map (tap → A* → gửi waypoint), ESP32 Test (telemetry + ping/calibrate), Control (step buttons + slider tốc độ), Tự hành (Start/Stop + live telemetry). Simulator service thay thế UDP khi `USE_SIMULATOR=true`.

## Notes

- `.pio/`, `.dart_tool/`, `build/`, Gradle cache và local IDE state đã bị ignore.
- Giữ `map_data/environment.json` và `mobile/assets/environment.json` đồng bộ khi đổi bản đồ.
- Không commit local WiFi credentials, keystores hoặc environment files.
- Tăng `CONFIG_VERSION` trong `firmware/src/config.h` khi đổi chân phần cứng hoặc encoder calibration → tự động reset calibration NVS.