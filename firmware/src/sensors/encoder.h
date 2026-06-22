#pragma once

#include <Arduino.h>
#include "../config.h"

// Hằng số phần cứng có thể điều chỉnh
static constexpr int   ENCODER_PULSES_PER_REV  = RobotConfig::ENCODER_PULSES_PER_REV;
static constexpr float ENCODER_WHEEL_DIAM_CM   = RobotConfig::WHEEL_DIAM_CM;

class Encoder {
public:
    explicit Encoder(int pinA, int pinB = -1);

    // Gắn interrupt trên pinA cạnh lên (RISING)
    void begin();

    // Tổng số xung tích lũy (đọc an toàn từ interrupt)
    long getPulseCount();

    // Vận tốc cm/s, tính lại mỗi 50ms
    float getVelocityCmPerSec();

    // Reset số xung và vận tốc về 0
    void reset();
    void resetPulses() { reset(); }

    float getDistanceCm();

    // Encoder 1 kênh (LM393 D0): hướng quay lấy từ lệnh motor.
    void setDirection(int direction);

    // Gọi từ ISR — không gọi trực tiếp
    void IRAM_ATTR onRisingA();

private:
    int _pinA;
    int _pinB;

    volatile long _pulseCount;
    volatile int  _direction;
    volatile unsigned long _lastPulseMicros;

    // Thời gian tối thiểu giữa 2 xung hợp lệ — CHỈ lọc glitch nhiễu cỡ µs (minGap≈2µs do
    // comparator dao động + EMI motor). Doubling LM393 phụ thuộc tốc độ (chậm→2x, nhanh→1x);
    // khi xe chạy nhanh tín hiệu sạch ~20 xung/vòng nên dùng ENCODER_PULSES_PER_REV=20 (config.h).
    // Với 20 xung/vòng, tốc độ chạy ~20-40 cm/s → xung thật cách ~26-51ms ≫ 6ms, không bị lọc nhầm.
    // 6000µs cho tối đa ~166 xung/s (~170 cm/s @1.02cm/xung) — dư biên.
    static constexpr unsigned long MIN_PULSE_INTERVAL_US = 6000;

    // Theo dõi vận tốc
    long  _lastPulseSnapshot;
    unsigned long _lastVelocityMs;
    float _velocityCmPerSec;

    // Hỗ trợ tối đa 4 encoder qua bảng dispatch tĩnh
    static Encoder* _instances[4];
    static int      _instanceCount;

    // ISR tĩnh — chuyển tiếp đến onRisingA() của từng instance
    static void IRAM_ATTR _isr0();
    static void IRAM_ATTR _isr1();
    static void IRAM_ATTR _isr2();
    static void IRAM_ATTR _isr3();
};
