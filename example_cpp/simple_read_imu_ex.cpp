#include <Wire.h>
#include <JY901.h>
#include <arduino.h>
/*
Test on Uno R3.
JY901    UnoR3
SDA <---> SDA
SCL <---> SCL
*/

void setup() 
{
  Serial.begin(115200);

  // 1. Trap the code here until the Serial Monitor is actually open
  while (!Serial) {
    delay(10); 
  }

  // If you see this, the USB is working perfectly!
  Serial.println("\n--- USB CONNECTED ---");
  Serial.println("ESP32-C3 is alive.");
  delay(500);

  Serial.println("Initializing I2C on pins SDA:6, SCL:7...");
  
  // 2. Force the XIAO's exact pins. 
  // DO NOT use JY901.StartIIC(); it defaults to the wrong pins!
  Wire.begin(8, 9); // SDA = GPIO8, SCL = GPIO9
  
  Serial.println("I2C Hardware Initialized. Entering Loop...");
}


void loop() 
{
  // 1. Request the latest data from the sensor
  JY901.GetAngle();
  JY901.GetQuaternion();

  // 2. Calculate and print Angles (Roll, Pitch, Yaw)
  // Formula: Raw Data / 32768.0 * 180.0
  Serial.print("Angle (Roll, Pitch, Yaw): ");
  Serial.print((float)JY901.stcAngle.Angle[0] / 32768.0 * 180.0, 2); 
  Serial.print(", ");
  Serial.print((float)JY901.stcAngle.Angle[1] / 32768.0 * 180.0, 2); 
  Serial.print(", ");
  Serial.println((float)JY901.stcAngle.Angle[2] / 32768.0 * 180.0, 2); 

  // 3. Calculate and print Quaternions (w, x, y, z)
  // Formula: Raw Data / 32768.0
  Serial.print("Quaternion (w, x, y, z):  ");
  Serial.print((float)JY901.stcQuater.q0 / 32768.0, 4);
  Serial.print(", ");
  Serial.print((float)JY901.stcQuater.q1 / 32768.0, 4);
  Serial.print(", ");
  Serial.print((float)JY901.stcQuater.q2 / 32768.0, 4);
  Serial.print(", ");
  Serial.println((float)JY901.stcQuater.q3 / 32768.0, 4);
  
  Serial.println("-----------------------------------------");
  
  // Wait a bit before polling again so it doesn't flood the terminal
  delay(100); 
}