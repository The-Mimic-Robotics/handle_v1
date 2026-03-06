#include <Wire.h>
#include <JY901.h>
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- BLE UUIDs ---
// You will need these exact UUIDs on the Jetson side to connect
#define SERVICE_UUID              "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_ANGLE "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_QUAT  "828917c1-ea55-4d4a-a66e-fd202cea0645"

BLEServer* pServer = NULL;
BLECharacteristic* pCharAngle = NULL;
BLECharacteristic* pCharQuat = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// --- Data Structures ---
// These pack the floats directly into continuous memory (bytes)
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

ImuAngles currentAngles;
ImuQuat currentQuat;

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

void setup() 
{
  Serial.begin(115200);

  // 1. Wait for Serial
  while (!Serial) { delay(10); }
  Serial.println("\n--- USB CONNECTED ---");

  // 2. Initialize I2C
  Serial.println("Initializing I2C on pins SDA:8, SCL:9...");
  Wire.begin(9, 8); 
  
  // 3. Initialize BLE
  Serial.println("Initializing BLE Server...");
  BLEDevice::init("ESP32_Controller_1"); // This is the name the Jetson will see
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create Angle Characteristic
  pCharAngle = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_ANGLE,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharAngle->addDescriptor(new BLE2902());

  // Create Quaternion Characteristic
  pCharQuat = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_QUAT,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharQuat->addDescriptor(new BLE2902());

  // Start the service and advertising
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  
  BLEDevice::startAdvertising();
  
  Serial.println("BLE Ready. Waiting for Jetson to connect...");
}

void loop() 
{
  // 1. Always poll the hardware
  JY901.GetAngle();
  JY901.GetQuaternion();

  // 2. If a device is connected, blast the data!
  if (deviceConnected) {
    // Populate the Angle struct
    currentAngles.roll  = (float)JY901.stcAngle.Angle[0] / 32768.0 * 180.0;
    currentAngles.pitch = (float)JY901.stcAngle.Angle[1] / 32768.0 * 180.0;
    currentAngles.yaw   = (float)JY901.stcAngle.Angle[2] / 32768.0 * 180.0;

    // Populate the Quaternion struct
    currentQuat.w = (float)JY901.stcQuater.q0 / 32768.0;
    currentQuat.x = (float)JY901.stcQuater.q1 / 32768.0;
    currentQuat.y = (float)JY901.stcQuater.q2 / 32768.0;
    currentQuat.z = (float)JY901.stcQuater.q3 / 32768.0;

    // Push the raw bytes over BLE
    pCharAngle->setValue((uint8_t*)&currentAngles, sizeof(ImuAngles));
    pCharAngle->notify();

    pCharQuat->setValue((uint8_t*)&currentQuat, sizeof(ImuQuat));
    pCharQuat->notify();

    // 20ms delay gives roughly 50Hz update rate, which is very stable for BLE
    delay(20); 
  }
  else {
    // Print to serial monitor just for debugging when not connected to Jetson
    Serial.print("Waiting for connection... Yaw: ");
    Serial.println((float)JY901.stcAngle.Angle[2] / 32768.0 * 180.0, 2); 
    delay(500);
  }

  // --- Reconnection Logic ---
  // If connection drops, start advertising again
  if (!deviceConnected && oldDeviceConnected) {
      delay(500); 
      pServer->startAdvertising(); 
      Serial.println("Restarted advertising");
      oldDeviceConnected = deviceConnected;
  }
  // If newly connected, update state
  if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
  }
}