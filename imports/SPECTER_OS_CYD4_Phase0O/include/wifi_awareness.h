#pragma once

#include <Arduino.h>

namespace specter::rf {

struct Snapshot {
  bool initialized = false;
  bool scanning = false;

  uint16_t scanGeneration = 0;
  uint16_t networkCount = 0;
  uint16_t openCount = 0;
  uint16_t hiddenCount = 0;

  int16_t strongestRssi = -127;
  uint8_t strongestChannel = 0;
  char strongestSsid[33] = {0};

  uint32_t lastScanStartMs = 0;
  uint32_t lastScanCompleteMs = 0;
  uint32_t failedScans = 0;
};

class PassiveWifiAwareness {
 public:
  void begin();
  void update(uint32_t nowMs);

  const Snapshot& snapshot() const { return snapshot_; }

  void requestScan();
  void setEnabled(bool enabled);
  bool enabled() const { return enabled_; }

 private:
  void beginScan(uint32_t nowMs);
  void consumeScanResults(int16_t count, uint32_t nowMs);
  void clearTransientScan();

  Snapshot snapshot_;
  bool enabled_ = true;
  bool forceScan_ = false;

  static constexpr uint32_t kScanIntervalMs = 12000;
  static constexpr uint32_t kInitialDelayMs = 2200;
};

extern PassiveWifiAwareness wifi;

}  // namespace specter::rf
