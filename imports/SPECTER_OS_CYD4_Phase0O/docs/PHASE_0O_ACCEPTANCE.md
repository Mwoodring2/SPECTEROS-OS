# Phase 0O acceptance

## Boot
- [ ] existing touch calibration survives
- [ ] existing pet state survives
- [ ] display remains stable
- [ ] boot reaches normal habitat

## Passive RF
- [ ] serial reports `[RF] passive awareness init`
- [ ] first asynchronous scan starts
- [ ] scan completes without blocking UI
- [ ] network count is plausible
- [ ] strongest RSSI/channel are plausible
- [ ] open/hidden counts update
- [ ] repeated scans occur approximately every 12 seconds

## UI
- [ ] header shows `RF READY` before first result
- [ ] header shows RF scan/result state
- [ ] Ghost animation remains smooth during scan
- [ ] touch remains responsive while scan is running

## Regression
- [ ] no Wi-Fi connection is attempted
- [ ] no deauth/injection behavior exists
- [ ] no capture behavior exists
- [ ] 8-bit renderer remains stable
- [ ] action buttons still work
