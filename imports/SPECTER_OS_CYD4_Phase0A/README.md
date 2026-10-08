# SPECTER//OS — CYD 4" Phase 0A

First hardware bring-up package for the Hosyond 4" ESP32-32E resistive-touch display, likely matching the LCDWIKI E32R40T family.

## Goal
This build contains **no virtual-pet logic yet**. It proves the ST7796S display, 480x320 landscape orientation, XPT2046 resistive touch, first-run calibration, NVS persistence, touch coordinate tracking, and COM4 upload/monitor configuration.

## Working pin configuration

| Function | GPIO |
|---|---:|
| TFT MISO | 12 |
| TFT MOSI | 13 |
| TFT SCLK | 14 |
| TFT CS | 15 |
| TFT DC / RS | 2 |
| TFT backlight | 27 |
| Touch CS | 33 |
| Touch IRQ | 36 |
| TFT reset | board reset / `-1` |

Display and XPT2046 touch share the display SPI bus.

## Build
```powershell
pio run -e cyd4
```

## Upload to COM4
```powershell
pio run -e cyd4 -t upload
```

## Serial monitor
```powershell
pio device monitor -p COM4 -b 115200
```

## First boot
1. Backlight turns on.
2. ST7796S initializes.
3. Display rotates to 480x320 landscape.
4. Touch calibration appears.
5. Touch the calibration targets.
6. Calibration is saved in NVS.
7. SPECTER//OS hardware-test screen appears.

After calibration, tapping anywhere should draw a crosshair and print calibrated x/y coordinates on screen and serial.

## Force recalibration
```powershell
pio run -e cyd4 -t erase
pio run -e cyd4 -t upload
```

## If red/blue are swapped
Add this only if the test confirms it:
```ini
-D TFT_RGB_ORDER=TFT_BGR
```

## Phase 0B after acceptance
- SPECTER//OS boot sequence
- Digital Ghost phase-in
- idle animation
- first virtual-pet state structure
- NVS-backed pet state
