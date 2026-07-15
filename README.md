# OpenRCP

OpenRCP is open-source firmware for a hardware remote control panel (RCP) that
shades Blackmagic Design cameras through an SDI camera-control shield. It runs
on a custom PCB built around a Waveshare ESP32-P4 module, presents the camera
state on its display, reads physical controls, drives a motorized iris fader,
and reports preview/program tally state.

> [!NOTE]
> All code in this repository was generated with AI. The main purpose of
> OpenRCP was to explore what could be built this way, learn from the process,
> and create a working proof of concept. It should therefore be treated as an
> experimental learning project, not as production-ready camera-control
> software. The background and development process are described in the
> [OpenRCP project article](https://jonasvanoyenbrugge.be/#/project/openrcp).

> [!IMPORTANT]
> This repository contains firmware for a custom hardware prototype, not a
> universal Blackmagic camera controller. Check the pin assignments, supply
> voltages, motor driver, and I2C devices against your own hardware before
> powering it. Incorrect wiring can damage the controller or attached devices.

## Current capabilities

- Select and maintain state for up to eight cameras (four selection buttons are
  fitted in the current hardware configuration).
- Adjust iris, gain, white balance, tint, lift/black balance, gain/white balance,
  contrast, and hue.
- Move a motorized iris fader when the selected camera changes.
- Send Blackmagic SDI camera-control packets and read camera tally information.
- Render the active camera and control values with LVGL.

Some model fields and display pages are prepared for future controls. The
current physical mapping is defined in
`components/control_mapper/src/control_mapper.c`.

## Hardware architecture

The prototype is built around the following parts, listed in logical signal
order from user input to camera-control output.

### 1. Processing and display

- **Main board:** a custom PCB designed around a Waveshare ESP32-P4 module. The
  OpenRCP prototype does not use the Waveshare ESP32-P4-NANO development board.
- **MCU module:** Waveshare ESP32-P4 module mounted on the custom PCB.
- **Display:** Waveshare 4-inch DSI LCD. LVGL renders the interface, while the
  Waveshare board support package initializes the DSI display and backlight.

The firmware is compiled for the `esp32p4` target and relies on external PSRAM
for display buffering. It currently uses the Waveshare ESP32-P4-NANO board
support package declared in `main/idf_component.yml` because that package
provides the required display support. This software dependency does not mean
that the prototype uses the ESP32-P4-NANO development board.

A Waveshare ESP32-P4 development board can also be used for development or as
the basis of another build, provided the display and external controls are
wired to match the firmware configuration. A different carrier board or
display may require pin, memory, and display-initialization changes.

### 2. Operator controls

- **Rotary encoders:** seven Bourns `PEC12R-4015F-S0024` incremental encoders
  with push switches. Their A/B signals and push switches connect directly to
  ESP32-P4 GPIOs.
- **Camera buttons:** four active-low momentary switches connected directly to
  ESP32-P4 GPIOs.
- **Additional buttons:** two groups of six buttons read through two `PCF8574P`
  I/O expanders on the shared I2C bus. Their configured addresses are `0x23`
  and `0x24`, with separate interrupt signals to the MCU.

The firmware enables internal pull-ups for the direct camera buttons and
encoder push switches. The encoder A/B inputs also use pull-ups. Confirm that
external hardware never drives an ESP32-P4 input above its permitted I/O
voltage.

### 3. Motorized iris control

- **Motorized slide potentiometer:** Bourns `PSM60-081A-103B2`. Its wiper is
  sampled by an ESP32-P4 ADC input to determine the physical fader position.
- **Motor driver:** `TB6612FNG` dual H-bridge driver. One bridge is controlled
  with PWM, two direction inputs, and standby. The motor must not be connected
  directly to ESP32-P4 GPIOs.

The logic supply, motor supply, grounding, and current capability must match
the chosen implementation. Motor noise should be kept out of the analog fader
feedback path through appropriate layout, decoupling, and grounding.

### 4. Camera-control interface

- **Interface:** a compatible Blackmagic Design 3G-SDI Arduino Shield.
- **Connection:** shared 100 kHz I2C bus.
- **Purpose:** the firmware sends Blackmagic camera-control packets for the
  selected camera and reads tally information returned by the shield.

The controller, I/O expanders, motor driver, display, and shield require a
stable power system with a shared reference where the circuit design requires
it. Verify every device's supply and logic levels before assembly.

## Wiring configuration

The firmware is currently configured for the following proof-of-concept
wiring. This is a firmware pin map, not a complete electrical schematic.

| Function | Connection |
| --- | --- |
| Blackmagic shield / shared I2C SDA | GPIO 21 |
| Blackmagic shield / shared I2C SCL | GPIO 22 |
| I2C frequency | 100 kHz |
| Shield address detection | `0x6E`, then `0x37` |
| Right button panel | address `0x23`, interrupt GPIO 6 |
| Left button panel | address `0x24`, interrupt GPIO 5 |
| Camera buttons 1–4 | GPIO 12, 10, 11, 9 |
| TB6612FNG PWM / AIN2 / AIN1 / standby | GPIO 29 / 28 / 27 / 26 |
| Bourns PSM60 fader wiper | GPIO 23 (ADC) |

The Bourns PEC12R encoder pins are:

| Control | A | B | Push |
| --- | ---: | ---: | ---: |
| White red | 43 | 44 | 42 |
| Black red | 52 | 53 | 51 |
| Black blue | 33 | 34 | 54 |
| White green | 40 | 41 | 39 |
| Black level | 30 | 31 | 32 |
| Black green | 46 | 47 | 45 |
| White blue | 49 | 50 | 48 |

Change button, panel, and encoder assignments in
`components/controls/include/controls_config.h`. Change the shield bus pins in
`components/bmd_shield/src/bmd_shield.c`, and the motor/ADC pins and motion
tuning in `components/iris_fader/src/iris_fader.c`.

### Fader calibration

The checked-in prototype uses an ADC range of `0`–`2528`, an inverted motor
direction, and empirically tuned PWM/deadband values. Before normal use:

1. Disconnect the camera-control output and make the mechanism safe to move.
2. Verify the potentiometer reading at both mechanical limits.
3. Set `IRIS_RAW_MIN` and `IRIS_RAW_MAX` to values inside those limits.
4. Confirm `MOTOR_DIRECTION_INVERTED` by commanding a small movement.
5. Tune PWM, deadband, and timeout constants conservatively for the motor and
   mechanics in use.

Keep an accessible way to remove motor power during initial testing.

## Software requirements

- ESP-IDF 5.5 or newer (the dependency lock was generated with ESP-IDF 5.5.4).
- Python and the toolchain installed by ESP-IDF.
- Git, CMake, and Ninja as provided or configured by ESP-IDF.

Managed components are declared in `main/idf_component.yml`; `idf.py` downloads
them on the first configure/build. The important direct dependencies are the
Waveshare ESP32-P4-NANO BSP and LVGL 9.

## Project layout

| Path | Purpose |
| --- | --- |
| `main/` | Firmware entry point and event loop |
| `components/bmd_shield/` | Blackmagic shield I2C and packet protocol |
| `components/controls/` | Buttons, panels, encoders, and event queue |
| `components/control_mapper/` | Physical-control to application-action mapping |
| `components/iris_fader/` | Motor control and fader feedback |
| `components/openrcp_model/` | Per-camera state model |
| `components/openrcp_app/` | Application actions and state updates |
| `components/openrcp_display/` | LVGL user interface |

## License

OpenRCP is distributed under the GNU General Public License v3.0. See
[`LICENSE`](LICENSE) for the full terms. Hardware design files are not currently
included in this repository; do not assume that the firmware license covers
third-party boards or shields.
