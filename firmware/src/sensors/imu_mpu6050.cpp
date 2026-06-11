#include "imu_mpu6050.h"
#include <Wire.h>
#include <math.h>

static constexpr uint8_t MPU_ADDR_LOW    = 0x68;
static constexpr uint8_t MPU_ADDR_HIGH   = 0x69;
static constexpr uint8_t REG_PWR_MGMT_1  = 0x6B;
static constexpr uint8_t REG_WHO_AM_I    = 0x75;
static constexpr uint8_t REG_SMPLRT_DIV  = 0x19;
static constexpr uint8_t REG_CONFIG      = 0x1A;
static constexpr uint8_t REG_GYRO_CONFIG = 0x1B;
static constexpr uint8_t REG_ACCEL_XOUT  = 0x3B;

static void scanI2cBus() {
    Serial.println("Scanning I2C bus on SDA=21 SCL=22...");

    int found = 0;
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("I2C device found at 0x%02X\n", address);
            found++;
        }
    }

    if (found == 0) {
        Serial.println("No I2C devices found. Check VCC/GND/SDA/SCL.");
    }
}

float ImuMpu6050::_normalizeAngle(float a) {
    if (!isfinite(a)) return 0.0f;
    while (a > PI) a -= 2.0f * PI;
    while (a < -PI) a += 2.0f * PI;
    return a;
}

bool ImuMpu6050::_writeByte(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(_address);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission(true) == 0;
}

bool ImuMpu6050::_readBytes(uint8_t reg, uint8_t* data, uint8_t len) {
    Wire.beginTransmission(_address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    uint8_t received = Wire.requestFrom(_address, len, (uint8_t)true);
    if (received != len) {
        return false;
    }

    for (uint8_t i = 0; i < len; i++) {
        data[i] = Wire.read();
    }
    return true;
}

bool ImuMpu6050::_readMotion6(int16_t* ax, int16_t* ay, int16_t* az,
                              int16_t* gx, int16_t* gy, int16_t* gz) {
    uint8_t data[14];
    if (!_readBytes(REG_ACCEL_XOUT, data, sizeof(data))) {
        return false;
    }

    *ax = (int16_t)((data[0] << 8) | data[1]);
    *ay = (int16_t)((data[2] << 8) | data[3]);
    *az = (int16_t)((data[4] << 8) | data[5]);
    *gx = (int16_t)((data[8] << 8) | data[9]);
    *gy = (int16_t)((data[10] << 8) | data[11]);
    *gz = (int16_t)((data[12] << 8) | data[13]);
    return true;
}

bool ImuMpu6050::begin() {
    Wire.begin(RobotConfig::MPU6050_SDA_PIN, RobotConfig::MPU6050_SCL_PIN);
    Wire.setClock(50000);
    Wire.setTimeOut(20);
    delay(100);

    scanI2cBus();

    for (uint8_t addr : {MPU_ADDR_LOW, MPU_ADDR_HIGH}) {
        _address = addr;
        uint8_t whoAmI = 0;

        if (!_readBytes(REG_WHO_AM_I, &whoAmI, 1)) {
            continue;
        }

        Serial.printf("MPU WHO_AM_I at 0x%02X = 0x%02X\n", addr, whoAmI);
        if (whoAmI == 0x68 || whoAmI == 0x70) {
            if (!_writeByte(REG_PWR_MGMT_1, 0x00)) {
                Serial.println("MPU6050 wake command failed.");
                return false;
            }

            delay(100);
            _writeByte(REG_SMPLRT_DIV, 0x09);  // 100 Hz sample rate
            _writeByte(REG_CONFIG, 0x03);      // DLPF ~44 Hz gyro bandwidth
            _writeByte(REG_GYRO_CONFIG, 0x08); // +/-500 deg/s (tránh saturation khi FastRotate)
            _lastUpdateMs = millis();
            _consecutiveReadErrors = 0;
            _healthy = true;
            Serial.printf("MPU6050 connected at 0x%02X.\n", addr);
            return true;
        }
    }

    Serial.println("MPU6050 WHO_AM_I read failed at both 0x68 and 0x69.");
    return false;
}

void ImuMpu6050::calibrate(int samples) {
    long biasAccum = 0;
    int validSamples = 0;
    int failedSamples = 0;

    for (int i = 0; i < samples; i++) {
        int16_t ax, ay, az, gx, gy, gz;
        if (_readMotion6(&ax, &ay, &az, &gx, &gy, &gz)) {
            if (validSamples < 20) {
                biasAccum += gz;
                validSamples++;
            } else {
                float mean = (float)biasAccum / validSamples;
                if (fabsf(gz - mean) < 220.0f) {
                    biasAccum += gz;
                    validSamples++;
                }
            }
            failedSamples = 0;
        } else {
            failedSamples++;
            if (failedSamples >= MAX_READ_ERRORS) {
                break;
            }
        }
        delay(2);
    }

    _gyroBiasZ = validSamples > 0 ? (float)biasAccum / validSamples : 0.0f;
    _healthy = validSamples > 0;
    _consecutiveReadErrors = _healthy ? 0 : MAX_READ_ERRORS;
    _nextRetryMs = _healthy ? 0 : millis() + RETRY_INTERVAL_MS;
    resetYaw();
}

bool ImuMpu6050::update() {
    unsigned long now = millis();
    if (!_healthy && now < _nextRetryMs) {
        _lastUpdateMs = now;
        return false;
    }

    float dt = (now - _lastUpdateMs) / 1000.0f;
    _lastUpdateMs = now;

    if (dt <= 0.0f) return _healthy;
    if (dt > MAX_DT_S) dt = MAX_DT_S;

    int16_t ax, ay, az, gx, gy, gz;
    if (!_readMotion6(&ax, &ay, &az, &gx, &gy, &gz)) {
        _consecutiveReadErrors++;
        if (_consecutiveReadErrors >= MAX_READ_ERRORS) {
            _healthy = false;
            _nextRetryMs = now + RETRY_INTERVAL_MS;
        }
        return false;
    }

    _consecutiveReadErrors = 0;
    _healthy = true;
    _nextRetryMs = 0;

    float gyroZ_rads = (gz - _gyroBiasZ) * GYRO_SCALE_RAD_S;
    if (fabsf(gyroZ_rads) < GYRO_DEADBAND_RAD_S) {
        gyroZ_rads = 0.0f;
    }
    _yawRad += gyroZ_rads * dt;

    _yawRad = _normalizeAngle(_yawRad);
    return true;
}

float ImuMpu6050::getAbsoluteYawRad() const {
    return _normalizeAngle(_yawRad - _referenceYawRad);
}

void ImuMpu6050::setReferenceAngle() {
    _referenceYawRad = _yawRad;
}

void ImuMpu6050::setAbsoluteYaw(float targetRad) {
    _referenceYawRad = _yawRad - targetRad;
}

void ImuMpu6050::resetYaw() {
    _yawRad = 0.0f;
    _referenceYawRad = 0.0f;
    _lastUpdateMs = millis();
    if (_healthy) {
        _consecutiveReadErrors = 0;
        _nextRetryMs = 0;
    }
}
