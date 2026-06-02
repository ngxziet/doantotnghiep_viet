# Mobile app - Xe tu hanh

Ung dung Flutter de dieu khien xe tu hanh qua UDP. App hien thi ban do,
tinh duong A*, gui waypoint cho ESP32 va nhan telemetry de cap nhat vi tri xe.

## Yeu cau

- Flutter SDK 3.x
- Dart SDK tuong thich voi Flutter
- Android Studio hoac thiet bi Android that
- Neu chay voi xe that: ESP32 da nap firmware

## Cai dat va chay app

```bash
cd mobile
flutter pub get
flutter test
flutter run
```

Mac dinh app dang dung ESP32 that:

```dart
const useSimulator = false;
```

Neu muon chay demo khong can xe, mo `lib/main.dart` va doi thanh:

```dart
const useSimulator = true;
```

## Chay voi ESP32

1. Ket noi dien thoai hoac may chay app vao WiFi cua ESP32:
   - SSID: `RobotCar`
   - Password: `12345678`
   - ESP32 IP: `192.168.4.1`
2. Dam bao `useSimulator = false` trong `lib/main.dart`.
3. Chay app bang `flutter run`.

UDP ports:

| Huong | Port | Noi dung |
| --- | --- | --- |
| ESP32 -> App | `4210` | Telemetry JSON |
| App -> ESP32 | `4211` | Waypoint, manual control, reset pose |

## Encoder LM393 dang dung

Xe dang dung module encoder LM393, khong phai encoder quadrature A/B. Moi banh
chi dung 1 chan tin hieu `D0`/`OUT`; chan `A0` bo qua.

| LM393 | ESP32 |
| --- | --- |
| VCC | 3V3 hoac 5V tuy module |
| GND | GND chung |
| D0/OUT encoder trai | GPIO34 |
| D0/OUT encoder phai | GPIO32 |

Firmware hien tai da dung dung kieu nay:

```cpp
Encoder encLeft(34);
Encoder encRight(32);
```

Vi LM393 chi co 1 kenh xung nen no khong tu biet chieu quay. Firmware lay
chieu tu lenh motor bang `setDirection(...)`, nen van dung duoc cho odometry
trong de tai nay.

Luu y:

- Tat ca module phai noi chung GND voi ESP32.
- Neu tin hieu `D0/OUT` cua module la 5V, nen ha ap ve 3.3V truoc khi vao ESP32.
- GPIO34 la chan input-only, dung tot cho encoder trai.

## Man hinh trong app

- Map: chon diem dich va xem duong di.
- Connection test: kiem tra ket noi ESP32.
- Manual control: dieu khien xe bang tay.
- Telemetry/calibration: xem pose, trang thai va reset pose.

## Lenh hay dung

```bash
flutter pub get
flutter test
flutter run
```
