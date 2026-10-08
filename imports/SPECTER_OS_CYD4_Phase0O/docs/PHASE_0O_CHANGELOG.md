# Phase 0O — Passive Wi-Fi Awareness Core

Parent: Phase 0N.

This is the first phase after the roadmap reset to operational capability first.

## Scope
Adds a passive, asynchronous Wi-Fi awareness subsystem.

The firmware now continuously maintains a lightweight RF snapshot:
- visible network count
- open network count
- hidden network count
- strongest observed RSSI
- strongest network channel
- strongest network SSID
- scan generation
- scan failure count

## Runtime model
- station radio mode only
- no connection attempt
- no packet injection
- no deauthentication
- no handshake capture
- no PMKID capture
- no promiscuous packet callback yet
- asynchronous `WiFi.scanNetworks(...)`
- default scan interval: 12 seconds
- first scan begins shortly after boot
- scan results are deleted after summarization to release memory

## UI
Operational RF telemetry replaces the pet-growth identity line in the header.

Examples:
- `RF READY`
- `RF SCANNING`
- `RF 14 O:2 CH:6 -48dBm`

The Ghost/pet systems remain present but are no longer the primary information hierarchy.

## Architecture
Added:
- `include/wifi_awareness.h`
- `src/wifi_awareness.cpp`

The module is separate from pet logic so later operational modes can build on the same RF state without coupling to Ghost behavior.

## Preserved
- existing visual compositor
- Phase 0L.1 animation baseline
- Phase 0M audio architecture
- Phase 0N data remains compatible
- touch calibration
- 8-bit renderer
- strict station gating
