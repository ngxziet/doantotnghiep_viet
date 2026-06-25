// =====================================================================
// encoder_pulse_test.cpp  —  Test chẩn đoán đếm xung encoder (ESP32)

#include <Arduino.h>
// ---------------------------------------------------------------------
// MỤC ĐÍCH: xác định encoder có bị đếm GẤP ĐÔI (bounce/EMI) hay không.
//
// Cách đếm BYTE-FOR-BYTE giống src/sensors/encoder.cpp (Encoder::onRisingA):
//   - Cùng MIN_PULSE_INTERVAL_US = 6000 (lọc glitch µs)
//   - Cùng logic ISR: lấy micros, nếu (now - lastPulseMicros) < MIN thì bỏ,
//     ngược lại cập nhật lastPulseMicros và tăng pulseCount theo direction.
//   - Cùng cách đọc atomic (noInterrupts / interrupts) như Encoder::getPulseCount.
//   - Direction bám theo lệnh motor (encoder 1 kênh LM393 không tự biết chiều).
//
// CÁCH DÙNG:
//   1. Nạp sketch này (board ESP32). Mở Serial Monitor 115200.
//   2. KÊ XE LÊN cho bánh không chạm đất. Tắt nguồn motor (chỉ test cơ học).
//   3. Gõ 'r' để reset, quay TAY đúng N vòng bánh, xem số đếm.
//        - pulses ~ 20 * N   -> encoder SẠCH, không nhân đôi (khớp main).
//        - pulses >> 20 * N -> có nhân đôi/bounce (cần Schmitt trigger hoặc chỉnh debounce).
//        - pulses < 20 * N  -> debounce đang nuốt xung thật (MIN_INTERVAL_US quá lớn).
//   4. (tuỳ chọn) Bật motor chạy, so pulses lúc chạy với lúc quay tay
//      -> nếu pulses lúc chạy cao hơn nhiều = có nhiễu từ motor/L298N.
//
// LỆNH Serial:
//   r = reset bộ đếm
//   p = in ngay (không đợi 500ms)
//   N (số nguyên 1..20) = đặt số vòng kỳ vọng (mặc định 1) để so sánh
// =====================================================================

// ---- Khớp với config.h của project ----
static constexpr int ENCODER_LEFT_PIN  = 34;
static constexpr int ENCODER_RIGHT_PIN = 32;
static constexpr int PULSES_PER_REV    = 20;
static constexpr float WHEEL_DIAM_CM   = 6.5f;
static constexpr float CM_PER_PULSE    = 3.14159265f * WHEEL_DIAM_CM / PULSES_PER_REV;

// Ngưỡng debounce ĐANG dùng trong firmware (encoder.h). Đổi để thử nghiệm.
static constexpr unsigned long MIN_INTERVAL_US = 6000;

static constexpr unsigned long PRINT_PERIOD_MS = 500;

// Sao y Encoder trong main: 1 bộ đếm duy nhất, có direction, KHÔNG RAW/minGap.
struct EncCounter {
    int  pin;
    volatile long          pulseCount;       // đã debounce + có dấu theo direction
    volatile int           direction;        // +1 / -1 / 0  (bám theo lệnh motor)
    volatile unsigned long lastPulseMicros;  // mốc cạnh hợp lệ gần nhất
};

EncCounter encL = {ENCODER_LEFT_PIN,  0, 1, 0};
EncCounter encR = {ENCODER_RIGHT_PIN, 0, 1, 0};

// Sao y Encoder::onRisingA — IRAM_ATTR, cùng MIN_PULSE_INTERVAL_US, cùng thứ tự thao tác.
static inline void IRAM_ATTR onRising(EncCounter& e) {
    unsigned long now = micros();
    if (now - e.lastPulseMicros < MIN_INTERVAL_US) return;
    e.lastPulseMicros = now;
    e.pulseCount += e.direction;
}

void IRAM_ATTR isrLeft()  { onRising(encL); }
void IRAM_ATTR isrRight() { onRising(encR); }

// Đọc atomic — giống Encoder::getPulseCount
static long readLeftPulses()  { noInterrupts(); long v = encL.pulseCount; interrupts(); return v; }
static long readRightPulses() { noInterrupts(); long v = encR.pulseCount; interrupts(); return v; }

// Số vòng kỳ vọng để so sánh (mặc định 1). Đổi bằng cách gõ số vào Serial.
static int g_expectedRevs = 1;

// Sao y Encoder::reset (tắt interrupt -> ghi 0 -> bật lại).
void resetCounters() {
    noInterrupts();
    encL.pulseCount = 0;
    encL.lastPulseMicros = 0;
    encR.pulseCount = 0;
    encR.lastPulseMicros = 0;
    interrupts();
    Serial.printf(">> RESET. Quay tay %d vong banh roi xem so dem (MONG DOI: %d xung/benh).\n",
                  g_expectedRevs, g_expectedRevs * PULSES_PER_REV);
}

void printStatus() {
    long left  = readLeftPulses();
    long right = readRightPulses();

    float revL = left  / (float)PULSES_PER_REV;
    float revR = right / (float)PULSES_PER_REV;

    long expectedPulses = (long)PULSES_PER_REV * g_expectedRevs;
    long diffL = left  - expectedPulses;
    long diffR = right - expectedPulses;

    Serial.println(F("---------------------------------------------"));
    Serial.printf("  ky-vong: %d vong * %d xung/vong = %ld xung/benh\n",
                  g_expectedRevs, PULSES_PER_REV, expectedPulses);
    Serial.printf("LEFT  pin=%d  pulses=%6ld  | rev=%.2f  dist=%.1fcm  | so-voi-ky-vong=%+ld\n",
                  ENCODER_LEFT_PIN, left, revL, left * CM_PER_PULSE, diffL);
    Serial.printf("RIGHT pin=%d  pulses=%6ld  | rev=%.2f  dist=%.1fcm  | so-voi-ky-vong=%+ld\n",
                  ENCODER_RIGHT_PIN, right, revR, right * CM_PER_PULSE, diffR);
    Serial.printf("diff(L-R) = %ld xung  (=%.2fcm lech)\n",
                  left - right, (left - right) * CM_PER_PULSE);

    // Cảnh báo nhanh — dựa trên sai lệch so với kỳ vọng (giống tinh thần main, không dùng RAW)
    if (left > expectedPulses + 5)
        Serial.printf("   [!] LEFT  dem %ld > ky-vong %ld -> co nhan doi/bounce.\n", left, expectedPulses);
    else if (left < expectedPulses - 5 && expectedPulses > 0)
        Serial.printf("   [!] LEFT  dem %ld < ky-vong %ld -> debounce dang nuot xung that.\n", left, expectedPulses);

    if (right > expectedPulses + 5)
        Serial.printf("   [!] RIGHT dem %ld > ky-vong %ld -> co nhan doi/bounce.\n", right, expectedPulses);
    else if (right < expectedPulses - 5 && expectedPulses > 0)
        Serial.printf("   [!] RIGHT dem %ld < ky-vong %ld -> debounce dang nuot xung that.\n", right, expectedPulses);
}

void setup() {
    Serial.begin(115200);
    delay(300);

    // GPIO34/35/36/39 ESP32 KHONG co pull-up noi -> module encoder phai tu keo (thuong da co).
    pinMode(ENCODER_LEFT_PIN,  INPUT);
    pinMode(ENCODER_RIGHT_PIN, INPUT);

    attachInterrupt(digitalPinToInterrupt(ENCODER_LEFT_PIN),  isrLeft,  RISING);
    attachInterrupt(digitalPinToInterrupt(ENCODER_RIGHT_PIN), isrRight, RISING);

    Serial.println(F("\n=== ENCODER PULSE TEST (giong main Encoder::onRisingA) ==="));
    Serial.printf("PULSES_PER_REV=%d  CM_PER_PULSE=%.3f  MIN_INTERVAL_US=%lu\n",
                  PULSES_PER_REV, CM_PER_PULSE, MIN_INTERVAL_US);
    Serial.println(F("Ke xe len, tat motor. Go 'r' reset roi quay tay N vong banh."));
    Serial.println(F("Mong doi: pulses = 20*N moi benh. Go so (1..20) de doi N."));
    Serial.println(F("Lenh: r=reset  p=in ngay  N=dat so vong ky vong\n"));
    resetCounters();
}

// Đọc 1 dòng lệnh từ Serial: chấp nhận "r", "p", hoặc số nguyên 1..20.
static void handleSerialLine() {
    static char buf[16];
    static size_t idx = 0;

    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            buf[idx] = 0;
            if (idx == 0) continue;
            if (buf[0] == 'r' || buf[0] == 'R') {
                resetCounters();
            } else if (buf[0] == 'p' || buf[0] == 'P') {
                printStatus();
            } else {
                int n = atoi(buf);
                if (n >= 1 && n <= 20) {
                    g_expectedRevs = n;
                    Serial.printf(">> Dat ky vong = %d vong (%d xung/benh).\n",
                                  g_expectedRevs, g_expectedRevs * PULSES_PER_REV);
                    resetCounters();
                } else {
                    Serial.println(F(">> Lenh khong hop le. Dung: r | p | N (1..20)"));
                }
            }
            idx = 0;
        } else if (idx < sizeof(buf) - 1) {
            buf[idx++] = c;
        }
    }
}

void loop() {
    handleSerialLine();

    static unsigned long lastPrint = 0;
    static long lastLeftSum = 0;
    static long lastRightSum = 0;
    if (millis() - lastPrint >= PRINT_PERIOD_MS) {
        lastPrint = millis();
        long l = readLeftPulses();
        long r = readRightPulses();
        if (l != lastLeftSum || r != lastRightSum) {  // chi in khi co thay doi
            lastLeftSum = l;
            lastRightSum = r;
            printStatus();
        }
    }
}