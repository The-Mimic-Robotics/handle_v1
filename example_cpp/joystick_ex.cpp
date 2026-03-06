#include <Arduino.h>

// Pin definitions
const int PIN_Y = 0;
const int PIN_SW = 1;
const int PIN_X = 3;

// Joystick configuration
const int ADC_MAX = 4095;
const int CALIBRATION_SAMPLES = 50;
const float DEADZONE = 0.08; // 8% deadzone

// Calibration variables
int centerX = 2048;
int centerY = 2048;

void calibrateJoystick() {
  long sumX = 0;
  long sumY = 0;

  Serial.println("Calibrating joystick... Do not touch.");
  
  // Take multiple readings to average out the resting position
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    sumX += analogRead(PIN_X);
    sumY += analogRead(PIN_Y);
    delay(10);
  }

  centerX = sumX / CALIBRATION_SAMPLES;
  centerY = sumY / CALIBRATION_SAMPLES;

  Serial.printf("Calibration complete. Center X: %d | Center Y: %d\n", centerX, centerY);
}

float normalizeAxis(int rawValue, int centerValue) {
  float normalized = 0.0;

  // Map from the calibrated center to the edges. 
  // We split this into two halves because the resting center is rarely exactly 2048.
  if (rawValue >= centerValue) {
    normalized = (float)(rawValue - centerValue) / (ADC_MAX - centerValue);
  } else {
    normalized = (float)(rawValue - centerValue) / centerValue;
  }

  // Constrain to catch any ADC noise pushing slightly past bounds
  normalized = constrain(normalized, -1.0, 1.0);

  // Apply a "smooth" deadzone
  if (abs(normalized) < DEADZONE) {
    return 0.0;
  } else if (normalized > 0) {
    // Rescale the remaining range from 0.0 to 1.0
    return (normalized - DEADZONE) / (1.0 - DEADZONE);
  } else {
    // Rescale the remaining range from -1.0 to 0.0
    return (normalized + DEADZONE) / (1.0 - DEADZONE);
  }
}

void setup() {
  Serial.begin(115200);

  // Configure pins
  pinMode(PIN_SW, INPUT_PULLUP);
  
  // Explicitly set ADC resolution to 12-bit (0-4095) for ESP32
  analogReadResolution(12); 

  delay(2000); // Give serial monitor time to connect before calibrating

  calibrateJoystick();
}

void loop() {
  int rawX = analogRead(PIN_X);
  int rawY = analogRead(PIN_Y);

  // Read switch: INPUT_PULLUP means normally HIGH (1). 
  // We invert it (!) so pressed = 1, unpressed = 0.
  int swState = !digitalRead(PIN_SW);

  float normX = normalizeAxis(rawX, centerX);
  float normY = normalizeAxis(rawY, centerY);

  // Print formatted output
  Serial.printf("X: %5.2f | Y: %5.2f | SW: %d\n", normX, normY, swState);

  delay(50); // Small delay to prevent serial flooding
}