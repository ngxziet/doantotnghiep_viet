# Codebase Summary — Xe Tự Hành (Autonomous Vehicle)

## Tổng quan hệ thống

Hệ thống xe tự hành nhỏ gồm 2 thành phần chính:

| Thành phần | Công nghệ | Vai trò |
|---|---|---|
| **Firmware** | ESP32 + C++ (PlatformIO, Arduino) | Điều khiển động cơ, đọc cảm biến, phát WiFi AP |
| **Mobile App** | Flutter + Dart | Giao diện bản đồ, tính đường A*, gửi lệnh qua UDP |

---

## Cấu trúc thư mục

```
doantotnghiep_viet/
├── firmware/                              # ESP32 C++ (PlatformIO)
│   ├── platformio.ini                     # Board: esp32dev; 5 envs: esp32dev, l298n_test,
│   │                                       #   servo_test, encoder_pulse_test, encoder_calib_test
│   └── src/
│       ├── main.cpp                       # Entry point, vòng lặp chính 50 Hz (20 ms)
│       ├── config.h                       # Toàn bộ hằng số: pins, PWM, ngưỡng, IMU, tốc độ
│       ├── communication/
│       │   ├── udp_server.h               # Interface nhận/gửi UDP
│       │   └── udp_server.cpp             # Parse JSON lệnh (không heap), broadcast telemetry
│       ├── sensors/
│       │   ├── encoder.h/.cpp             # Đếm xung qua ngắt, debounce 6 ms, hỗ trợ LM393 1 kênh
│       │   ├── imu_mpu6050.h/.cpp         # Tích phân gyro Z → yaw (MPU-6050, I2C raw)
│       │   ├── ultrasonic_hcsr04.h/.cpp   # HC-SR04: median 5 (chậm) + readQuickCm (khi đang chạy)
│       │   └── sensor_pan_servo.h/.cpp    # SG90 quét trái/phải (LEDC ch 2, 50 Hz, 16-bit)
│       ├── controllers/
│       │   ├── motor_driver.h/.cpp        # LEDC PWM 2 bánh (ch 0/1, 5 kHz, 8-bit) + slew
│       │   ├── node_navigator.h/.cpp      # State machine xoay 3 tầng + đi thẳng giữa node
│       │   └── autonomous_explorer.h/.cpp # State machine tự hành né vật cản (HC-SR04 + servo)
│       ├── calibration/
│       │   └── calibration_manager.h/.cpp # Auto-tune motor trim (5%/segment) + encoder scale, NVS
│       └── localization/
│           ├── odometry.h                 # Pose (x, y, θ), ODOMETRY_ALPHA = 0.35
│           └── odometry.cpp               # Complementary filter encoder 65% + IMU 35%
│   # Các sketch test độc lập (PlatformIO env riêng):
│   #   l298n_test.cpp          — quay từng bánh theo chu kỳ
│   #   servo_test.cpp          — servo 0/90/180° + đọc HC-SR04
│   #   encoder_pulse_test.cpp  — đếm xung thô, so sánh kỳ vọng (hand-rotate)
│   #   encoder_calib_test.cpp  — motor tự quay / hand-rotate hiệu chuẩn PULSES_PER_REV
│
├── mobile/                                # Flutter App (Dart)
│   ├── pubspec.yaml                       # deps: udp ^5.0.3, path_provider ^2.1.0
│   ├── assets/
│   │   └── environment.json               # Bản đồ đóng gói vào app
│   ├── lib/
│   │   ├── main.dart                      # Entry point — chọn UdpService/SimulatorService
│   │   │                                   #   theo --dart-define=USE_SIMULATOR
│   │   ├── models/
│   │   │   ├── robot_state.dart           # Pose(x,y,θ), RobotState(pose, obstDist, status,
│   │   │   │                                #   imu_connected, heading_deg, node_index,
│   │   │   │                                #   route_size, calibrated)
│   │   │   └── map_data.dart              # Node, Edge, RoadMap + runtime disabledEdges
│   │   ├── planning/
│   │   │   ├── astar.dart                 # A* heuristic Euclidean + turn penalty
│   │   │   ├── road_map.dart              # Re-export từ models/map_data.dart
│   │   │   └── waypoint_builder.dart      # Path [nodeId] → List<Offset> tọa độ mét
│   │   ├── screens/
│   │   │   ├── main_shell.dart            # Bottom nav 4 màn hình (Map, Test, Control, Tự hành)
│   │   │   ├── map_screen.dart            # Map: tap node → A* → gửi waypoint,
│   │   │   │                                #   block/unblock edge runtime, reset pose
│   │   │   ├── connection_test_screen.dart # Telemetry UDP ESP32 + ping/calibrate (imu, motor, encoder)
│   │   │   ├── manual_control_screen.dart # Step buttons + slider tốc độ (manual + step)
│   │   │   ├── autonomous_screen.dart     # Tự hành: Start/Stop + telemetry trực tiếp
│   │   │   ├── map_painter.dart           # CustomPainter: graph + robot + trail + planned path
│   │   │   └── map_status_widgets.dart    # Status bar, obstacle warning, pose info, connection indicator
│   │   └── services/
│   │       ├── udp_service.dart           # ITransportService + UDP telemetry/command,
│   │       │                                #   auto-rebind khi mất kết nối (debounce 2 lần check)
│   │       └── simulator_service.dart     # SimulatorService: giả lập 0.5 m/s, 10 Hz, có triggerObstacle
│   └── test/
│       ├── astar_test.dart                # 7 tests: ngắn nhất, unreachable, disabled edge, equal-turn
│       ├── road_map_test.dart             # 8 tests: load JSON, getNeighbors, disableEdge, nearest node
│       ├── waypoint_builder_test.dart     # 3 tests: build waypoints từ path node IDs
│       ├── odometry_calc_test.dart        # 7 tests: công thức toán thuần, không cần hardware
│       ├── robot_state_test.dart          # 8 tests: parse JSON hợp lệ / lỗi → null, defaults, status fallback
│       └── widget_test.dart               # smoke test 4 tab
│
├── map_data/
│   └── environment.json                   # Bản đồ lưới waypoint 5×5 m, 105 node, 172 cạnh
│
└── docs/
    ├── codebase-summary.md                # File này
    ├── bao-cao-xe-tu-hanh.md/.html/.pdf/.docx  # Báo cáo đồ án
    └── ket-qua-thuc-nghiem-5.2.xlsx             # Số liệu thực nghiệm Chương 5.2
```

---

## Giao thức truyền thông UDP

ESP32 phát WiFi AP: **SSID `RobotCar`** | **pass `12345678`** | **IP `192.168.4.1`**

| Hướng | Port | Tần suất | Định dạng |
|---|---|---|---|
| ESP32 → App (telemetry) | 4210 | 10 Hz (broadcast `192.168.4.255`) | JSON |
| App → ESP32 (lệnh) | 4211 | Khi cần | JSON |

App gửi **3 lần** cùng `seq`; firmware dedup bằng `last_seq` (`UdpServer::_lastSeq`).

**Các loại lệnh (`type`):**
- `route` — waypoint list (tap trên map)
- `manual` — giữ motor trong `MANUAL_TIMEOUT_MS = 700 ms`
- `step` — discrete test step (`TEST_STEP_DURATION_MS = 400 ms`)
- `autonomous` — bật/tắt chế độ tự hành né vật cản (`command: start | stop`)
- `reset_pose` — reset odometry về (0, 0, 0)
- `calibrate` — `mode: imu | motor_balance | encoder`
- ping giả lập: route command với `waypoints=[]` → firmware chỉ cập nhật `seq`

**Telemetry JSON:**
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

**Status values:** `idle` | `moving` | `arrived` | `exploring` | `scanning` | `avoiding` | `stuck` | `obstacle_detected` | `emergency_stop` | `imu_missing` | `calibrating` | `calibrated` | `rotation_timeout`

---

## Luồng điều hướng

```
User tap node đích trên map
        ↓
getNearestNode(tapPosition) → nodeId
        ↓
AStar.findPath(map, startNode, goalNode) → ["A", "B", "C", "D"]
        ↓
WaypointBuilder.build(map, path) → [Offset(0,0), Offset(0.5,0), ...]
        ↓
ITransportService.sendWaypoints(waypoints, seq++)   (burst 3 lần trong UdpService)
        ↓ (có hardware)   UdpService     → UDP 4211 → ESP32 → motor
        ↓ (không hardware) SimulatorService → timer nội bộ 100 ms
        ↓
telemetryStream.listen → RobotState → setState() → repaint
```

---

## Chế độ tự hành né vật cản (`autonomous_explorer.cpp`)

Máy trạng thái phản ứng (reactive) chạy lồng trong vòng lặp 50 Hz, bật/tắt bằng Start/Stop trên màn hình Tự hành. Mượn `NodeNavigator::setStepTurn` cho pha né.

```
DRIVE       → servo giữa (90°), chạy thẳng PWM 160 + giữ hướng IMU
              mỗi 50 ms đo nhanh phía trước; < 30 cm (xác nhận 2 lần) → dừng → STOP_SETTLE
STOP_SETTLE → chờ 500 ms cho L298N inductive spike tan
SCAN LEFT   → servo 150°, chờ 350 ms (SERVO_SETTLE_MS), đọc kỹ (median 5)
SCAN RIGHT  → servo 30°,  chờ 350 ms, đọc kỹ → quyết định:
   • ≥1 bên ≥ 30 cm            → rẽ 90° về bên LỚN hơn (bằng nhau → ưu tiên TRÁI)
   • cả 2 bên < 30 nhưng ≥ 25  → quay đầu 180°
   • còn lại (< 25 cm)         → STUCK (dừng hẳn)
TURN        → giao góc cho Node Navigator xoay; xong → đồng bộ IMU → về DRIVE
```

**Ngưỡng (config.h):** `AUTO_STOP_DISTANCE_CM=30`, `AUTO_TURN_CLEARANCE_CM=25`, `AUTO_DRIVE_PWM=160`, `AUTO_DRIVE_SAMPLE_MS=50`, `AUTO_STOP_SETTLE_MS=500`. Servo `SERVO_PIN=19` (LEDC ch 2, 50 Hz, 16-bit, 0.5–2.5 ms pulse), góc 90/150/30°.

*(Lưu ý: né vật cản hiện là phản ứng cục bộ; cờ `USE_ULTRASONIC` cho emergency-stop trong chế độ bám lộ trình vẫn tắt mặc định trong `main.cpp`. Hướng phát triển: chặn cạnh bản đồ + A\* replan toàn cục.)*

---

## Odometry — Complementary Filter

```
θ_enc   = θ_prev + (dRight - dLeft) / wheelBase
θ_fused = θ_enc + 0.35 × (θ_imu - θ_enc)        ← ODOMETRY_ALPHA = 0.35
x      += d × cos(θ_mid)
y      += d × sin(θ_mid)                          ← Y-down coordinate system
```

- α = 0.35: encoder làm chủ heading (65%), IMU chỉ hiệu chỉnh (35%).
- Khi đi thẳng (`navigator.isDrivingStraight()`): heading bị ghi đè bằng IMU thuần trong `main.cpp` — triệt tiêu drift do encoder khi đang giữ hướng.
- Khi xoay tại chỗ: encoder bị `suppressTranslation`, IMU 100% cho heading.
- Khi IMU mất kết nối: encoder 100% fallback, `status = "imu_missing"`.
- Vị trí (x, y): 100% từ encoder, IMU không tham gia.
- Cập nhật mỗi 20 ms trong vòng lặp chính (50 Hz).

## Thuật toán xoay — 3 tầng

### Tầng 1: Skip (< 10°)
Heading error < 10° → bỏ qua xoay, đi thẳng luôn, heading correction tự chỉnh.

### Tầng 2: Fast Rotate (> 25°)
Xoay liên tục ở PWM cố định theo thời gian ước lượng (`FAST_ROTATE_SPEED_DPS = 150°/s × FAST_ROTATE_UNDERSHOOT = 0.85`):
- Góc nhỏ (< 120°, trái/phải): PWM 200 (`FAST_ROTATE_PWM`).
- Góc lớn (> 120°, quay đầu): PWM 210 (`FAST_ROTATE_REVERSE_PWM`, giảm từ 220 tránh trượt quá 180°).
- Cross-target detection: dừng ngay khi error đổi dấu (fix bug 180° angle wrapping).
- Sau khi dừng → chuyển sang nudge để tinh chỉnh phần còn lại.

### Tầng 3: Nudge (tinh chỉnh)
Step-stop-measure cho góc nhỏ (10°–25°) hoặc phần còn lại sau fast rotate:

```
loop:
  Cấp PWM (175, adaptive) trong 30–55 ms → dừng motor
  Chờ NUDGE_SETTLE_MS = 220 ms (IMU ổn định, xe đứng yên)
  Đo heading thực tế từ IMU
  if |error| ≤ 4°                  → xong, chuyển sang Drive hoặc Arrived
  if tiến bộ < 1° (MIN_PROGRESS)   → tăng PWM +10 (thắng ma sát tĩnh)
  if tiến bộ > 3° (OVER_PROGRESS)  → giảm PWM -15 (tránh mất kiểm soát)
  else                              → giữ nguyên PWM, nhích tiếp
```

- Mỗi nudge cycle ~ 250 ms, cua 90° mất ~ 4–8 cycle.
- PWM thích ứng: tự tăng khi lực không đủ, tự giảm khi xoay quá nhanh.
- Robot luôn đứng yên khi đo → data IMU sạch, không bị ảnh hưởng rung.
- Timeout 60 s (`ROTATE_TIMEOUT_MS`) → báo `rotation_timeout`.
- Overshoot guard: nếu tổng xoay > initial error + 60° → dừng (chống xoay vòng).

---

## Bản đồ demo

Lưới waypoint 5×5 m, **105 node** (id `Nxxxx` theo tọa độ cm, vd `N0000`=(0,0), `N0500`=(0.5,0)), **172 cạnh bidirectional** nối các hành lang liền kề (chỉ một tập con của lưới đầy đủ 11×11 để tránh quá nhiều đường đi). Bước lưới 0.5 m (khớp `NODE_DISTANCE_M` trong firmware). Tọa độ 0.0–5.0 m theo cả hai trục. Runtime có thể bật/tắt cạnh (block/unblock) để mô phỏng vật cản.

---

## Chạy không có hardware (Simulator Mode)

`main.dart` đọc biến môi trường `USE_SIMULATOR` để chọn transport:

```bash
cd mobile
flutter pub get
flutter test                                    # chạy unit tests
flutter run                                     # chạy với UDP thật (cần ESP32)
flutter run --dart-define=USE_SIMULATOR=true    # chạy với simulator
```

Simulator giả lập 0.5 m/s, broadcast telemetry ở 10 Hz; có `triggerObstacle(distCm)` để test obstacle warning UI.

---

## Các thư viện sử dụng

### Firmware (C++)
| Lib | Version | Dùng cho |
|---|---|---|
| Wire (built-in) | — | I2C raw read/write cho MPU-6050 |
| WiFi (built-in) | — | ESP32 SoftAP + UDP socket |
| Preferences (built-in) | — | Lưu calibration data vào NVS flash |
| electroniccats/MPU6050 | ^1.0.0 | (Khai báo trong `platformio.ini`; hiện không dùng — đang dùng I2C raw) |
| me-no-dev/AsyncTCP | ^1.1.1 | (Khai báo; dự phòng) |

### Flutter (Dart)
| Package | Version | Dùng cho |
|---|---|---|
| udp | ^5.0.3 | UDP socket gửi/nhận |
| path_provider | ^2.1.0 | Lấy đường dẫn file để ghi CSV log |

---

## Trạng thái hiện tại

| Phase | Mô tả | Trạng thái |
|---|---|---|
| 00 | Mua linh kiện, lắp ráp phần cứng | Hoàn thành |
| 01 | Firmware: Encoder, IMU, Motor, Node Navigator | Hoàn thành |
| 02 | Firmware: Odometry, UDP, Calibration, Flutter models | Hoàn thành |
| 03 | Flutter UI: MapScreen, A*, tap → navigate | Hoàn thành |
| 04 | Obstacle avoidance: HC-SR04 + servo quét, chế độ tự hành | Hoàn thành (né phản ứng cục bộ; replan toàn cục là hướng phát triển) |
| 05 | Finalization: tune params, đo sai số, demo, viết báo cáo | Đang thực hiện — tuning encoder + rotation; báo cáo `docs/bao-cao-xe-tu-hanh.{md,html,pdf,docx}` |