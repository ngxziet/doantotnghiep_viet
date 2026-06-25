// =====================================================================
// encoder_calib_test.cpp  —  Hiệu chuẩn encoder: quay TỰ ĐỘNG (motor) / quay TAY
//
// MỤC ĐÍCH: đo CHÍNH XÁC số xung mỗi vòng bánh để chỉnh ENCODER_PULSES_PER_REV.
//   - Quay bằng MOTOR (nhanh) = đúng tốc độ xe chạy → số xung "sạch" để lấy làm chuẩn.
//   - Quay bằng TAY (chậm) = để so sánh, thấy doubling tốc-độ-thấp của LM393.
//
// CÁCH DÙNG:
//   1) Nạp:  pio run -e encoder_calib_test -t upload   (mở Serial 115200).
//   2) KÊ XE LÊN cho bánh không chạm đất. Dán băng keo đánh dấu 1 bánh để ĐẾM VÒNG.
//   3) Gõ 'N' (vd 5) = số vòng bánh bạn sẽ đếm (nhiều vòng → chính xác hơn).
//   4) Gõ '1' = motor tự quay  HOẶC  '2' = bạn quay tay.
//      Đếm đủ N vòng bánh THỰC TẾ rồi gõ 's' để DỪNG + ĐỌC kết quả.
//   5) Xem dòng ">> Đặt ENCODER_PULSES_PER_REV ~ X" → sửa số đó vào config.h.
//
// LỆNH Serial:  1=motor quay  2=quay tay  s=dừng+đọc  r=reset  N(1..50)=số vòng sẽ đếm
// =====================================================================
#include <Arduino.h>

// ---- Chân & hằng số khớp config.h / motor_driver.h ----
static constexpr int   ENC_L = 34, ENC_R = 32;        // encoder trái/phải
static constexpr int   IN1 = 12, IN2 = 13, ENA = 14;  // motor trái  (LEDC kênh 0)
static constexpr int   IN3 = 26, IN4 = 27, ENB = 25;  // motor phải  (LEDC kênh 1)
static constexpr int   CH_L = 0, CH_R = 1;
static constexpr int   PWM_FREQ = 5000, PWM_BITS = 8;
static constexpr int   PULSES_PER_REV = 20;           // giá trị ĐANG dùng (để so sánh)
static constexpr float WHEEL_DIAM_CM  = 6.5f;
static constexpr float WHEEL_CIRC_CM  = 3.14159265f * WHEEL_DIAM_CM;        // 1 vòng = chu vi
static constexpr float CM_PER_PULSE   = WHEEL_CIRC_CM / PULSES_PER_REV;
static constexpr unsigned long MIN_INTERVAL_US = 6000;   // khớp debounce encoder.h
static constexpr int   TEST_PWM = 120;                   // tốc độ quay motor khi test (vừa phải để đếm vòng)
static constexpr unsigned long AUTO_TIMEOUT_MS = 20000;  // tự dừng sau 20s phòng hờ

// ---- Encoder ISR (sao y encoder.cpp: cùng debounce, đếm cạnh lên) ----
volatile long pcL = 0, pcR = 0;
volatile unsigned long lastL = 0, lastR = 0;
void IRAM_ATTR isrL() { unsigned long n = micros(); if (n - lastL < MIN_INTERVAL_US) return; lastL = n; pcL++; }
void IRAM_ATTR isrR() { unsigned long n = micros(); if (n - lastR < MIN_INTERVAL_US) return; lastR = n; pcR++; }
static long getL() { noInterrupts(); long v = pcL; interrupts(); return v; }
static long getR() { noInterrupts(); long v = pcR; interrupts(); return v; }
static void resetEnc() { noInterrupts(); pcL = pcR = 0; lastL = lastR = 0; interrupts(); }

// ---- Motor (đi thẳng tiến, đủ để quay bánh) ----
static void motorStop() {
    ledcWrite(CH_L, 0); ledcWrite(CH_R, 0);
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}
static void motorForward(int pwm) {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    ledcWrite(CH_L, pwm); ledcWrite(CH_R, pwm);
}

static int  g_revs = 1;            // số vòng bánh dùng làm mốc
static bool autoRunning = false;   // motor đang quay (mode '1')
static bool g_autoMode = false;    // lần đo gần nhất là motor (true) hay tay (false)
static unsigned long autoStart = 0;

static void report(const char* tag, bool autoMode) {
    long l = getL(), r = getR();
    long avg = (l + r) / 2;
    Serial.println(F("=================================================="));
    Serial.printf("[%s]\n", tag);
    Serial.printf("LEFT pulses=%ld | RIGHT pulses=%ld | dist(TB)=%.1fcm  (1 vong banh = %.1fcm)\n",
                  l, r, avg * CM_PER_PULSE, WHEEL_CIRC_CM);
    Serial.println(F("--- CACH SUA ENCODER_PULSES_PER_REV ---"));
    if (autoMode) {
        // Motor đã chạy tới target = g_revs*20 xung. KHÔNG suy được từ số xung (vòng lặp logic).
        // Phải KIỂM TRA vật lý: bánh quay thực mấy vòng, hoặc xe đi thực bao nhiêu cm.
        Serial.printf("Motor da chay %ld xung (firmware coi = %d vong).\n", avg, g_revs);
        Serial.printf("KE XE LEN -> nhin bang keo: banh quay THUC may vong?\n");
        Serial.printf("   - dung %d vong         -> PULSES_PER_REV=%d OK (giu nguyen)\n", g_revs, PULSES_PER_REV);
        Serial.printf("   - chi ~%.1f vong (nua) -> encoder NHAN DOI  -> dat %d\n", g_revs / 2.0f, PULSES_PER_REV * 2);
        Serial.printf("HOAC dat xe XUONG SAN, do quang xe di THUC bang thuoc:\n");
        Serial.printf("   - di ~%.1fcm       -> PULSES_PER_REV=%d OK\n", WHEEL_CIRC_CM * g_revs, PULSES_PER_REV);
        Serial.printf("   - di ~%.1fcm (nua) -> nhan doi -> dat %d\n", WHEEL_CIRC_CM * g_revs / 2.0f, PULSES_PER_REV * 2);
    } else {
        // Quay tay đúng g_revs vòng -> đo trực tiếp được xung/vòng.
        float ppv = g_revs > 0 ? avg / (float)g_revs : 0;
        Serial.printf("Ban quay tay dung %d vong -> %.1f xung/vong\n", g_revs, ppv);
        Serial.printf(">> Dat ENCODER_PULSES_PER_REV = %.0f\n", ppv);
        if (ppv > PULSES_PER_REV * 1.5f)
            Serial.println(F("   [!] Gan gap doi 20 -> DOUBLING toc do thap. So sanh voi mode '1' (motor)."));
    }
    Serial.println(F("=================================================="));
}

void setup() {
    Serial.begin(115200); delay(300);
    pinMode(ENC_L, INPUT); pinMode(ENC_R, INPUT);
    attachInterrupt(digitalPinToInterrupt(ENC_L), isrL, RISING);
    attachInterrupt(digitalPinToInterrupt(ENC_R), isrR, RISING);
    pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
    ledcSetup(CH_L, PWM_FREQ, PWM_BITS); ledcSetup(CH_R, PWM_FREQ, PWM_BITS);
    ledcAttachPin(ENA, CH_L); ledcAttachPin(ENB, CH_R);
    motorStop();

    Serial.println(F("\n=== ENCODER CALIB TEST  (motor / tay) ==="));
    Serial.printf("PULSES_PER_REV hien tai=%d  | 1 vong banh = %.1f cm\n", PULSES_PER_REV, WHEEL_CIRC_CM);
    Serial.println(F("KE XE LEN (banh khong cham dat). Danh dau 1 banh de DEM VONG."));
    Serial.println(F("Lenh:  N=so vong se dem (vd 5)  |  1=motor quay  |  2=quay tay  |  s=dung+doc  |  r=reset"));
    resetEnc();
}

static void handleSerial() {
    static char buf[16]; static size_t idx = 0;
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            buf[idx] = 0; if (idx == 0) continue;
            if (buf[0] == '1') {
                resetEnc(); autoRunning = true; g_autoMode = true; autoStart = millis(); motorForward(TEST_PWM);
                Serial.printf(">> MOTOR QUAY (PWM %d) toi %d vong (%d xung) roi TU DUNG. ('s' de dung som)\n",
                              TEST_PWM, g_revs, g_revs * PULSES_PER_REV);
            } else if (buf[0] == '2') {
                motorStop(); autoRunning = false; g_autoMode = false; resetEnc();
                Serial.printf(">> CHE DO TAY. Quay tay du %d vong banh roi go 's' de doc.\n", g_revs);
            } else if (buf[0] == 's' || buf[0] == 'S') {
                motorStop(); autoRunning = false; report("DUNG TAY", g_autoMode);
            } else if (buf[0] == 'r' || buf[0] == 'R') {
                motorStop(); autoRunning = false; resetEnc(); Serial.println(F(">> RESET bo dem."));
            } else {
                int n = atoi(buf);
                if (n >= 1 && n <= 50) { g_revs = n; Serial.printf(">> Se dem %d vong banh.\n", n); }
                else Serial.println(F(">> Lenh: 1 | 2 | s | r | N(1..50)"));
            }
            idx = 0;
        } else if (idx < sizeof(buf) - 1) buf[idx++] = c;
    }
}

void loop() {
    handleSerial();
    if (autoRunning) {
        long avg = (getL() + getR()) / 2;
        if (avg >= (long)g_revs * PULSES_PER_REV) {       // đã quay đủ "g_revs vòng" theo PULSES_PER_REV
            motorStop(); autoRunning = false;
            Serial.printf(">> Da quay du %d vong (theo PULSES_PER_REV=%d) -> TU DUNG.\n", g_revs, PULSES_PER_REV);
            report("AUTO MOTOR", true);
        } else if (millis() - autoStart > AUTO_TIMEOUT_MS) {
            motorStop(); autoRunning = false;
            Serial.println(F(">> AUTO timeout 20s -> tu dung.")); report("TIMEOUT", true);
        }
    }
    // In live mỗi 500ms khi số đếm thay đổi
    static unsigned long lp = 0; static long ll = -1, lr = -1;
    if (millis() - lp >= 500) {
        lp = millis(); long l = getL(), r = getR();
        if (l != ll || r != lr) {
            ll = l; lr = r;
            Serial.printf("   ... LEFT=%ld  RIGHT=%ld   (~%.2f / %.2f vong @%d xung/vong)\n",
                          l, r, l / (float)PULSES_PER_REV, r / (float)PULSES_PER_REV, PULSES_PER_REV);
        }
    }
}
