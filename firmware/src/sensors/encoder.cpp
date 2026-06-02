#include "encoder.h"

// Static instance dispatch table
Encoder* Encoder::_instances[4] = {nullptr, nullptr, nullptr, nullptr};
int      Encoder::_instanceCount = 0;

Encoder::Encoder(int pinA, int pinB)
    : _pinA(pinA), _pinB(pinB),
      _pulseCount(0),
      _direction(1),
      _lastPulseSnapshot(0),
      _lastVelocityMs(0),
      _velocityCmPerSec(0.0f)
{}

void Encoder::begin() {
    pinMode(_pinA, INPUT_PULLUP);
    if (_pinB >= 0) {
        pinMode(_pinB, INPUT_PULLUP);
    }

    // Register this instance and attach matching static ISR
    if (_instanceCount >= 4) return;
    int idx = _instanceCount++;
    _instances[idx] = this;

    void (*isrFn)() = nullptr;
    switch (idx) {
        case 0: isrFn = _isr0; break;
        case 1: isrFn = _isr1; break;
        case 2: isrFn = _isr2; break;
        case 3: isrFn = _isr3; break;
        default: return; // More than 4 encoders not supported
    }
    attachInterrupt(digitalPinToInterrupt(_pinA), isrFn, RISING);
}

long Encoder::getPulseCount() {
    // Atomic snapshot: disable interrupts briefly on ESP32 is not needed for
    // 32-bit reads, but noInterrupts/restoreInterrupts is safest.
    noInterrupts();
    long count = _pulseCount;
    interrupts();
    return count;
}

float Encoder::getVelocityCmPerSec() {
    unsigned long now = millis();
    unsigned long elapsed = now - _lastVelocityMs;

    // Recompute every 50 ms
    if (elapsed >= 50) {
        long currentCount = getPulseCount();
        long deltaPulses   = currentCount - _lastPulseSnapshot;
        float dt_sec       = elapsed / 1000.0f;

        // velocity = (pulses / pulses_per_rev) * circumference / dt
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
    _pulseCount += _direction;
}

// Static ISR stubs — each dispatches to the corresponding instance
void IRAM_ATTR Encoder::_isr0() { if (_instances[0]) _instances[0]->onRisingA(); }
void IRAM_ATTR Encoder::_isr1() { if (_instances[1]) _instances[1]->onRisingA(); }
void IRAM_ATTR Encoder::_isr2() { if (_instances[2]) _instances[2]->onRisingA(); }
void IRAM_ATTR Encoder::_isr3() { if (_instances[3]) _instances[3]->onRisingA(); }
