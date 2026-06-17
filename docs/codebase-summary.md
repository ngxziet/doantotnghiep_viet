# Codebase Summary — Xe Tự Hành (Autonomous Vehicle)

## Tổng quan hệ thống

Hệ thống xe tự hành nhỏ gồm 2 thành phần chính:

| Thành phần | Công nghệ | Vai trò |
|---|---|---|
| **Firmware** | ESP32 + C++ (PlatformIO) | Điều khiển động cơ, đọc cảm biến, phát WiFi AP |
| **Mobile App** | Flutter + Dart | Giao diện bản đồ, tính đường A*, gửi lệnh qua UDP |

---

## Cấu trúc thư mục

```
doantotnghiep/
├── firmware/                              # ESP32 C++ (PlatformIO)
│   ├── platformio.ini                     # Board: esp32dev, libs: MPU6050, AsyncTCP
│   └── src/
│       ├── main.cpp                       # Entry point, vòng lặp chính 50Hz (20ms)
│       ├── communication/
│       │   ├── udp_server.h               # Interface nhận/gửi UDP
│       │   └── udp_server.cpp             # Parse JSON lệnh, broadcast telemetry JSON
│       ├── sensors/
│       │   ├── encoder.h/.cpp             # Đọc xung encoder, tính vận tốc bánh xe
│       │   ├── imu_mpu6050.h/.cpp         # Tích phân gyro Z → yaw (MPU-6050)
│       │   ├── ultrasonic_hcsr04.h/.cpp   # HC-SR04, đọc kỹ (median 5) + đọc nhanh
│       │   └── sensor_pan_servo.h/.cpp    # Servo SG90 quét cảm biến (LEDC ch2, 50Hz)
│       ├── controllers/
│       │   ├── motor_driver.h/.cpp        # LEDC PWM 2 bánh (ch 0/1, 5kHz, 8-bit)
│       │   ├── node_navigator.h/.cpp      # State machine xoay + đi thẳng giữa các node
│       │   └── autonomous_explorer.h/.cpp # State machine tự hành né vật cản (HC-SR04+servo)
│       ├── calibration/
│       │   └── calibration_manager.h/.cpp # Auto-tune motor trim, lưu NVS
│       └── localization/
│           ├── odometry.h                 # Interface pose (x, y, θ), ALPHA = 0.35
│           └── odometry.cpp               # Complementary Filter encoder 65% + IMU 35%
│
├── mobile/                                # Flutter App (Dart)
│   ├── pubspec.yaml                       # deps: udp ^2.0.0, path_provider ^2.1.0
│   ├── assets/
│   │   └── environment.json               # Bản đồ đóng gói vào app
│   ├── lib/
│   │   ├── main.dart                      # Entry point (StatefulWidget) — inject + dispose transport
│   │   ├── models/
│   │   │   ├── robot_state.dart           # Pose(x,y,θ), RobotState(pose, obstDist, status)
│   │   │   └── map_data.dart              # Node, Edge, RoadMap + runtime disabledEdges
│   │   ├── planning/
│   │   │   ├── astar.dart                 # A* heuristic Euclidean, trả List<String>?
│   │   │   ├── road_map.dart              # Load JSON asset → RoadMap object
│   │   │   └── waypoint_builder.dart      # Path [nodeId] → List<Offset> tọa độ mét
│   │   ├── screens/
│   │   │   ├── main_shell.dart            # Bottom menu 4 màn hình
│   │   │   ├── map_screen.dart            # Map: tap node → A* → navigate
│   │   │   ├── connection_test_screen.dart # Test telemetry/UDP ESP32
│   │   │   ├── manual_control_screen.dart # Điều khiển tay
│   │   │   ├── autonomous_screen.dart     # Tự hành: nút Start/Stop + telemetry trực tiếp
│   │   │   ├── map_painter.dart           # CustomPainter: vẽ graph + robot + trail
│   │   │   └── map_status_widgets.dart    # Status bar, obstacle warning, pose info
│   │   └── services/
│   │       ├── udp_service.dart           # ITransportService + UDP telemetry/command
│   │       └── simulator_service.dart     # SimulatorService: giả lập 0.5m/s, 10Hz
│   └── test/
│       ├── astar_test.dart                # 6 tests: đường ngắn nhất, unreachable, disabled edge
│       ├── road_map_test.dart             # 8 tests: load JSON, getNeighbors, disableEdge
│       ├── waypoint_builder_test.dart     # 3 tests: build waypoints từ path node IDs
│       ├── odometry_calc_test.dart        # 7 tests: công thức toán thuần, không cần hardware
│       └── robot_state_test.dart          # 5 tests: parse JSON hợp lệ / lỗi → null
│
├── map_data/
│   └── environment.json                   # Bản đồ gốc: 8 node A-H, hình T + nhánh
│
├── plans/
│   ├── setup-guide.html                   # Tài liệu HTML đầy đủ (mở trực tiếp browser)
│   ├── xe-tu-hanh-project-detail.md       # Kiến trúc tổng thể, quyết định thiết kế
│   ├── hardware-setup-wiring-checklist.md # Linh kiện, sơ đồ nối dây, cảnh báo GPIO
│   └── 260424-1053-xe-tu-hanh-master-implementation/
│       ├── plan.md                        # Tổng quan 6 phase + git commit strategy
│       ├── phase-00-hardware.md           # Mua linh kiện, lắp ráp, test UDP PING
│       ├── phase-01-firmware-base.md      # Encoder, IMU, PID, Motor driver
│       ├── phase-02-odometry-udp.md       # Odometry, complementary filter, UDP service
│       ├── phase-03-flutter-ui.md         # MapScreen, MapPainter, tap→navigate
│       ├── phase-04-obstacle-avoidance.md # HC-SR04, emergency stop, replan A*
│       └── phase-05-finalization.md       # CSV log, đo sai số thực nghiệm, video demo
│
└── docs/
    └── codebase-summary.md                # File này
```

---

## Giao thức truyền thông UDP

ESP32 phát WiFi AP: **SSID `RobotCar`** | **pass `12345678`** | **IP `192.168.4.1`**

| Hướng | Port | Tần suất | Ví dụ JSON |
|---|---|---|---|
| ESP32 → App (telemetry) | 4210 | 10 Hz | `{"pose":{"x":0.35,"y":0.12,"theta":1.57},"obstacle_dist_cm":45.2,"status":"moving"}` |
| App → ESP32 (lệnh) | 4211 | Khi cần | `{"seq":1,"waypoints":[{"x":0.5,"y":0.0},{"x":1.0,"y":0.0}]}` |

Lệnh waypoints gửi **3 lần** cùng `seq`, ESP32 dedup bằng `last_seq`.

**Lệnh tự hành:** `{"seq":N,"type":"autonomous","command":"start"|"stop"}` — bật/tắt chế độ tự hành né vật cản. Các loại lệnh khác: `manual`, `step`, `reset_pose`, `calibrate`, `ping`.

**Status values:** `idle` | `moving` | `arrived` | `exploring` | `scanning` | `avoiding` | `stuck` | `obstacle_detected` | `emergency_stop` | `imu_missing` | `calibrating`

---

## Luồng điều hướng

```
User tap node đích trên map
        ↓
findNearestNode(tapPosition) → nodeId
        ↓
AStar.findPath(map, startNode, goalNode) → ["A", "B", "C", "D"]
        ↓
WaypointBuilder.build(map, path) → [Offset(0,0), Offset(0.5,0), ...]
        ↓
ITransportService.sendWaypoints(waypoints, seq++)
        ↓ (có hardware)   UdpService     → UDP 4211 → ESP32 → motor
        ↓ (không hardware) SimulatorService → timer nội bộ 100ms
        ↓
telemetryStream.listen → RobotState → setState() → repaint
```

---

## Chế độ tự hành né vật cản (autonomous_explorer.cpp)

Máy trạng thái phản ứng (reactive) chạy lồng trong vòng lặp 50Hz, bật/tắt bằng nút Start/Stop trên màn hình Tự hành. Mượn lại cơ chế xoay của Node Navigator (`setStepTurn`) cho pha né.

```
DRIVE      → servo giữa(90°), chạy thẳng PWM160 + giữ hướng IMU
             mỗi 50ms đọc nhanh phía trước; <30cm (xác nhận 2 lần) → dừng → SCAN
SCAN LEFT  → servo 150°, chờ 350ms, đọc kỹ (median 5) → khoảng cách trái
SCAN RIGHT → servo 30°,  chờ 350ms, đọc kỹ → khoảng cách phải → quyết định:
   • ≥1 bên ≥30cm        → rẽ 90° về bên LỚN hơn (bằng nhau → ưu tiên TRÁI)
   • cả 2 bên <30 nhưng ≥25cm → quay đầu 180°
   • còn lại (<25cm)     → STUCK (dừng hẳn)
TURN       → giao góc cho Node Navigator xoay; xong → đồng bộ IMU → về DRIVE
```

Ngưỡng (config.h): `AUTO_STOP_DISTANCE_CM=30`, `AUTO_TURN_CLEARANCE_CM=25`, `AUTO_DRIVE_PWM=160`, servo `SERVO_PIN=19` (LEDC ch2, 50Hz, 16-bit), góc 90/150/30°, chờ ổn định 350ms.

*(Lưu ý: né vật cản hiện là phản ứng cục bộ; cờ `USE_ULTRASONIC` cho emergency-stop chế độ bám lộ trình vẫn tắt mặc định. Hướng phát triển: chặn cạnh bản đồ + A\* replan toàn cục.)*

---

## Odometry — Complementary Filter

```
θ_enc   = θ_prev + (dRight - dLeft) / wheelBase
θ_fused = θ_enc + 0.35 × (θ_imu - θ_enc)
x      += d × cos(θ_mid)
y      -= d × sin(θ_mid)          ← Y-down coordinate system
```

- α = 0.35: encoder làm chủ heading (65%), IMU chỉ hiệu chỉnh (35%)
- Khi xoay tại chỗ: IMU 100% (encoder không đáng tin khi 2 bánh ngược chiều)
- Khi IMU mất kết nối: encoder 100% fallback
- Vị trí (x, y): 100% từ encoder, IMU không tham gia
- Cập nhật mỗi 20ms trong vòng lặp chính (50Hz)

## Thuật toán xoay — 3 tầng

### Tầng 1: Skip (<10°)
Heading error dưới 10° → bỏ qua xoay, đi thẳng luôn, heading correction tự chỉnh.

### Tầng 2: Fast Rotate (>25°)
Xoay liên tục ở PWM cố định theo thời gian ước lượng (150°/s × 75% undershoot):
- Góc nhỏ (<120°, trái/phải): PWM 200
- Góc lớn (>120°, quay đầu): PWM 190 — giảm tốc tránh trượt quá
- Cross-target detection: dừng ngay khi error đổi dấu (fix bug 180° angle wrapping)
- Sau khi dừng → chuyển sang nudge để tinh chỉnh phần còn lại

### Tầng 3: Nudge (tinh chỉnh)
Step-stop-measure cho góc nhỏ (10°-25°) hoặc phần còn lại sau fast rotate:

```
loop:
  Cấp PWM 175 trong 35ms → dừng motor
  Chờ 150ms (IMU ổn định, xe đứng yên)
  Đo heading thực tế từ IMU
  if |error| ≤ 4° → xong, chuyển sang Drive hoặc Arrived
  if tiến bộ < 1° → tăng PWM +10 (thắng ma sát tĩnh)
  if tiến bộ > 3° → giảm PWM -15 (tránh mất kiểm soát)
  else            → giữ nguyên PWM, nhích tiếp
```

- Mỗi nudge cycle ~185ms, cua 90° mất ~4-8 cycle
- PWM thích ứng: tự tăng khi lực không đủ, tự giảm khi xoay quá nhanh
- Robot luôn đứng yên khi đo → data IMU sạch, không bị ảnh hưởng rung
- Timeout 12 giây nếu xoay quá lâu → báo fault dừng

---

## Bản đồ demo

```
H(0,0.5) ─────────── F(1,0.5) ── G(1,1)
│                         │
A(0,0) ── B(0.5,0) ── C(1,0) ── D(1.5,0)
               │
           E(0.5,0.5)
```

8 node (A-H), tất cả cạnh bidirectional, khoảng cách 0.5m (H→F = 1.118m đường chéo).
3 junction: B, C, F | 4 dead-end: A, D, E, G | Tọa độ: 0.0 – 1.5m

---

## Chạy không có hardware (Simulator Mode)

`main.dart` đọc biến môi trường `USE_SIMULATOR` để chọn transport:

```bash
cd mobile
flutter pub get
flutter test                                    # chạy unit tests
flutter run                                     # chạy với UDP thật (cần ESP32)
flutter run --dart-define=USE_SIMULATOR=true     # chạy với simulator
```

---

## Các thư viện sử dụng

### Firmware (C++)
| Lib | Version | Dùng cho |
|---|---|---|
| Wire (built-in) | — | I2C raw read/write cho MPU-6050 (không dùng lib bên ngoài) |
| WiFi (built-in) | — | ESP32 SoftAP + UDP socket |
| Preferences (built-in) | — | Lưu calibration data vào NVS flash |

### Flutter (Dart)
| Package | Version | Dùng cho |
|---|---|---|
| udp | ^2.0.0 | UDP socket gửi/nhận |
| path_provider | ^2.1.0 | Lấy đường dẫn file để ghi CSV log |

---

## Trạng thái hiện tại

| Phase | Mô tả | Trạng thái |
|---|---|---|
| 00 | Mua linh kiện, lắp ráp phần cứng | Hoàn thành |
| 01 | Firmware: Encoder, IMU, Motor, Node Navigator | Hoàn thành |
| 02 | Firmware: Odometry, UDP, Calibration, Flutter models | Hoàn thành |
| 03 | Flutter UI: MapScreen, A*, tap→navigate | Hoàn thành |
| 04 | Obstacle avoidance: HC-SR04 + servo quét, chế độ tự hành | Hoàn thành (né phản ứng cục bộ; replan toàn cục là hướng phát triển) |
| 05 | Finalization: tune params, đo sai số, demo | Đang thực hiện — tuning encoder + rotation |
