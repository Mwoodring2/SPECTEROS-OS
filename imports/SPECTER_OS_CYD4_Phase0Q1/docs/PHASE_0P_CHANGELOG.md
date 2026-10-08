# Phase 0P — Network Inventory / Observed Network Table

Parent: Phase 0O hardware PASS.

## Inventory
The passive RF subsystem now retains up to 32 observed BSSIDs in RAM.

Per network:
- SSID
- BSSID
- RSSI
- channel
- authentication type
- seen count
- first-seen timestamp
- last-seen timestamp

Records update by BSSID on every passive scan and sort strongest-first.

## UI
Tap the RF telemetry in the upper-right header to open `NETWORK INVENTORY`.

The screen shows six rows per page:
- SSID
- BSSID
- channel
- RSSI
- security
- seen count

Controls:
- PREV
- CLOSE
- NEXT
- RF header tap also closes

Open networks are highlighted.

The inventory redraws when a passive scan completes. Habitat rendering is suppressed while this operational screen is open.

## Persistence
Inventory is intentionally RAM-only in 0P.
Long-term observation history belongs on SD rather than NVS.

## Scope
Passive discovery only. No association, injection, deauthentication, raw TX, handshake capture, or PMKID capture.
