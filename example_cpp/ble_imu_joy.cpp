#include <Wire.h>
#include <JY901.h>
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- BLE UUIDs ---
#define SERVICE_UUID              "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_ANGLE "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_QUAT  "828917c1-ea55-4d4a-a66e-fd202cea0645"
// New UUID for Joystick Data
#define CHARACTERISTIC_UUID_JOY   "9c661337-b499-497d-aa5b-0105316e6e22" 

BLEServer* pServer = NULL;
BLECharacteristic* pCharAngle = NULL;
BLECharacteristic* pCharQuat = NULL;
BLECharacteristic* pCharJoy = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;

// --- Joystick Configuration ---
const int PIN_Y = 0;
const int PIN_SW = 1;
const int PIN_X = 3;
const int ADC_MAX = 4095;
const int CALIBRATION_SAMPLES = 50;
const float DEADZONE = 0.08;

int centerX = 2048;
int centerY = 2048;

// --- Data Structures ---
struct ImuAngles {
  float roll;
  float pitch;
  float yaw;
} __attribute__((packed));

struct ImuQuat {
  float w;
  float x;
  float y;
  float z;
} __attribute__((packed));

// New struct for Joystick
struct JoystickData {
  float x;
  float y;
  uint8_t sw; // 1 for pressed, 0 for unpressed
} __attribute__((packed));

ImuAngles currentAngles;
ImuQuat currentQuat;
JoystickData currentJoy;

// --- BLE Callbacks ---
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Device Connected!");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Device Disconnected!");
    }
};

// --- Joystick Helper Functions ---
void calibrateJoystick() {
  long sumX = 0;
  long sumY = 0;

  Serial.println("Calibrating joystick... Do not touch.");
  
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

  if (rawValue >= centerValue) {
    normalized = (float)(rawValue - centerValue) / (ADC_MAX - centerValue);
  } else {
    normalized = (float)(rawValue - centerValue) / centerValue;
  }

  normalized = constrain(normalized, -1.0, 1.0);

  if (abs(normalized) < DEADZONE) {
    return 0.0;
  } else if (normalized > 0) {
    return (normalized - DEADZONE) / (1.0 - DEADZONE);
  } else {
    return (normalized + DEADZONE) / (1.0 - DEADZONE);
  }
}

void setup() 
{
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println("\n--- USB CONNECTED ---");

  // 1. Initialize Joystick Pins & Calibrate
  pinMode(PIN_SW, INPUT_PULLUP);
  analogReadResolution(12); 
  calibrateJoystick();

  // 2. Initialize I2C for IMU
  Serial.println("Initializing I2C on pins SDA:8, SCL:9...");
  Wire.begin(9, 8); 
  
  // 3. Initialize BLE
  Serial.println("Initializing BLE Server...");
  BLEDevice::init("ESP32_Controller_1"); 
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Angle Characteristic
  pCharAngle = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_ANGLE,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharAngle->addDescriptor(new BLE2902());

  // Quaternion Characteristic
  pCharQuat = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_QUAT,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharQuat->addDescriptor(new BLE2902());

  // Joystick Characteristic
  pCharJoy = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_JOY,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharJoy->addDescriptor(new BLE2902());

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  
  BLEDevice::startAdvertising();
  
  Serial.println("BLE Ready. Waiting for connection...");
}

void loop() 
{
  // 1. Poll IMU hardware
  JY901.GetAngle();
  JY901.GetQuaternion();

  // 2. Poll Joystick hardware
  int rawX = analogRead(PIN_X);
  int rawY = analogRead(PIN_Y);
  int swState = !digitalRead(PIN_SW);

  // 3. Blast data if connected
  if (deviceConnected) {
    // Populate Angles
    currentAngles.roll  = (float)JY901.stcAngle.Angle[0] / 32768.0 * 180.0;
    currentAngles.pitch = (float)JY901.stcAngle.Angle[1] / 32768.0 * 180.0;
    currentAngles.yaw   = (float)JY901.stcAngle.Angle[2] / 32768.0 * 180.0;

    // Populate Quaternions
    currentQuat.w = (float)JY901.stcQuater.q0 / 32768.0;
    currentQuat.x = (float)JY901.stcQuater.q1 / 32768.0;
    currentQuat.y = (float)JY901.stcQuater.q2 / 32768.0;
    currentQuat.z = (float)JY901.stcQuater.q3 / 32768.0;

    // Populate Joystick
    currentJoy.x = normalizeAxis(rawX, centerX);
    currentJoy.y = normalizeAxis(rawY, centerY);
    currentJoy.sw = (uint8_t)swState;

    // Push raw bytes over BLE
    pCharAngle->setValue((uint8_t*)&currentAngles, sizeof(ImuAngles));
    pCharAngle->notify();

    pCharQuat->setValue((uint8_t*)&currentQuat, sizeof(ImuQuat));
    pCharQuat->notify();

    pCharJoy->setValue((uint8_t*)&currentJoy, sizeof(JoystickData));
    pCharJoy->notify();

    delay(20); // ~50Hz
  }
  else {
    // Print for debugging when disconnected
    Serial.printf("Waiting... Yaw: %5.2f | Joy X: %5.2f | Joy Y: %5.2f | SW: %d\n", 
                  (float)JY901.stcAngle.Angle[2] / 32768.0 * 180.0,
                  normalizeAxis(rawX, centerX),
                  normalizeAxis(rawY, centerY),
                  swState);
    delay(500);
  }

  // --- Reconnection Logic ---
  if (!deviceConnected && oldDeviceConnected) {
      delay(500); 
      pServer->startAdvertising(); 
      Serial.println("Restarted advertising");
      oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
  }
}