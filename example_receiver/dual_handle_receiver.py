import asyncio
import struct
import sys
from bleak import BleakScanner, BleakClient
from bleak.exc import BleakError

# --- Device Configurations ---
CONFIG = {
    "left": {
        "name": "AXE4_left",
        "uuids": {
            "angle": "beb5483e-36e1-4688-b7f5-ea07361b26a8",
            "quat":  "828917c1-ea55-4d4a-a66e-fd202cea0645",
            "joy":   "9c661337-b499-497d-aa5b-0105316e6e22"
        }
    },
    "right": {
        "name": "AXE4_right",
        "uuids": {
            "angle": "d1a68735-86b2-4d26-b8f2-1b633075c3f9",
            "quat":  "f3c83012-78d1-4e96-a14a-7bc991060932",
            "joy":   "2a8497d5-d852-4f01-90a6-16e51141bc25"
        }
    }
}

# --- Global State Dictionary ---
state = {
    "left":  {"connected": False, "roll": 0.0, "pitch": 0.0, "yaw": 0.0, "qw": 1.0, "qx": 0.0, "qy": 0.0, "qz": 0.0, "joy_x": 0.0, "joy_y": 0.0, "joy_z": 0.0, "sw": 0, "sw2": 0},
    "right": {"connected": False, "roll": 0.0, "pitch": 0.0, "yaw": 0.0, "qw": 1.0, "qx": 0.0, "qy": 0.0, "qz": 0.0, "joy_x": 0.0, "joy_y": 0.0, "joy_z": 0.0, "sw": 0, "sw2": 0}
}

# --- Notification Handlers (Using closures to know which side is updating) ---
def make_angle_handler(side):
    def handler(sender, data):
        state[side]["roll"], state[side]["pitch"], state[side]["yaw"] = struct.unpack('<3f', data)
    return handler

def make_quat_handler(side):
    def handler(sender, data):
        state[side]["qw"], state[side]["qx"], state[side]["qy"], state[side]["qz"] = struct.unpack('<4f', data)
    return handler

def make_joy_handler(side):
    def handler(sender, data):
        state[side]["joy_x"], state[side]["joy_y"], state[side]["joy_z"], state[side]["sw"], state[side]["sw2"] = struct.unpack('<3f2B', data)
    return handler

# --- Connection Manager Task ---
async def manage_device(side):
    """Handles scanning, connecting, and reconnecting for a single controller."""
    cfg = CONFIG[side]
    device_name = cfg["name"]
    uuids = cfg["uuids"]

    while True:
        try:
            # 1. Scan for the specific device
            state[side]["connected"] = False
            device = await BleakScanner.find_device_by_name(device_name, timeout=5.0)
            
            if not device:
                await asyncio.sleep(1.0) # Wait a bit before scanning again
                continue

            # 2. Setup Disconnect Event
            disconnected_event = asyncio.Event()
            
            def handle_disconnect(_):
                state[side]["connected"] = False
                disconnected_event.set()

            # 3. Connect and Subscribe
            async with BleakClient(device, disconnected_callback=handle_disconnect) as client:
                state[side]["connected"] = True
                
                await client.start_notify(uuids["angle"], make_angle_handler(side))
                await client.start_notify(uuids["quat"], make_quat_handler(side))
                await client.start_notify(uuids["joy"], make_joy_handler(side))
                
                # Wait here until the disconnect event is triggered
                await disconnected_event.wait()

        except BleakError as e:
            # Catch BLE errors (like connection failed) and just loop to try again
            await asyncio.sleep(2.0)
        except Exception as e:
            await asyncio.sleep(2.0)

# --- UI Printing Task ---
async def render_ui():
    """Prints the state of both controllers to the terminal at 20Hz."""
    print("\n" * 10) # Make room for the UI
    try:
        while True:
            # Move cursor UP 10 lines and clear to end of screen
            sys.stdout.write("\033[11A\033[J")
            
            for side in ["left", "right"]:
                s = state[side]
                name = CONFIG[side]["name"]
                status = "🟢 CONNECTED" if s["connected"] else "🔴 DISCONNECTED"
                
                sys.stdout.write(f"================= {name} ({status}) =================\n")
                if s["connected"]:
                    sys.stdout.write(f"[ANGLE] Roll: {s['roll']:7.2f} | Pitch: {s['pitch']:7.2f} | Yaw: {s['yaw']:7.2f}\n")
                    sys.stdout.write(f"[QUAT]  w: {s['qw']:6.4f} | x: {s['qx']:6.4f} | y: {s['qy']:6.4f} | z: {s['qz']:6.4f}\n")
                    sys.stdout.write(f"[JOY]   X: {s['joy_x']:7.2f} | Y: {s['joy_y']:7.2f} | Z: {s['joy_z']:7.2f} | SW1: {s['sw']} | SW2: {s['sw2']}\n")
                else:
                    sys.stdout.write("Searching for device...\n\n\n")
            
            sys.stdout.write("==============================================================\n")
            sys.stdout.write("Press Ctrl+C to exit.\n")
            sys.stdout.flush()
            
            await asyncio.sleep(0.05) # 20Hz refresh rate
    except asyncio.CancelledError:
        pass

# --- Main Entry Point ---
async def main():
    print("Starting Dual-Handle BLE Manager...")
    
    # Create background tasks for managing devices and UI
    task_left = asyncio.create_task(manage_device("left"))
    task_right = asyncio.create_task(manage_device("right"))
    task_ui = asyncio.create_task(render_ui())

    try:
        # Run everything together
        await asyncio.gather(task_left, task_right, task_ui)
    except KeyboardInterrupt:
        print("\n\nShutting down safely. Cleaning up tasks...")
        task_left.cancel()
        task_right.cancel()
        task_ui.cancel()
        await asyncio.sleep(0.5)
        print("Done.")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass # Catch the interrupt at the absolute top level to avoid tracebacks