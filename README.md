# OpenRCP

OpenRCP is open-source firmware for a hardware remote control panel (RCP) that
shades Blackmagic Design cameras through an SDI camera-control shield. It runs
on a Waveshare ESP32-P4-NANO, presents the camera state on its display, reads
physical controls, drives a motorized iris fader, and reports preview/program
tally state.

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

## Required hardware

- Waveshare ESP32-P4-NANO development board with its display connected.
- A compatible Blackmagic Design 3G-SDI Arduino Shield, connected over I2C.
- Two I2C button input boards at addresses `0x23` and `0x24`.
- Seven quadrature rotary encoders with push switches.
- Four active-low camera-selection push buttons.
- A motorized fader, a suitable H-bridge/motor driver, and an analog feedback
  connection. The motor must not be driven directly from an ESP32 GPIO.
- A stable supply sized for the ESP32 board, display, shield, input boards, and
  fader motor. Join grounds where required by the chosen driver design.

The code enables internal pull-ups for the camera buttons and rotary encoder
switches. Confirm that external hardware does not drive these pins above the
ESP32-P4 I/O voltage.

## Hardware configuration

The firmware is currently configured for the following prototype wiring.

| Function | Connection |
| --- | --- |
| Blackmagic shield / shared I2C SDA | GPIO 21 |
| Blackmagic shield / shared I2C SCL | GPIO 22 |
| I2C frequency | 100 kHz |
| Shield address detection | `0x6E`, then `0x37` |
| Right button panel | address `0x23`, interrupt GPIO 6 |
| Left button panel | address `0x24`, interrupt GPIO 5 |
| Camera buttons 1–4 | GPIO 12, 10, 11, 9 |
| Fader motor PWM / AIN2 / AIN1 / standby | GPIO 29 / 28 / 27 / 26 |
| Fader potentiometer | GPIO 23 (ADC) |

Rotary encoder pins are:

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

## Build and flash

From an ESP-IDF-enabled terminal:

```sh
git clone <your-repository-url>
cd OpenRCP
idf.py set-target esp32p4
idf.py build
idf.py -p <serial-port> flash monitor
```

Replace `<your-repository-url>` and `<serial-port>` with the repository URL and
the board's serial port. Exit the serial monitor with `Ctrl+]`.

The included development container provides an ESP-IDF environment. USB access
from a container depends on the host OS and container runtime; flashing from a
native ESP-IDF terminal may be simpler on Windows or macOS.

## Controls

- Camera buttons 1–3 select cameras 1–3. In the current mapping, physical
  camera button 4 is reserved as shift and does not select camera 4.
- The right button panel controls gain down/up, white balance down/up, and tint
  down/up. The left panel is detected but currently unassigned.
- Turning an encoder adjusts its labelled color or black-level value. Encoder
  pushes provide tint, white-balance, and gain shortcuts.
- Holding a shading encoder for three seconds resets that encoder's value.
- Holding camera button 1, 2, or 3 for three seconds resets the selected
  camera's shading values.
- Holding white-balance down and up together for three seconds resets cameras
  1–4 and transmits their default state.

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

## Troubleshooting

- **Shield or panels are missing:** check the boot log's I2C scan, wiring,
  common ground, pull-ups, and configured addresses.
- **The fader moves in the wrong direction:** remove motor power, then verify the
  driver wiring and `MOTOR_DIRECTION_INVERTED` before retrying.
- **The display does not start:** confirm the exact Waveshare board/display
  revision and that the BSP dependency matches it.
- **A control is reversed:** adjust `invert_direction` for that encoder in
  `controls_config.h`.

## License

OpenRCP is distributed under the GNU General Public License v3.0. See
[`LICENSE`](LICENSE) for the full terms. Hardware design files are not currently
included in this repository; do not assume that the firmware license covers
third-party boards or shields.
