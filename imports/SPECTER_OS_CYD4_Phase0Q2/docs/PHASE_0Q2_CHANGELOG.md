# Phase 0Q.2 — Operational Navigation Cleanup

Parent: Phase 0Q.1.

## Navigation hierarchy

### Home / habitat
`SPECTER//OS` header is now the canonical operational entry point.

Tap:
`SPECTER//OS` -> `NETWORK INVENTORY`

The RF telemetry region is informational on the home screen and no longer doubles as the inventory button.

### Network Inventory
`CHANNELS` has one clear function:

`NETWORK INVENTORY` -> `CHANNEL INTELLIGENCE`

The old RF-header shortcut for closing inventory was removed.

### Channel Intelligence
- `INVENTORY` -> Network Inventory
- `CLOSE` -> Home / habitat
- `RESCAN` -> Fresh passive asynchronous scan

## Why
This removes ambiguous touch paths and gives the operational screens a predictable hierarchy:

HOME
-> SPECTER//OS
-> NETWORK INVENTORY
-> CHANNELS
-> CHANNEL INTELLIGENCE

The CHANNELS button remains dedicated to channel intelligence rather than also behaving like a home/back control.

## Scope
Navigation-only patch. RF scanning, inventory logic, density calculations, and passive-only behavior are unchanged.
