# Phase 0Q — Channel Intelligence & RF Density View

Parent: Phase 0P hardware PASS.

## Channel model
The passive RF subsystem now derives per-channel statistics for Wi-Fi channels 1–13:

- observed network count
- strongest RSSI
- average RSSI
- density score

The density score is a lightweight passive heuristic:
- network count is the primary weight
- stronger observed RF increases the score

The system also identifies:
- busiest channel
- quietest channel

## UI
From `NETWORK INVENTORY`, tap `CHANNELS`.

The channel screen shows:
- one bar per channel
- network count above each bar
- density represented by bar height
- busiest channel outlined in magenta
- quietest channel outlined in green
- LOW / MOD / BUSY / HOT color legend
- busiest-channel summary
- quietest-channel summary

Controls:
- INVENTORY
- CLOSE
- RESCAN

`RESCAN` requests a fresh passive asynchronous scan.

## Runtime
The channel screen refreshes after each completed passive scan.

Habitat rendering remains suppressed while operational screens are open.

## Scope
Still passive-only:
- no association
- no packet injection
- no deauthentication
- no raw 802.11 TX
- no handshake / PMKID capture

## Preserved
- Phase 0P observed inventory
- 32-BSSID RAM table
- Phase 0O passive async scanning
- NVS / calibration
- 8-bit renderer
