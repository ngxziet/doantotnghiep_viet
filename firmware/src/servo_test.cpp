#include <Arduino.h>

// --- Servo (LEDC thô, kênh riêng) ---
static constexpr int SERVO_PIN = 19;
static constexpr int SERVO_PWM_CH = 2;
static constexpr int SERVO_PWM_FREQ = 50;
static constexpr int SERVO_PWM_BITS = 16;
static constexpr int SERVO_MIN_PULSE_US = 500;   // 0.5ms → -90° (1-2ms chỉ ±45°)
static constexpr int SERVO_MAX_PULSE_US = 2500;  // 2.5ms → +90°
static constexpr unsigned long STEP_MS = 2000;
static constexpr unsigned long POWER_ON_DELAY_MS = 5000;
static constexpr unsigned long SERVO_SETTLE_MS = 350;  // chờ servo quay xong mới đo sonar

// --- HC-SR04 (chân lấy từ config để khớp với firmware chính) ---
static constexpr int TRIG_PIN = 5;
static constexpr int ECHO_PIN = 18;
static constexpr int  HCSR_SAMPLES = 5;             // số mẫu lấy trung vị
static constexpr unsigned long HCSR_GAP_MS = 10;    // nghỉ giữa các mẫu
static constexpr unsigned long HCSR_TIMEOUT_US = 30000UL;  // ~510cm
static constexpr float MAX_RANGE_CM = 200.0f;       // trả về khi ngoài tầm / timeout

static void writeAngle(int deg) {
    deg = constrain(deg, 0, 180);
    long span = SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US;
    long pulseUs = SERVO_MIN_PULSE_US + (span * deg) / 180;
    long maxDuty = (1L << SERVO_PWM_BITS) - 1;
    long periodUs = 1000000L / SERVO_PWM_FREQ;
    long duty = (pulseUs * maxDuty) / periodUs;
    ledcWrite(SERVO_PWM_CH, (uint32_t)duty);
}

// Đo một mẫu thô (cm). Trả về MAX_RANGE_CM nếu timeout / vượt tầm.
static float singleReadCm() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, HCSR_TIMEOUT_US);
    if (duration == 0) return MAX_RANGE_CM;

    float dist = duration * 0.0343f / 2.0f;  // tốc độ âm thanh / 2 (đi + về)
    return (dist > MAX_RANGE_CM) ? MAX_RANGE_CM : dist;
}

// Trung vị 5 mẫu để lọc nhiễu (insertion sort đơn giản).
static float medianReadCm() {
    float arr[HCSR_SAMPLES];
    for (int i = 0; i < HCSR_SAMPLES; i++) {
        arr[i] = singleReadCm();
        if (i < HCSR_SAMPLES - 1) delay(HCSR_GAP_MS);
    }
    for (int i = 1; i < HCSR_SAMPLES; i++) {
        float key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
    return arr[HCSR_SAMPLES / 2];
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Servo
    ledcSetup(SERVO_PWM_CH, SERVO_PWM_FREQ, SERVO_PWM_BITS);
    ledcAttachPin(SERVO_PIN, SERVO_PWM_CH);

    // HC-SR04
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);

    Serial.println("=== Servo + HC-SR04 test ===");
    Serial.println("Plug servo + sonar power NOW, then wait 5s...");
    Serial.println("Starting position = 90 deg (center).");

    // Bắt đầu ở 90° để giảm giật (center là điểm an toàn nhất).
    writeAngle(90);

    // Đợi 5s để user cắm nguồn xong rồi mới bắt đầu cycle.
    delay(POWER_ON_DELAY_MS);

    Serial.println("Cycle: 0 -> 90 -> 180 (every 2s).");
    Serial.println("At each angle: wait servo settle, then read HC-SR04 (median of 5).");
    Serial.println();
}

void loop() {
    // 0 deg: xung MIN — đầu dò sonar quay về phía SERVO_MIN_PULSE
    writeAngle(0);
    delay(SERVO_SETTLE_MS);
    float d0 = medianReadCm();
    Serial.printf("deg=0    dist=%.1f cm\n", d0);
    delay(STEP_MS);

    // 90 deg: nhìn thẳng
    writeAngle(90);
    delay(SERVO_SETTLE_MS);
    float d90 = medianReadCm();
    Serial.printf("deg=90   dist=%.1f cm\n", d90);
    delay(STEP_MS);

    // 180 deg: xung MAX — đầu dò sonar quay về phía SERVO_MAX_PULSE
    writeAngle(180);
    delay(SERVO_SETTLE_MS);
    float d180 = medianReadCm();
    Serial.printf("deg=180  dist=%.1f cm\n", d180);
    delay(STEP_MS);
}
