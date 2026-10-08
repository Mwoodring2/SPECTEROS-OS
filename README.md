# SPECTER//OS — Phase 0 virtual pet

## Current flashed firmware

The current working firmware is `imports/SPECTER_OS_CYD4_Phase0Q2`.
Earlier phase packages are retained in `imports/` for reference. The root-level
prototype described below predates those packages.

Build and flash the current firmware from its directory:

```powershell
Set-Location imports/SPECTER_OS_CYD4_Phase0Q2
pio run -e cyd4
pio run -e cyd4 -t upload
```

This targets COM4 and uses the 3.5 MB app partition in `partitions_specter.csv`.
Local compilation, transparent sprite compositing, and missing-calibration
recovery fixes are included. Generated PlatformIO build files are excluded from Git.

Digital Ghost is the main experience. The boot sequence leads to a persistent,
animated pet screen rather than a tool launcher.

## Prototype behavior

- Signal, Charge, Mood, and Stability are clamped to 0–100.
- Each minute costs one Signal, Charge, and Mood, and two Stability.
- DATA restores 18 Signal; CHARGE restores 22 Charge; STABLE restores 20 Stability.
- PLAY spends five Charge, restores 16 Mood, and triggers 2.5 seconds of dancing
  and particles. It requires at least five Charge.
- Touches trigger once per press, with a 750 ms care cooldown.
- Low Stability takes precedence over tiredness; otherwise high Mood and adequate
  Signal produce the happy state. Neutral ghosts float; happy ghosts emit particles;
  tired ghosts fade; unstable ghosts phase and tear.
- ESP32 Preferences stores stats, care count, fractional minute, and last valid
  timestamp after successful interactions and every minute. A sudden power loss
  can lose up to a minute of unsaved decay.

## CYD4 hardware test

The default PlatformIO environment is `cyd4_hardware_test`; `cyd4_pet` selects
the virtual pet. Both target COM4 with TFT_eSPI and the LCDWIKI E32R40T wiring:
ST7796S, SPI MOSI 13 / MISO 12 / clock 14, LCD CS 15 / DC 2, backlight 27,
XPT2046 CS 33. Rotation 1 produces 480 × 320 landscape.
Reference: https://www.lcdwiki.com/4.0inch_ESP32-32E_Display

On first boot, tap the four calibration targets; calibration is saved in NVS.
The hardware test then displays the identification screen. Each press draws a
crosshair and reports coordinates, with AREA OK for touches inside the test box.
Coordinates are also logged at 115200 baud. COM4 on screen is the configured
host port, not a value detected by the board. The driver uses direct drawing.

Build: `pio run -e cyd4_hardware_test`
Upload: `pio run -e cyd4_hardware_test -t upload`

Set the system clock from an RTC or network before `ghost::pet.begin()` to account
for time powered off. Configure the local timezone for the header clock. Without
a valid clock at startup, powered-off time cannot be measured and is skipped;
uptime decay still works. The header shows `--:--` until a valid clock exists.

Integrity, dormancy, corrupted form, progression rewards, accessories, skins, and
device tools are future features. The fourth button directly stabilizes the pet
in Phase 0; a MORE menu can replace it when those features arrive.
