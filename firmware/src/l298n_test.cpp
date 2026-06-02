#include <Arduino.h>

static constexpr int IN1 = 12;
static constexpr int IN2 = 13;
static constexpr int ENA = 14;
static constexpr int IN3 = 26;
static constexpr int IN4 = 27;
static constexpr int ENB = 25;

static constexpr int PWM_FREQ = 5000;
static constexpr int PWM_BITS = 8;
static constexpr int PWM_LEFT_CH = 0;
static constexpr int PWM_RIGHT_CH = 1;
static constexpr int TEST_SPEED = 255;
static constexpr unsigned long STEP_MS = 2000;

static void setLeft(int speed) {
    speed = constrain(speed, -255, 255);

    if (speed > 0) {
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
    } else if (speed < 0) {
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
    } else {
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
    }

    ledcWrite(PWM_LEFT_CH, abs(speed));
}

static void setRight(int speed) {
    speed = constrain(speed, -255, 255);

    if (speed > 0) {
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
    } else if (speed < 0) {
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, HIGH);
    } else {
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
    }

    ledcWrite(PWM_RIGHT_CH, abs(speed));
}

static void setMotors(int left, int right, const char* label) {
    Serial.printf("%s: left=%d right=%d\n", label, left, right);
    setLeft(left);
    setRight(right);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);

    ledcSetup(PWM_LEFT_CH, PWM_FREQ, PWM_BITS);
    ledcSetup(PWM_RIGHT_CH, PWM_FREQ, PWM_BITS);
    ledcAttachPin(ENA, PWM_LEFT_CH);
    ledcAttachPin(ENB, PWM_RIGHT_CH);

    setMotors(0, 0, "stop");
    Serial.println("L298N standalone test started.");
    Serial.println("Pins: IN1=12 IN2=13 ENA=14 IN3=26 IN4=27 ENB=25");
}

void loop() {
    setMotors(TEST_SPEED, TEST_SPEED, "forward");
    delay(STEP_MS);

    setMotors(0, 0, "stop");
    delay(800);

    setMotors(-TEST_SPEED, -TEST_SPEED, "backward");
    delay(STEP_MS);

    setMotors(0, 0, "stop");
    delay(800);

    setMotors(TEST_SPEED, 0, "left motor only");
    delay(STEP_MS);

    setMotors(0, TEST_SPEED, "right motor only");
    delay(STEP_MS);

    setMotors(0, 0, "stop");
    delay(1200);
}
