import asyncio
import struct
from bleak import BleakScanner, BleakClient

# --- Match these exactly to your ESP32 C++ code ---
DEVICE_NAME = "ESP32_Controller_1"
CHAR_UUID_ANGLE = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
CHAR_UUID_QUAT  = "828917c1-ea55-4d4a-a66e-fd202cea0645"

# --- Notification Handlers ---
# These functions trigger automatically the millisecond new data arrives
def angle_notification_handler(sender, data):
    # Unpack 12 bytes into 3 little-endian floats ('<3f')
    roll, pitch, yaw = struct.unpack('<3f', data)
    print(f"[ANGLE] Roll: {roll:7.2f} | Pitch: {pitch:7.2f} | Yaw: {yaw:7.2f}")

def quat_notification_handler(sender, data):
    # Unpack 16 bytes into 4 little-endian floats ('<4f')
    w, x, y, z = struct.unpack('<4f', data)
    print(f"[QUAT]  w: {w:6.4f} | x: {x:6.4f} | y: {y:6.4f} | z: {z:6.4f}")

async def main():
    print(f"Scanning for {DEVICE_NAME}...")
    
    # 1. Find the ESP32
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
    
    if not device:
        print(f"Could not find {DEVICE_NAME}. Make sure it is powered on and advertising.")
        return

    print(f"Found {DEVICE_NAME} at {device.address}! Connecting...")

    # 2. Connect and subscribe to the data streams
    async with BleakClient(device) as client:
        print("Connected! Subscribing to characteristics...\n")
        
        await client.start_notify(CHAR_UUID_ANGLE, angle_notification_handler)
        await client.start_notify(CHAR_UUID_QUAT, quat_notification_handler)
        
        print("--- Receiving Data. Press Ctrl+C to stop. ---\n")
        
        # Keep the script running infinitely while data streams in
        try:
            while True:
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            print("\nDisconnecting...")
            await client.stop_notify(CHAR_UUID_ANGLE)
            await client.stop_notify(CHAR_UUID_QUAT)
            print("Disconnected safely.")

if __name__ == "__main__":
    asyncio.run(main())