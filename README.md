# Xbox Controller Emulator (Linux)

A lightweight Linux daemon that turns your keyboard and mouse into a virtual Xbox gamepad, using the kernel's `uinput` subsystem. No extra drivers, no user-space hacks — the virtual device shows up to games and applications exactly like a real Xbox controller would.

## How It Works

The daemon reads raw input events directly from your keyboard and mouse device nodes (`/dev/input/eventX`), then translates them into gamepad events on a virtual `uinput` device:

- **Keyboard → Buttons & D-Pad / Left Stick**: `WASD` drives the left analog stick, and mapped keys trigger face buttons.
- **Mouse movement → Right Stick**: mouse deltas are translated into `ABS_RX` / `ABS_RY` events, simulating the right analog stick.
- **Mouse buttons → Triggers**: left/right click map to the analog triggers (`BTN_TL2` / `BTN_TR2`).

Instead of busy-waiting on either device, the daemon uses `poll()` to block until either the keyboard or mouse actually has new input, so it stays idle when nothing is happening rather than spinning the CPU.

## Key Mapping

| Input | Gamepad Output |
|---|---|
| `W` / `A` / `S` / `D` | Left analog stick |
| Mouse movement | Right analog stick |
| `Enter` / `Space` | A button |
| `G` | Y button |
| `E` | X button |
| `C` | B button |
| `Left Shift` | Left stick click (L3) |
| `Right Shift` | Right stick click (R3) |
| `Left Ctrl` | Recenter right stick |
| Mouse Left Click | Left trigger (LT) |
| Mouse Right Click | Right trigger (RT) |
| Mouse Middle Click | Recenter right stick |
| `Q` then `Enter` | Quit the daemon |

## Prerequisites

- Linux with the `uinput` kernel module available (loaded via `modprobe uinput` if not already active)
- Root privileges (writing to `/dev/uinput` and reading raw `/dev/input/eventX` nodes requires it)
- A C/C++ compiler (`gcc` or `g++`)

## Setup

1. **Find your keyboard and mouse event nodes.**
   ```bash
   ls -l /dev/input/by-id/
   # or
   cat /proc/bus/input/devices
   ```
   Look for entries like `eventX` corresponding to your keyboard and mouse.

2. **Update the device paths** in `emulator.cpp` to match your system:
   ```cpp
   char KEYBOARD_PATH[] = "/dev/input/event0"; // update to your keyboard's event node
   char MOUSE_PATH[]    = "/dev/input/event1"; // update to your mouse's event node
   ```

3. **Build:**
   ```bash
   g++ emulator.cpp -o emulator
   ```

4. **Run (requires root):**
   ```bash
   sudo ./emulator
   ```

5. Once running, any application expecting an Xbox controller (Steam, emulators, native Linux games with gamepad support) will detect **"Virtual Xbox Gamepad Daemon"** as a connected device.

## Stopping the Daemon

- `Ctrl+C`, or
- Press `Q` then `Enter`

Either path triggers a clean shutdown: the virtual `uinput` device is destroyed and both input file descriptors are closed via a signal-safe cleanup routine.

## Known Limitations

- Device paths (`/dev/input/eventX`) are hardcoded and can change across reboots — you may need to re-check and update them periodically, or symlink via `/dev/input/by-id/`.
- Requires root to access raw input devices and `/dev/uinput`.
- Currently supports one fixed keyboard/mouse layout; no runtime remapping yet.

## Contributing

Issues and PRs are welcome — this started as a personal project to get hands-on with Linux input internals, so there's plenty of room to extend (configurable bindings, multiple controller profiles, a udev rule for non-root access, etc.).

## License

MIT
