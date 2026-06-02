#pragma once

#include <Arduino.h>
#include "../config.h"

// Tunable hardware constants
static constexpr int   ENCODER_PULSES_PER_REV  = RobotConfig::ENCODER_PULSES_PER_REV;
static constexpr float ENCODER_WHEEL_DIAM_CM   = RobotConfig::WHEEL_DIAM_CM;

class Encoder {
public:
    explicit Encoder(int pinA, int pinB = -1);

    // Attach interrupt on pinA RISING edge
    void begin();

    // Total accumulated pulse count (thread-safe snapshot)
    long getPulseCount();

    // Velocity in cm/s, recomputed every 50 ms
    float getVelocityCmPerSec();

    // Zero pulse count and velocity
    void reset();
    void resetPulses() { reset(); }

    float getDistanceCm();

    // For single-channel encoders (LM393 D0), direction comes from motor command.
    void setDirection(int direction);

    // Called from ISR — do not call directly
    void IRAM_ATTR onRisingA();

private:
    int _pinA;
    int _pinB;

    volatile long _pulseCount;
    volatile int  _direction;

    // Velocity tracking
    long  _lastPulseSnapshot;
    unsigned long _lastVelocityMs;
    float _velocityCmPerSec;

    // Up to 4 Encoder instances supported via static dispatch table
    static Encoder* _instances[4];
    static int      _instanceCount;

    // Static ISR stubs — dispatched to instance onRisingA()
    static void IRAM_ATTR _isr0();
    static void IRAM_ATTR _isr1();
    static void IRAM_ATTR _isr2();
    static void IRAM_ATTR _isr3();
};
