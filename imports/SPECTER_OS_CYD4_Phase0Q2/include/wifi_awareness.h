#pragma once

#include <Arduino.h>

namespace specter::rf {

constexpr uint8_t kMaxObservedNetworks = 32;
constexpr uint8_t kWifiChannelMin = 1;
constexpr uint8_t kWifiChannelMax = 13;
constexpr uint8_t kWifiChannelCount = kWifiChannelMax - kWifiChannelMin + 1;

struct ChannelStat {
  uint8_t channel = 0;
  uint8_t networkCount = 0;
  int16_t strongestRssi = -127;
  int16_t averageRssi = -127;
  uint16_t densityScore = 0;
};

struct ObservedNetwork {
  char ssid[33] = {0};
  char bssid[18] = {0};

  int16_t rssi = -127;
  uint8_t channel = 0;
  uint8_t auth = 0;

  uint16_t seenCount = 0;
  uint32_t firstSeenMs = 0;
  uint32_t lastSeenMs = 0;
};

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

  uint8_t observedCount() const { return observedCount_; }
  const ObservedNetwork* observed(uint8_t index) const {
    return index < observedCount_ ? &observed_[index] : nullptr;
  }

  const ChannelStat* channelStat(uint8_t channel) const {
    if (channel < kWifiChannelMin || channel > kWifiChannelMax) return nullptr;
    return &channelStats_[channel - kWifiChannelMin];
  }

  uint8_t busiestChannel() const { return busiestChannel_; }
  uint8_t quietestChannel() const { return quietestChannel_; }

  void requestScan();
  void setEnabled(bool enabled);
  bool enabled() const { return enabled_; }

 private:
  void beginScan(uint32_t nowMs);
  void consumeScanResults(int16_t count, uint32_t nowMs);
  void clearTransientScan();

  void updateObservedNetwork(
      const char* ssid,
      const uint8_t* bssid,
      int16_t rssi,
      uint8_t channel,
      uint8_t auth,
      uint32_t nowMs);
  void sortObservedByRssi();
  void rebuildChannelStats();
  int findObservedByBssid(const char* bssidText) const;

  Snapshot snapshot_;
  ObservedNetwork observed_[kMaxObservedNetworks];
  uint8_t observedCount_ = 0;

  ChannelStat channelStats_[kWifiChannelCount];
  uint8_t busiestChannel_ = 0;
  uint8_t quietestChannel_ = 0;
  bool enabled_ = true;
  bool forceScan_ = false;

  static constexpr uint32_t kScanIntervalMs = 12000;
  static constexpr uint32_t kInitialDelayMs = 2200;
};

extern PassiveWifiAwareness wifi;

}  // namespace specter::rf
