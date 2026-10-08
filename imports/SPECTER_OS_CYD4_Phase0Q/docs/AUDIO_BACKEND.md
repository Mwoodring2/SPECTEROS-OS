# SPECTER//OS audio backend

## Phase 0M default
Hosyond/LCDWiki ESP32-32E onboard audio path:

- GPIO26: DAC audio signal
- GPIO4: onboard mono amplifier enable, active-low

The engine outputs centered DAC square-wave tone patterns into the onboard amplifier.

## External NS4168 I2S module
The approved external audio hardware is still supported as the intended higher-fidelity upgrade:

- one NS4168 I2S mono amplifier module
- matching 3 W speaker
- 5 V / GND
- I2S BCLK / WS / DIN

Phase 0M deliberately does not assign those three I2S GPIOs because the final conflict-free pin set has not yet been locked for this exact 4-inch board build.

The audio engine is isolated from pet logic so an I2S backend can replace the DAC backend later without rewriting action/personality state handling.
