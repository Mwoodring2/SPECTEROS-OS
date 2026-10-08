# Observed Network Inventory

Phase 0P maintains a fixed 32-entry passive inventory keyed by BSSID.

Why BSSID:
- several access points can share an SSID
- hidden networks may have no SSID
- BSSID provides the observed radio identity for table updates

The list sorts by current RSSI after each scan.

It is intentionally not written to NVS. A future SD logging phase should own persistent RF observation history.
