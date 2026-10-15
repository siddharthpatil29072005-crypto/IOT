#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// Pin assignments
constexpr uint8_t TRIGGER_LEFT_PIN = 9;
constexpr uint8_t ECHO_LEFT_PIN = 8;
constexpr uint8_t TRIGGER_RIGHT_PIN = 7;
constexpr uint8_t ECHO_RIGHT_PIN = 6;
constexpr uint8_t GREEN_LED_PIN = 3;
constexpr uint8_t RED_LED_PIN = 4;
constexpr uint8_t FILM_LEFT_SERVO_PIN = 5;
constexpr uint8_t FILM_RIGHT_SERVO_PIN = 10;

// Process parameters (centimetres)
float sensorSeparationCm = 80.0f;
constexpr float TARGET_DIAMETER_CM = 50.0f;
constexpr float ALLOWED_TOLERANCE_CM = 2.0f;
constexpr float MIN_VALID_DISTANCE_CM = 2.0f;
constexpr float MAX_VALID_DISTANCE_CM = 400.0f;

constexpr uint32_t ECHO_TIMEOUT_US = 25000UL;
constexpr uint32_t SENSOR_SETTLE_US = 2500UL;
constexpr uint32_t MEASUREMENT_INTERVAL_MS = 100UL;
constexpr uint32_t DISPLAY_INTERVAL_MS = 250UL;
constexpr uint32_t LED_BLINK_INTERVAL_MS = 300UL;

// Wokwi demo mode: animate the bubble continuously between 46 cm and 54 cm.
// Set to false for physical HC-SR04 measurements or slider-controlled Wokwi input.
constexpr bool CONTINUOUS_FILM_SIMULATION = false;
constexpr float SIMULATION_AMPLITUDE_CM = 4.0f;
constexpr uint32_t SIMULATION_PERIOD_MS = 12000UL;

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo filmLeftServo;
Servo filmRightServo;

enum class SensorPhase : uint8_t {
  Idle,
  WaitingForRise,
  WaitingForFall
};

struct UltrasonicSensor {
  uint8_t triggerPin;
  uint8_t echoPin;
  SensorPhase phase = SensorPhase::Idle;
  uint32_t deadlineUs = 0;
  uint32_t riseTimeUs = 0;
  float distanceCm = NAN;
  bool valid = false;

  UltrasonicSensor(uint8_t trigger, uint8_t echo)
      : triggerPin(trigger), echoPin(echo) {}
};

UltrasonicSensor leftSensor{TRIGGER_LEFT_PIN, ECHO_LEFT_PIN};
UltrasonicSensor rightSensor{TRIGGER_RIGHT_PIN, ECHO_RIGHT_PIN};

uint32_t nextMeasurementMs = 0;
uint32_t lastDisplayMs = 0;
float bubbleDiameterCm = NAN;
bool diameterValid = false;
bool inTolerance = false;
bool indicatorState = false;
uint32_t lastBlinkMs = 0;

bool timeReached(uint32_t now, uint32_t target) {
  return static_cast<int32_t>(now - target) >= 0;
}

void startMeasurement(UltrasonicSensor &sensor) {
  digitalWrite(sensor.triggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(sensor.triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(sensor.triggerPin, LOW);

  sensor.phase = SensorPhase::WaitingForRise;
  sensor.deadlineUs = micros() + ECHO_TIMEOUT_US;
  sensor.valid = false;
}

void serviceMeasurement(UltrasonicSensor &sensor, uint32_t nowUs) {
  if (sensor.phase == SensorPhase::Idle) {
    return;
  }

  const bool timedOut = static_cast<int32_t>(nowUs - sensor.deadlineUs) >= 0;

  if (sensor.phase == SensorPhase::WaitingForRise) {
    if (digitalRead(sensor.echoPin) == HIGH) {
      sensor.riseTimeUs = nowUs;
      sensor.phase = SensorPhase::WaitingForFall;
    } else if (timedOut) {
      sensor.phase = SensorPhase::Idle;
      sensor.valid = false;
    }
    return;
  }

  if (digitalRead(sensor.echoPin) == LOW) {
    const uint32_t pulseWidthUs = nowUs - sensor.riseTimeUs;
    const float measuredDistanceCm = pulseWidthUs / 58.0f;
    sensor.distanceCm = measuredDistanceCm;
    sensor.valid = measuredDistanceCm >= MIN_VALID_DISTANCE_CM &&
                   measuredDistanceCm <= MAX_VALID_DISTANCE_CM;
    sensor.phase = SensorPhase::Idle;
  } else if (timedOut) {
    sensor.phase = SensorPhase::Idle;
    sensor.valid = false;
  }
}

void calculateDiameter() {
  if (CONTINUOUS_FILM_SIMULATION) {
    const float phase = (millis() % SIMULATION_PERIOD_MS) *
                        (2.0f * PI / SIMULATION_PERIOD_MS);
    bubbleDiameterCm = TARGET_DIAMETER_CM + SIMULATION_AMPLITUDE_CM * sin(phase);
    diameterValid = true;
    return;
  }

  diameterValid = leftSensor.valid && rightSensor.valid;
  if (!diameterValid) {
    bubbleDiameterCm = NAN;
    return;
  }

  const float D = sensorSeparationCm;
  const float d1 = leftSensor.distanceCm;
  const float d2 = rightSensor.distanceCm;
  bubbleDiameterCm = D - (d1 + d2);
  diameterValid = bubbleDiameterCm >= 0.0f && bubbleDiameterCm <= D;
}

void updateIndicators() {
  inTolerance = diameterValid &&
                fabs(bubbleDiameterCm - TARGET_DIAMETER_CM) <= ALLOWED_TOLERANCE_CM;

  if (!diameterValid) {
    filmLeftServo.write(90);
    filmRightServo.write(90);
    return;
  }

  const int filmHalfAngle = constrain(
      static_cast<int>((bubbleDiameterCm - (TARGET_DIAMETER_CM - SIMULATION_AMPLITUDE_CM)) *
                           50.0f / (2.0f * SIMULATION_AMPLITUDE_CM) + 15.0f),
      15, 65);
  filmLeftServo.write(90 + filmHalfAngle);
  filmRightServo.write(90 - filmHalfAngle);
}

void serviceIndicators(uint32_t nowMs) {
  if (timeReached(nowMs, lastBlinkMs + LED_BLINK_INTERVAL_MS)) {
    lastBlinkMs = nowMs;
    indicatorState = !indicatorState;
  }

  const bool greenLedOn = diameterValid && inTolerance && indicatorState;
  const bool redLedOn = (!diameterValid || !inTolerance) && indicatorState;
  digitalWrite(GREEN_LED_PIN, greenLedOn ? HIGH : LOW);
  digitalWrite(RED_LED_PIN, redLedOn ? HIGH : LOW);
}

void updateDisplay() {
  lcd.setCursor(0, 0);
  lcd.print("Diameter:       ");
  lcd.setCursor(0, 1);

  if (!diameterValid) {
    lcd.print("Sensor fault     ");
    return;
  }

  lcd.print(bubbleDiameterCm, 1);
  lcd.print(" cm ");
  lcd.print(inTolerance ? "OK   " : "ALARM");
}

void serviceSensors() {
  const uint32_t nowUs = micros();
  serviceMeasurement(leftSensor, nowUs);
  serviceMeasurement(rightSensor, nowUs);

  static bool leftTriggered = false;
  static uint32_t lastTriggerUs = 0;

  if (leftSensor.phase == SensorPhase::Idle &&
      rightSensor.phase == SensorPhase::Idle &&
      static_cast<uint32_t>(nowUs - lastTriggerUs) >= SENSOR_SETTLE_US) {
    if (!leftTriggered) {
      startMeasurement(leftSensor);
      leftTriggered = true;
      lastTriggerUs = nowUs;
    } else {
      startMeasurement(rightSensor);
      leftTriggered = false;
      lastTriggerUs = nowUs;
    }
  }
}

void setup() {
  pinMode(TRIGGER_LEFT_PIN, OUTPUT);
  pinMode(ECHO_LEFT_PIN, INPUT);
  pinMode(TRIGGER_RIGHT_PIN, OUTPUT);
  pinMode(ECHO_RIGHT_PIN, INPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  filmLeftServo.attach(FILM_LEFT_SERVO_PIN);
  filmRightServo.attach(FILM_RIGHT_SERVO_PIN);

  digitalWrite(TRIGGER_LEFT_PIN, LOW);
  digitalWrite(TRIGGER_RIGHT_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bubble Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  nextMeasurementMs = millis() + MEASUREMENT_INTERVAL_MS;
}

void loop() {
  serviceSensors();

  const uint32_t nowMs = millis();
  if (timeReached(nowMs, nextMeasurementMs)) {
    nextMeasurementMs = nowMs + MEASUREMENT_INTERVAL_MS;
    calculateDiameter();
    updateIndicators();
  }

  if (timeReached(nowMs, lastDisplayMs + DISPLAY_INTERVAL_MS)) {
    lastDisplayMs = nowMs;
    updateDisplay();
  }

  serviceIndicators(nowMs);
}
