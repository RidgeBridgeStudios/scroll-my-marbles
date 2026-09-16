# Scroll My Marbles

**Scroll My Marbles** is an alternative scrolling and button remapping solution for Linux pointing devices without a dedicated physical scroll wheel, specifically designed for the **Logitech TrackMan Marble T-BC21** ("Marble Mouse", 4-button) and **Logitech TrackMan Marble FX** trackballs (and compatible with other trackballs and mice).

It allows you to scroll vertically and horizontally across all applications by simply holding down a configurable modifier button (default: Small Left side button / `BTN_SIDE` on T-BC21) and rolling the trackball. If you click the button without moving the ball, it emulates a normal middle click.

Inspired by and translating the core logic of [TBScroll](https://github.com/spitfirex86/TBScroll) to Linux, **Scroll My Marbles** operates at the native kernel `evdev`/`uinput` layer, providing universal compatibility on both **Wayland** (GNOME, Zorin, KDE Plasma, Sway, etc.) and **X11** sessions without needing root privileges.

---

## Supported Devices

| Device | USB VID:PID | Default Modifier | Notes |
| :--- | :--- | :--- | :--- |
| **Logitech TrackMan Marble T-BC21** ("Marble Mouse", 4-button) | `046d:c408` | `BTN_SIDE` (Small Left) | Primary target. Kernel name: "Logitech USB Trackball" |
| **Logitech TrackMan Marble FX** | `046d:c401` | `BTN_MIDDLE` | Supported |
| **Generic USB Trackball** | Any | `BTN_SIDE` / `BTN_MIDDLE` | Fallback match for input devices with trackball attributes |

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

## 🚀 Quick Install (Beginner Friendly)

No compiling or command-line experience required! Choose the method that suits you best:

### ⚡ Option 1: 1-Line Automatic Install (Recommended & Easiest)
Open your terminal (press `Ctrl` + `Alt` + `T`) and paste:

```bash
curl -fsSL https://raw.githubusercontent.com/RidgeBridgeStudios/scroll-my-marbles/main/install.sh | bash
```

> **What this does automatically:**
> 1. Detects your Linux distribution and downloads the latest official `.deb` package.
> 2. Installs required system libraries and the application binary (`/usr/bin/scroll-my-marbles`).
> 3. Configures `udev` hardware permission rules so you can run without root/sudo.
> 4. Adds **Scroll My Marbles** with high-resolution icons to your application launcher.

---

### 📦 Option 2: Download & Install the `.deb` Package (Ubuntu, Debian, Linux Mint, Zorin OS, Pop!_OS)

1. Open the **[GitHub Releases Tab](https://github.com/RidgeBridgeStudios/scroll-my-marbles/releases)**.
2. Under **Assets**, click to download **`scroll-my-marbles_1.2.1_amd64.deb`**.
3. Install it using either method:
   - **Graphical**: Double-click the downloaded `.deb` file to open it in your Software Center / App Center, then click **Install**.
   - **Terminal**: Open the folder where the file downloaded (e.g. `Downloads`) and run:
     ```bash
     sudo apt install ./scroll-my-marbles_*_amd64.deb
     ```
4. Done! You will find **Scroll My Marbles** in your applications menu.

---

### 🖱️ First-Time Use & Beginner Tips

1. **How to Scroll**:
   - **Hold down the Small Left side button** (`BTN_SIDE`, default on T-BC21) and **roll the trackball** to scroll vertically or horizontally.
   - You can also choose the **Small Right side button** (`BTN_EXTRA`) or chord **Both Side Buttons** (`ScrollModifierMode=ChordBothSideButtons`) in the Settings window.
   - **Click & release without rolling** sends an emulated middle click (e.g., to open a link in a new browser tab or close a tab).
2. **Device Detection & Kernel Name**:
   - The Logitech TrackMan Marble T-BC21 identifies over USB with VID:PID `046d:c408`. In the Linux kernel, its device name is `"Logitech USB Trackball"` (or `"USB Trackball"`). Scroll My Marbles automatically detects and binds to this device out of the box using its USB VID/PID.
3. **Device Permission Setup (Important)**:
   - The installer automatically configures hardware rules (`/lib/udev/rules.d/70-scroll-my-marbles.rules`).
   - If scrolling doesn't respond on your very first run, simply **unplug and reconnect your trackball** (or log out and back in) so Linux refreshes its device permissions.
4. **Tray Icon & Autostart on Login**:
   - Scroll My Marbles runs in your system tray / notification area.
   - Click the tray icon to open **Settings** and toggle **"Autostart on Login"** so scrolling is always active whenever you boot your computer.

---

### 🗑️ How to Uninstall
To remove Scroll My Marbles at any time:
```bash
# Using the installer script:
curl -fsSL https://raw.githubusercontent.com/RidgeBridgeStudios/scroll-my-marbles/main/install.sh | bash -s -- --uninstall

# Or using apt:
sudo apt remove scroll-my-marbles
```

---

## 🛠️ Building from Source (Developers & Advanced Users)

If you are developing, customizing the code, or on a non-Debian distribution:

### 1. Install Build Dependencies
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
  debhelper \
  fakeroot
```

### 2. Build and Package with `./build.sh`
Run the automated build script to compile, run tests, and produce packages:
```bash
./build.sh
```
This generates:
- `scroll-my-marbles_1.2.1_amd64.deb`
- `scroll-my-marbles-1.2.1-linux-x86_64.tar.gz`
- `SHA256SUMS.txt`

### 3. Manual Build with Meson & Ninja
```bash
meson setup build --prefix=/usr
ninja -C build
ninja -C build test
sudo ninja -C build install
```

---

## Permissions & Udev Configuration

To allow running as a normal user without `sudo`, the udev rule (`/lib/udev/rules.d/70-scroll-my-marbles.rules`) grants access to `/dev/uinput` and the physical input devices:

```udev
KERNEL=="uinput", SUBSYSTEM=="misc", TAG+="uaccess", OPTIONS+="static_node=uinput"

# Logitech TrackMan Marble T-BC21 ("Marble Mouse", 4-button) USB VID:PID
KERNEL=="event[0-9]*", SUBSYSTEM=="input", ATTRS{idVendor}=="046d", ATTRS{idProduct}=="c408", TAG+="uaccess"
KERNEL=="event[0-9]*", SUBSYSTEM=="input", ATTRS{id/vendor}=="046d", ATTRS{id/product}=="c408", TAG+="uaccess"

# Logitech TrackMan Marble FX USB VID:PID
KERNEL=="event[0-9]*", SUBSYSTEM=="input", ATTRS{idVendor}=="046d", ATTRS{idProduct}=="c401", TAG+="uaccess"
KERNEL=="event[0-9]*", SUBSYSTEM=="input", ATTRS{id/vendor}=="046d", ATTRS{id/product}=="c401", TAG+="uaccess"

# Fallback name and attribute matching
KERNEL=="event[0-9]*", SUBSYSTEM=="input", ENV{ID_INPUT_TRACKBALL}=="1", TAG+="uaccess"
KERNEL=="event[0-9]*", SUBSYSTEM=="input", ATTRS{name}=="*USB Trackball*", TAG+="uaccess"
KERNEL=="event[0-9]*", SUBSYSTEM=="input", ATTRS{name}=="*TrackMan*", TAG+="uaccess"
KERNEL=="event[0-9]*", SUBSYSTEM=="input", ATTRS{name}=="*Marble*", TAG+="uaccess"
```

If you installed manually from source without the package:
```bash
sudo cp data/70-scroll-my-marbles.rules /lib/udev/rules.d/
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
DeviceName=Logitech USB Trackball
DevicePath=
VSensitivity=50
HSensitivity=200
ReverseScroll=false
SmoothScroll=false
ScrollButton=BTN_SIDE
ScrollModifierMode=SingleButton
EmulateClick=true
EmulatedClickButton=BTN_MIDDLE
Autostart=true

[Buttons]
Button4Action=ScrollModifier
Button5Action=ScrollModifier
Button3Action=PassThrough
Button2Action=PassThrough
```

### Configuration Options
- `ScrollModifierMode`:
  - `SingleButton`: Hold down `ScrollButton` (e.g. `BTN_SIDE` or `BTN_EXTRA`) to scroll.
  - `ChordBothSideButtons`: Press both small side buttons (`BTN_SIDE` and `BTN_EXTRA`) simultaneously to scroll.
- `ScrollButton`: The modifier button code when `ScrollModifierMode` is set to `SingleButton` (defaults to `BTN_SIDE`).
- `EmulateClick`: If true, releasing the scroll modifier without moving the ball emits a click of `EmulatedClickButton`.
- `EmulatedClickButton`: Button code to emit on release without movement (defaults to `BTN_MIDDLE`).
- `VSensitivity` / `HSensitivity`: Accumulated motion thresholds before emitting a vertical / horizontal scroll detent (defaults to 50 / 200).

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

MIT License. Copyright (c) 2026 RidgeBridgeStudios.  
Original TBScroll Windows logic copyright (c) 2021 Spitfire_x86.