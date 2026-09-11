# Scroll My Marbles

**Scroll My Marbles** is an alternative scrolling and button remapping solution for Linux pointing devices without a dedicated physical scroll wheel, specifically designed for the **Logitech TrackMan Marble FX** trackball (and compatible with other trackballs and mice).

It allows you to scroll vertically and horizontally across all applications by simply holding down a configurable modifier button (default: Middle Button) and rolling the trackball. If you click the button without moving the ball, it emulates a normal middle click.

Inspired by and translating the core logic of [TBScroll](https://github.com/spitfirex86/TBScroll) to Linux, **Scroll My Marbles** operates at the native kernel `evdev`/`uinput` layer, providing universal compatibility on both **Wayland** (GNOME, Zorin, KDE Plasma, Sway, etc.) and **X11** sessions without needing root privileges.

---

## Features

- **Universal Wayland & X11 Compatibility**: Intercepts physical raw events using `libevdev` exclusive grabbing (`EVIOCGRAB`) and outputs forwarded movements and wheel events through a virtual `/dev/uinput` device.
- **Fluid Scroll Emulation**:
  - Independent **Vertical** (`VSensitivity`) and **Horizontal** (`HSensitivity`) sensitivity thresholds.
  - **Reverse / Natural Scrolling** option.
  - **Smooth High-Resolution Scrolling**: Emits high-precision fractional wheel events (`REL_WHEEL_HI_RES` / `REL_HWHEEL_HI_RES`) on modern Linux desktops.
  - **Perpendicular Jitter Cancellation**: Automatically suppresses axis bleed in detent mode.
- **Click Emulation on Release**: Tapping the scroll button without moving the trackball emits a normal middle click (or a user-configured button click).
- **Auxiliary Button Remapping**: Map extra buttons (e.g. Button 4 / `BTN_SIDE` and Button 5 / `BTN_EXTRA`) to send Middle Click, Back, Forward, or disable them.
- **Hotplug & Auto-reconnect**: Gracefully recovers if the trackball is unplugged and reconnected.
- **Modern Settings GUI**: Built with **GTK4 + Libadwaita** adhering to GNOME desktop standards, featuring an interactive scroll testing area.
- **System Tray Integration**: Native DBus `StatusNotifierItem` and `com.canonical.dbusmenu` indicator providing Quick Settings, Device Status, About, and Exit.
- **Unprivileged Non-Root Execution**: Shipped with udev rules tagging devices with `uaccess`.
- **Diagnostics Test Mode**: Includes a terminal `--test` mode to inspect devices and verify scrolling logic directly from the console.

---

## Screenshots & Architecture

```
┌────────────────────────────────────────────────────────────────────────┐
│                        Scroll My Marbles                               │
│                                                                        │
│  ┌───────────────────────┐             ┌────────────────────────────┐  │
│  │   Settings Window     │             │      System Tray           │  │
│  │   (GTK4 + Libadwaita) │             │ (StatusNotifierItem/DBus)  │  │
│  └───────────┬───────────┘             └─────────────┬──────────────┘  │
│              │                                       │                 │
│              └───────────────────┬───────────────────┘                 │
│                                  │                                     │
│                     ┌────────────┴────────────┐                        │
│                     │  Config (GLib KeyFile)  │                        │
│                     │ ~/.config/scroll-my-... │                        │
│                     └────────────┬────────────┘                        │
│                                  │                                     │
│                     ┌────────────┴────────────┐                        │
│                     │  Core Worker Thread     │                        │
│                     │  (libevdev + uinput)    │                        │
│                     └──────┬────────────▲─────┘                        │
└────────────────────────────┼────────────┼──────────────────────────────┘
                             │            │
            Forwarded events │            │ Exclusive Grab (EVIOCGRAB)
            + Scroll Wheel   │            │
                             ▼            │
                     ┌──────────────┐  ┌──────────────────────┐
                     │ /dev/uinput  │  │  /dev/input/eventX   │
                     │   (Virtual   │  │ (Logitech TrackMan   │
                     │    Device)   │  │      Marble FX)      │
                     └──────────────┘  └──────────────────────┘
```

---

## Installation & Packaging

### Target Platform
- **OS**: Ubuntu 22.04 / 24.04, Debian 12+, Zorin OS 17/18, Fedora, or any modern Linux distribution.
- **Display Server**: Wayland or X11.

### 1. Install Build Dependencies
To compile the application or build the `.deb` package:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  meson \
  ninja-build \
  pkg-config \
  libglib2.0-dev \
  libevdev-dev \
  libgtk-4-dev \
  libadwaita-1-dev \
  debhelper
```

### 2. Build the Native `.deb` Package
From the root of the repository:

```bash
dpkg-buildpackage -us -uc -b
```

This generates `../scroll-my-marbles_1.0.0-1_<arch>.deb`.

### 3. Install the `.deb`
Install the package using `dpkg` or `apt`:

```bash
sudo dpkg -i ../scroll-my-marbles_1.0.0-1_*.deb
# If there are missing runtime dependencies:
sudo apt-get install -f
```

The installer:
1. Installs the binary to `/usr/bin/scroll-my-marbles`.
2. Installs the desktop launcher to `/usr/share/applications/scroll-my-marbles.desktop`.
3. Installs high-resolution icons to `/usr/share/icons/hicolor/`.
4. Installs the udev rule to `/lib/udev/rules.d/99-scroll-my-marbles.rules` and reloads udev rules automatically.

---

## Building Locally with Meson & Ninja

For development without generating a `.deb`:

```bash
# Configure build directory
meson setup build

# Compile
ninja -C build

# Run unit tests
ninja -C build test

# Install locally
sudo ninja -C build install
```

---

## Permissions & Udev Configuration

To allow running as a normal user without `sudo`, the udev rule (`/lib/udev/rules.d/99-scroll-my-marbles.rules`) grants access to `/dev/uinput` and the physical input devices:

```udev
KERNEL=="uinput", SUBSYSTEM=="misc", TAG+="uaccess", OPTIONS+="static_node=uinput"
SUBSYSTEM=="input", ATTRS{name}=="*TrackMan Marble*", TAG+="uaccess"
SUBSYSTEM=="input", ATTRS{name}=="*Marble FX*", TAG+="uaccess"
SUBSYSTEM=="input", ATTRS{name}=="*Logitech TrackMan*", TAG+="uaccess"
SUBSYSTEM=="input", ATTRS{name}=="*Trackball*", TAG+="uaccess"
SUBSYSTEM=="input", ENV{ID_INPUT_TRACKBALL}=="1", TAG+="uaccess"
```

If you installed manually without the `.deb`:
```bash
sudo cp data/99-scroll-my-marbles.rules /lib/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Alternatively, add your user to the `input` group:
```bash
sudo usermod -a -G input $USER
```

---

## Command Line Usage

```
Usage: scroll-my-marbles [OPTIONS]

Options:
  --tray        Start minimized in the system tray (used for autostart)
  --settings    Open the settings window on startup
  --test        Run interactive diagnostics test mode in terminal
  --device PATH Override device with specific /dev/input/event* path
  --version     Display application version
  --help        Display this help message
```

### Diagnostics & Test Mode
Run with `--test` to verify event capture, ball movement, and scroll calculations in your terminal:
```bash
scroll-my-marbles --test
```

---

## Configuration File

Settings are stored in INI format at:
`~/.config/scroll-my-marbles/config.ini`

Example configuration:
```ini
[General]
DeviceName=Logitech TrackMan Marble FX
DevicePath=
VSensitivity=20
HSensitivity=120
ReverseScroll=false
SmoothScroll=false
ScrollButton=BTN_MIDDLE
EmulateClick=true
EmulatedClickButton=BTN_MIDDLE
Autostart=true

[Buttons]
Button4Action=MiddleClick
Button5Action=PassThrough
Button3Action=ScrollModifier
Button2Action=PassThrough
```

### Supported Button Action Values
- `PassThrough`: Forwards the physical button untouched.
- `ScrollModifier`: Uses this button as the hold-to-scroll modifier.
- `MiddleClick`: Emulates middle click (`BTN_MIDDLE`) upon press.
- `LeftClick`: Emulates left click (`BTN_LEFT`).
- `RightClick`: Emulates right click (`BTN_RIGHT`).
- `Back`: Emulates browser/file-manager back (`BTN_SIDE`).
- `Forward`: Emulates browser/file-manager forward (`BTN_EXTRA`).
- `Disabled`: Suppresses the button entirely.

---

## License

MIT License. Copyright (c) 2026 businessgaberino-commits.  
Original TBScroll Windows logic copyright (c) 2021 Spitfire_x86.