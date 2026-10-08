# Phase 0Q.1 — Operational UI Cleanup

Parent: Phase 0Q hardware functional.

This pass changes presentation only. RF logic and channel-density calculations are untouched.

## Inventory cleanup
- moved `CHANNELS` from the footer/help region to the top-right header
- removed overlap with the page/help line
- simplified top-right state to `PASSIVE n` / `SCAN n`
- replaced long `TAP RF HEADER TO CLOSE` footer text with:
  `PAGE x/y // n OBSERVED`
- increased header-label contrast
- BSSID now uses the brighter blue-soft secondary color
- secure `SEC` values now render white instead of dim gray
- `SEEN` counts render cyan

## Channel view cleanup
- top-right busy/quiet status uses higher contrast
- `LEGEND` label uses white
- density-score explanation uses blue-soft instead of dim gray

## Locked
- Phase 0Q channel model
- density heuristic
- network inventory data model
- navigation behavior
- scan interval / asynchronous scan logic
- passive-only scope
