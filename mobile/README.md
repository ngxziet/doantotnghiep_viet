# Mobile app — Xe tự hành

Ứng dụng Flutter điều khiển xe tự hành qua UDP. App hiển thị bản đồ,
tính đường A*, gửi waypoint cho ESP32 và nhận telemetry để cập nhật vị trí xe.

## Yêu cầu

- Flutter SDK 3.x
- Dart SDK tương thích với Flutter
- Android Studio hoặc thiết bị Android thật
- Nếu chạy với xe thật: ESP32 đã nạp firmware

## Cài đặt và chạy app

```bash
cd mobile
flutter pub get
flutter test
flutter run                                    # chạy với ESP32 thật (mặc định)
flutter run --dart-define=USE_SIMULATOR=true   # chạy giả lập (không cần ESP32)
```

`USE_SIMULATOR` đọc qua `String.fromEnvironment('USE_SIMULATOR')` trong `lib/main.dart`.
Không cần sửa code — chỉ cần truyền `--dart-define` khi `flutter run`.

## Chạy với ESP32

1. Kết nối điện thoại hoặc máy chạy app vào WiFi của ESP32:
   - SSID: `RobotCar`
   - Password: `12345678`
   - ESP32 IP: `192.168.4.1`
2. Chạy app bằng `flutter run` (KHÔNG truyền `USE_SIMULATOR=true`).

UDP ports:

| Hướng | Port | Nội dung |
| --- | --- | --- |
| ESP32 → App | `4210` | Telemetry JSON (broadcast `192.168.4.255`) |
| App → ESP32 | `4211` | Waypoint, manual, step, autonomous, reset pose, calibrate |

## Encoder LM393 đang dùng

Xe đang dùng module encoder LM393, không phải encoder quadrature A/B. Mỗi bánh
chỉ dùng 1 chân tín hiệu `D0`/`OUT`; chân `A0` bỏ qua.

| LM393 | ESP32 |
| --- | --- |
| VCC | 3V3 hoặc 5V tuỳ module |
| GND | GND chung |
| D0/OUT encoder trái | GPIO34 |
| D0/OUT encoder phải | GPIO32 |

Firmware hiện tại đã đúng kiểu này:

```cpp
Encoder encLeft(34);
Encoder encRight(32);
```

Vì LM393 chỉ có 1 kênh xung nên nó không tự biết chiều quay. Firmware lấy
chiều từ lệnh motor bằng `setDirection(...)`, nên vẫn dùng được cho odometry
trong đề tài này.

Lưu ý:

- Tất cả module phải nối chung GND với ESP32.
- Nếu tín hiệu `D0/OUT` của module là 5V, nên hạ áp về 3.3V trước khi vào ESP32.
- GPIO34 là chân input-only, dùng tốt cho encoder trái.

## Màn hình trong app

Bottom navigation 4 màn hình:

- **Map**: chọn điểm đích và xem đường đi; có thể block/unblock cạnh runtime.
- **ESP32 Test**: xem telemetry trực tiếp, gửi ping, hiệu chuẩn IMU / motor / encoder.
- **Control**: điều khiển xe bằng step buttons + slider tốc độ.
- **Tự hành**: bật/tắt chế độ tự hành né vật cản, hiển thị telemetry thời gian thực.

## Lệnh hay dùng

```bash
flutter pub get
flutter test
flutter run                                  # chạy với ESP32 thật
flutter run --dart-define=USE_SIMULATOR=true # chạy giả lập
```