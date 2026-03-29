import asyncio
import struct
import sys
from bleak import BleakScanner, BleakClient

# --- Match these exactly to your ESP32 C++ code ---
DEVICE_NAME = "AXE3_left"  #AXE3_right, or AXE3_left
CHAR_UUID_ANGLE = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
CHAR_UUID_QUAT  = "828917c1-ea55-4d4a-a66e-fd202cea0645"
CHAR_UUID_JOY   = "9c661337-b499-497d-aa5b-0105316e6e22"

# --- Global State Dictionary ---
state = {
    "roll": 0.0, "pitch": 0.0, "yaw": 0.0,
    "qw": 1.0, "qx": 0.0, "qy": 0.0, "qz": 0.0,
    "joy_x": 0.0, "joy_y": 0.0, "joy_z": 0.0, "sw": 0, "sw2": 0
}

# --- Notification Handlers ---
def angle_notification_handler(sender, data):
    state["roll"], state["pitch"], state["yaw"] = struct.unpack('<3f', data)

def quat_notification_handler(sender, data):
    state["qw"], state["qx"], state["qy"], state["qz"] = struct.unpack('<4f', data)

def joy_notification_handler(sender, data):
    # Unpack 14 bytes: 3 little-endian floats and 2 unsigned chars ('<3f2B')
    state["joy_x"], state["joy_y"], state["joy_z"], state["sw"], state["sw2"] = struct.unpack('<3f2B', data)

async def main():
    print(f"Scanning for {DEVICE_NAME}...")
    
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
    
    if not device:
        print(f"Could not find {DEVICE_NAME}. Make sure it is powered on and advertising.")
        return

    print(f"Found {DEVICE_NAME} at {device.address}! Connecting...")

    async with BleakClient(device) as client:
        print("Connected! Subscribing to characteristics...\n")
        
        await client.start_notify(CHAR_UUID_ANGLE, angle_notification_handler)
        await client.start_notify(CHAR_UUID_QUAT, quat_notification_handler)
        await client.start_notify(CHAR_UUID_JOY, joy_notification_handler)
        
        print("--- Receiving Data. Press Ctrl+C to stop. ---\n")
        
        # Print 5 empty lines to make room for our table
        print("\n" * 5)
        
        try:
            while True:
                # Move cursor UP 6 lines (\033[6A) and clear to end of screen (\033[J)
                sys.stdout.write("\033[6A\033[J")
                
                # Draw the table
                sys.stdout.write("======================== TELEOP STATE ========================\n")
                sys.stdout.write(f"[ANGLE] Roll: {state['roll']:7.2f} | Pitch: {state['pitch']:7.2f} | Yaw: {state['yaw']:7.2f}\n")
                sys.stdout.write(f"[QUAT]  w: {state['qw']:6.4f} | x: {state['qx']:6.4f} | y: {state['qy']:6.4f} | z: {state['qz']:6.4f}\n")
                sys.stdout.write(f"[JOY]   X: {state['joy_x']:7.2f} | Y: {state['joy_y']:7.2f} | Z: {state['joy_z']:7.2f} | SW1: {state['sw']} | SW2: {state['sw2']}\n")
                sys.stdout.write("==============================================================\n")
                sys.stdout.flush()
                
                # Update the UI at 20Hz (every 50ms)
                await asyncio.sleep(0.05)
                
        except KeyboardInterrupt:
            print("\n\nDisconnecting...")
            await client.stop_notify(CHAR_UUID_ANGLE)
            await client.stop_notify(CHAR_UUID_QUAT)
            await client.stop_notify(CHAR_UUID_JOY)
            print("Disconnected safely.")

if __name__ == "__main__":
    asyncio.run(main())