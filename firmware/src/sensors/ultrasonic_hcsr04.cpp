#include "ultrasonic_hcsr04.h"

UltrasonicHcsr04::UltrasonicHcsr04(int trigPin, int echoPin)
    : _trigPin(trigPin), _echoPin(echoPin)
{}

void UltrasonicHcsr04::begin() {
    pinMode(_trigPin, OUTPUT);
    pinMode(_echoPin, INPUT);
    digitalWrite(_trigPin, LOW);
}

float UltrasonicHcsr04::_singleReadCm() {
    // Đảm bảo trigger ở mức thấp trước khi phát xung
    digitalWrite(_trigPin, LOW);
    delayMicroseconds(2);

    // Xung trigger 10µs
    digitalWrite(_trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(_trigPin, LOW);

    // Timeout 30ms → tầm xa tối đa ~510cm; giới hạn tại MAX_RANGE_CM
    long duration = pulseIn(_echoPin, HIGH, 30000UL);

    if (duration == 0) return MAX_RANGE_CM; // hết thời gian → ngoài tầm

    // Tốc độ âm thanh: ~0.0343 cm/µs; chia 2 vì sóng đi và về
    float dist = duration * 0.0343f / 2.0f;
    return (dist > MAX_RANGE_CM) ? MAX_RANGE_CM : dist;
}

float UltrasonicHcsr04::_median5(float arr[5]) {
    // Insertion sort đơn giản cho 5 phần tử
    for (int i = 1; i < 5; i++) {
        float key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
    return arr[2]; // vị trí trung vị
}

float UltrasonicHcsr04::readDistanceCm() {
    float samples[5];
    for (int i = 0; i < 5; i++) {
        samples[i] = _singleReadCm();
        delay(10); // nghỉ ngắn giữa các lần đo để tránh nhiễu dội
    }
    _lastDistCm = _median5(samples);
    return _lastDistCm;
}

bool UltrasonicHcsr04::isObstacleNear() {
    return _lastDistCm < WARN_THRESHOLD_CM;
}

bool UltrasonicHcsr04::isEmergency() {
    return _lastDistCm < EMERGENCY_STOP_CM;
}
