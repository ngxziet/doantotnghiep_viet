#include "encoder.h"

// Bảng dispatch tĩnh cho các instance encoder
Encoder* Encoder::_instances[4] = {nullptr, nullptr, nullptr, nullptr};
int      Encoder::_instanceCount = 0;

Encoder::Encoder(int pinA, int pinB)
    : _pinA(pinA), _pinB(pinB),
      _pulseCount(0),
      _direction(1),
      _lastPulseMicros(0),
      _lastPulseSnapshot(0),
      _lastVelocityMs(0),
      _velocityCmPerSec(0.0f)
{}

void Encoder::begin() {
    pinMode(_pinA, INPUT_PULLUP);
    if (_pinB >= 0) {
        pinMode(_pinB, INPUT_PULLUP);
    }

    // Đăng ký instance và gắn ISR tĩnh tương ứng
    if (_instanceCount >= 4) return;
    int idx = _instanceCount++;
    _instances[idx] = this;

    void (*isrFn)() = nullptr;
    switch (idx) {
        case 0: isrFn = _isr0; break;
        case 1: isrFn = _isr1; break;
        case 2: isrFn = _isr2; break;
        case 3: isrFn = _isr3; break;
        default: return; // Không hỗ trợ quá 4 encoder
    }
    attachInterrupt(digitalPinToInterrupt(_pinA), isrFn, RISING);
}

long Encoder::getPulseCount() {
    // Đọc atomic: tắt interrupt tạm để tránh race condition khi đọc giá trị
    noInterrupts();
    long count = _pulseCount;
    interrupts();
    return count;
}

float Encoder::getVelocityCmPerSec() {
    unsigned long now = millis();
    unsigned long elapsed = now - _lastVelocityMs;

    // Tính lại mỗi 50ms
    if (elapsed >= 50) {
        long currentCount = getPulseCount();
        long deltaPulses   = currentCount - _lastPulseSnapshot;
        float dt_sec       = elapsed / 1000.0f;

        // vận tốc = (xung / xung_mỗi_vòng) × chu_vi / dt
        _velocityCmPerSec = (deltaPulses / (float)ENCODER_PULSES_PER_REV)
                            * (PI * ENCODER_WHEEL_DIAM_CM)
                            / dt_sec;

        _lastPulseSnapshot = currentCount;
        _lastVelocityMs    = now;
    }
    return _velocityCmPerSec;
}

float Encoder::getDistanceCm() {
    return getPulseCount() * RobotConfig::CM_PER_PULSE;
}

void Encoder::reset() {
    noInterrupts();
    _pulseCount = 0;
    interrupts();
    _lastPulseSnapshot = 0;
    _lastVelocityMs    = millis();
    _velocityCmPerSec  = 0.0f;
}

void Encoder::setDirection(int direction) {
    if (direction > 0) {
        _direction = 1;
    } else if (direction < 0) {
        _direction = -1;
    } else {
        _direction = 0;
    }
}

void IRAM_ATTR Encoder::onRisingA() {
    unsigned long now = micros();
    if (now - _lastPulseMicros < MIN_PULSE_INTERVAL_US) return;
    _lastPulseMicros = now;
    _pulseCount += _direction;
}

// ISR tĩnh — mỗi hàm chuyển tiếp đến instance tương ứng
void IRAM_ATTR Encoder::_isr0() { if (_instances[0]) _instances[0]->onRisingA(); }
void IRAM_ATTR Encoder::_isr1() { if (_instances[1]) _instances[1]->onRisingA(); }
void IRAM_ATTR Encoder::_isr2() { if (_instances[2]) _instances[2]->onRisingA(); }
void IRAM_ATTR Encoder::_isr3() { if (_instances[3]) _instances[3]->onRisingA(); }
