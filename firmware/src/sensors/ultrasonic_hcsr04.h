#pragma once

#include <Arduino.h>

// Ngưỡng khoảng cách (cm)
static constexpr float EMERGENCY_STOP_CM  = 10.0f;
static constexpr float WARN_THRESHOLD_CM  = 20.0f;
static constexpr float MAX_RANGE_CM       = 200.0f;

class UltrasonicHcsr04 {
public:
    UltrasonicHcsr04(int trigPin, int echoPin);

    void begin();

    // Trả về trung vị của 5 lần đo (cm). Giới hạn tại MAX_RANGE_CM.
    // Chậm (~50ms+ do delay giữa các mẫu) — chỉ dùng khi xe đứng yên.
    float readDistanceCm();

    // Một lần đo nhanh (không lấy trung vị, không delay) — dùng khi đang chạy
    // để vòng lặp không bị nghẽn. Cập nhật _lastDistCm như readDistanceCm.
    float readQuickCm();

    // true nếu khoảng cách đo gần nhất < ngưỡng cảnh báo (20cm)
    bool isObstacleNear();

    // true nếu khoảng cách đo gần nhất < ngưỡng dừng khẩn cấp (10cm)
    bool isEmergency();

private:
    int _trigPin;
    int _echoPin;
    float _lastDistCm = MAX_RANGE_CM;

    // Phát 1 xung trigger và trả về khoảng cách thô (cm)
    float _singleReadCm();

    // Sắp xếp mảng 5 phần tử; trả về phần tử ở vị trí giữa (trung vị)
    static float _median5(float arr[5]);
};
