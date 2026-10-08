# Channel Intelligence

Phase 0Q computes a passive density score for 2.4 GHz Wi-Fi channels 1–13.

## Score
The score is not a spectrum-analyzer measurement.

It is a lightweight operational heuristic derived from:
1. observed access-point count on the channel
2. strongest observed RSSI

This makes it useful for quick field awareness while remaining inexpensive enough for the ESP32 UI loop.

## Interpretation
LOW
little observed occupancy

MOD
moderate observed occupancy

BUSY
multiple / stronger observed APs

HOT
high passive density score

## Limitations
Normal Wi-Fi scans report discovered access points, not airtime utilization.

A later passive monitor-mode phase can add frame-density / activity measurements if desired.
