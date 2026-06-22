// =====================================================================
// encoder_pulse_test.ino  —  Test chẩn đoán đếm xung encoder (ESP32)
// ---------------------------------------------------------------------
// MỤC ĐÍCH: xác định encoder có bị đếm GẤP ĐÔI (bounce/EMI) hay không.
//
// Đếm song song 2 kiểu trên CÙNG 1 chân:
//   - RAW       : đếm mọi cạnh lên RISING, KHÔNG lọc  -> thấy xung thừa
//   - DEBOUNCED : RISING + bỏ xung cách nhau < MIN_INTERVAL_US
// Đồng thời ghi lại KHOẢNG CÁCH NHỎ NHẤT giữa 2 cạnh lên (us) -> để chỉnh debounce.
//
// CÁCH DÙNG:
//   1. Nạp sketch này (board ESP32). Mở Serial Monitor 115200.
//   2. KÊ XE LÊN cho bánh không chạm đất. Tắt nguồn motor (chỉ test cơ học).
//   3. Gõ 'r' để reset, quay TAY đúng 1 vòng bánh, xem số đếm.
//        - RAW ~ 20  & DEBOUNCED ~ 20  -> encoder SẠCH, không nhân đôi.
//        - RAW ~ 40+ & DEBOUNCED ~ 20  -> bounce/EMI, debounce đang cứu đúng.
//        - DEBOUNCED < 20             -> debounce QUÁ LỚN, đang nuốt xung thật.
//   4. (tuỳ chọn) Bật motor chạy, so RAW lúc chạy với lúc quay tay
//      -> RAW tăng thêm khi motor chạy = có nhiễu từ motor/L298N.
//   5. Xem MIN_GAP để chọn MIN_PULSE_INTERVAL_US: đặt nhỏ hơn khoảng cách
//      xung THẬT (lúc chạy nhanh nhất) nhưng lớn hơn khoảng cách cạnh GIẢ.
//
// LỆNH Serial:  r = reset bộ đếm   |   p = in ngay (không đợi 500ms)
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

struct EncCounter {
    int  pin;
    volatile unsigned long rawCount;        // mọi cạnh lên
    volatile unsigned long debCount;        // cạnh lên đã lọc
    volatile unsigned long lastMicros;      // mốc cạnh lên hợp lệ gần nhất (cho debounce)
    volatile unsigned long lastRawMicros;   // mốc cạnh lên thô gần nhất (cho min-gap)
    volatile unsigned long minGapUs;        // khoảng cách nhỏ nhất giữa 2 cạnh thô
};

EncCounter encL = {ENCODER_LEFT_PIN,  0, 0, 0, 0, 0xFFFFFFFF};
EncCounter encR = {ENCODER_RIGHT_PIN, 0, 0, 0, 0, 0xFFFFFFFF};

static inline void IRAM_ATTR onEdge(EncCounter& e) {
    unsigned long now = micros();

    // --- min gap giữa 2 cạnh thô liên tiếp ---
    unsigned long rawGap = now - e.lastRawMicros;
    if (e.lastRawMicros != 0 && rawGap < e.minGapUs) e.minGapUs = rawGap;
    e.lastRawMicros = now;

    // --- đếm thô ---
    e.rawCount++;

    // --- đếm có debounce ---
    if (now - e.lastMicros >= MIN_INTERVAL_US) {
        e.lastMicros = now;
        e.debCount++;
    }
}

void IRAM_ATTR isrLeft()  { onEdge(encL); }
void IRAM_ATTR isrRight() { onEdge(encR); }

void resetCounters() {
    noInterrupts();
    encL.rawCount = encL.debCount = 0; encL.lastMicros = encL.lastRawMicros = 0; encL.minGapUs = 0xFFFFFFFF;
    encR.rawCount = encR.debCount = 0; encR.lastMicros = encR.lastRawMicros = 0; encR.minGapUs = 0xFFFFFFFF;
    interrupts();
    Serial.println(F(">> RESET. Quay tay 1 vong banh roi xem so dem."));
}

void printStatus() {
    noInterrupts();
    unsigned long rawL = encL.rawCount, debL = encL.debCount, gapL = encL.minGapUs;
    unsigned long rawR = encR.rawCount, debR = encR.debCount, gapR = encR.minGapUs;
    interrupts();

    float revL = debL / (float)PULSES_PER_REV;
    float revR = debR / (float)PULSES_PER_REV;

    Serial.println(F("---------------------------------------------"));
    Serial.printf("LEFT (pin %d): RAW=%lu  DEB=%lu  | rev=%.2f  dist=%.1fcm  minGap=%luus\n",
                  ENCODER_LEFT_PIN, rawL, debL, revL, debL * CM_PER_PULSE,
                  gapL == 0xFFFFFFFF ? 0UL : gapL);
    Serial.printf("RIGHT(pin %d): RAW=%lu  DEB=%lu  | rev=%.2f  dist=%.1fcm  minGap=%luus\n",
                  ENCODER_RIGHT_PIN, rawR, debR, revR, debR * CM_PER_PULSE,
                  gapR == 0xFFFFFFFF ? 0UL : gapR);

    // Cảnh báo nhanh
    if (rawL >= debL * 1.5 && debL > 0) Serial.println(F("   [!] LEFT  RAW >> DEB  -> co bounce/nhan doi (debounce dang loc)."));
    if (rawR >= debR * 1.5 && debR > 0) Serial.println(F("   [!] RIGHT RAW >> DEB  -> co bounce/nhan doi (debounce dang loc)."));
}

void setup() {
    Serial.begin(115200);
    delay(300);

    // GPIO34/35/36/39 ESP32 KHONG co pull-up noi -> module encoder phai tu keo (thuong da co).
    pinMode(ENCODER_LEFT_PIN,  INPUT);
    pinMode(ENCODER_RIGHT_PIN, INPUT);

    attachInterrupt(digitalPinToInterrupt(ENCODER_LEFT_PIN),  isrLeft,  RISING);
    attachInterrupt(digitalPinToInterrupt(ENCODER_RIGHT_PIN), isrRight, RISING);

    Serial.println(F("\n=== ENCODER PULSE TEST ==="));
    Serial.printf("PULSES_PER_REV=%d  CM_PER_PULSE=%.3f  MIN_INTERVAL_US=%lu\n",
                  PULSES_PER_REV, CM_PER_PULSE, MIN_INTERVAL_US);
    Serial.println(F("Le: ke xe len, tat motor, go 'r' reset roi quay tay 1 vong banh."));
    Serial.println(F("Mong doi: RAW va DEB deu ~20/vong. Neu RAW ~40 = nhan doi.\n"));
    resetCounters();
}

void loop() {
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'r' || c == 'R') resetCounters();
        else if (c == 'p' || c == 'P') printStatus();
    }

    static unsigned long lastPrint = 0;
    static unsigned long lastRawSum = 0;
    if (millis() - lastPrint >= PRINT_PERIOD_MS) {
        lastPrint = millis();
        unsigned long sum = encL.rawCount + encR.rawCount;
        if (sum != lastRawSum) {          // chi in khi co thay doi
            lastRawSum = sum;
            printStatus();
        }
    }
}
