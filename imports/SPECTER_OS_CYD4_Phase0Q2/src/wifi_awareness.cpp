#include "wifi_awareness.h"

#include <WiFi.h>

namespace specter::rf {

PassiveWifiAwareness wifi;

void PassiveWifiAwareness::begin() {
  WiFi.mode(WIFI_STA);

  // Do not connect to a network here. This phase is passive discovery only.
  // Persistent credential state is left untouched.
  snapshot_.initialized = true;
  snapshot_.lastScanCompleteMs = millis();

  for (uint8_t i = 0; i < kWifiChannelCount; ++i) {
    channelStats_[i].channel = static_cast<uint8_t>(kWifiChannelMin + i);
  }

  Serial.printf("[RF] passive awareness init mode=STA interval=%lu ms\n",
                static_cast<unsigned long>(kScanIntervalMs));
}

void PassiveWifiAwareness::setEnabled(bool enabled) {
  enabled_ = enabled;

  if (!enabled_) {
    WiFi.scanDelete();
    snapshot_.scanning = false;
    forceScan_ = false;
    Serial.println("[RF] passive awareness disabled");
  } else {
    forceScan_ = true;
    Serial.println("[RF] passive awareness enabled");
  }
}

void PassiveWifiAwareness::requestScan() {
  if (!enabled_) return;
  forceScan_ = true;
}

void PassiveWifiAwareness::beginScan(uint32_t nowMs) {
  if (!enabled_ || snapshot_.scanning) return;

  WiFi.scanDelete();

  // async=true, show_hidden=true
  const int16_t rc = WiFi.scanNetworks(true, true);

  if (rc == WIFI_SCAN_RUNNING || rc >= 0) {
    snapshot_.scanning = true;
    snapshot_.lastScanStartMs = nowMs;
    forceScan_ = false;
    Serial.printf("[RF] scan start generation=%u\n",
                  static_cast<unsigned>(snapshot_.scanGeneration + 1));
    return;
  }

  snapshot_.failedScans++;
  snapshot_.scanning = false;
  forceScan_ = false;
  Serial.printf("[RF] scan start failed rc=%d failures=%lu\n",
                static_cast<int>(rc),
                static_cast<unsigned long>(snapshot_.failedScans));
}


int PassiveWifiAwareness::findObservedByBssid(const char* bssidText) const {
  if (bssidText == nullptr || bssidText[0] == '\0') return -1;

  for (uint8_t i = 0; i < observedCount_; ++i) {
    if (strncmp(observed_[i].bssid, bssidText, sizeof(observed_[i].bssid)) == 0) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

void PassiveWifiAwareness::updateObservedNetwork(
    const char* ssid,
    const uint8_t* bssid,
    int16_t rssi,
    uint8_t channel,
    uint8_t auth,
    uint32_t nowMs) {
  if (bssid == nullptr) return;

  char bssidText[18] = {0};
  snprintf(
      bssidText,
      sizeof(bssidText),
      "%02X:%02X:%02X:%02X:%02X:%02X",
      bssid[0], bssid[1], bssid[2],
      bssid[3], bssid[4], bssid[5]);

  int slot = findObservedByBssid(bssidText);

  if (slot < 0) {
    if (observedCount_ < kMaxObservedNetworks) {
      slot = observedCount_++;
    } else {
      // Replace a weak/stale observation when full.
      uint8_t candidate = 0;
      for (uint8_t i = 1; i < observedCount_; ++i) {
        const bool older =
            observed_[i].lastSeenMs < observed_[candidate].lastSeenMs;
        const bool muchWeaker =
            observed_[i].rssi < observed_[candidate].rssi - 10;
        if (older || muchWeaker) {
          candidate = i;
        }
      }
      slot = candidate;
    }

    observed_[slot] = ObservedNetwork{};
    observed_[slot].firstSeenMs = nowMs;
  }

  ObservedNetwork& entry = observed_[slot];

  if (ssid == nullptr || ssid[0] == '\0') {
    strncpy(entry.ssid, "<hidden>", sizeof(entry.ssid) - 1);
  } else {
    strncpy(entry.ssid, ssid, sizeof(entry.ssid) - 1);
  }
  entry.ssid[sizeof(entry.ssid) - 1] = '\0';

  strncpy(entry.bssid, bssidText, sizeof(entry.bssid) - 1);
  entry.bssid[sizeof(entry.bssid) - 1] = '\0';

  entry.rssi = rssi;
  entry.channel = channel;
  entry.auth = auth;
  entry.lastSeenMs = nowMs;

  if (entry.seenCount < 65535) {
    entry.seenCount++;
  }
}

void PassiveWifiAwareness::sortObservedByRssi() {
  if (observedCount_ < 2) return;

  for (uint8_t i = 1; i < observedCount_; ++i) {
    ObservedNetwork key = observed_[i];
    int j = static_cast<int>(i) - 1;

    while (j >= 0 && observed_[j].rssi < key.rssi) {
      observed_[j + 1] = observed_[j];
      --j;
    }
    observed_[j + 1] = key;
  }
}


void PassiveWifiAwareness::rebuildChannelStats() {
  int32_t rssiSums[kWifiChannelCount] = {0};

  for (uint8_t i = 0; i < kWifiChannelCount; ++i) {
    channelStats_[i].channel = static_cast<uint8_t>(kWifiChannelMin + i);
    channelStats_[i].networkCount = 0;
    channelStats_[i].strongestRssi = -127;
    channelStats_[i].averageRssi = -127;
    channelStats_[i].densityScore = 0;
  }

  for (uint8_t i = 0; i < observedCount_; ++i) {
    const ObservedNetwork& net = observed_[i];
    if (net.channel < kWifiChannelMin || net.channel > kWifiChannelMax) continue;

    const uint8_t idx = static_cast<uint8_t>(net.channel - kWifiChannelMin);
    ChannelStat& stat = channelStats_[idx];

    if (stat.networkCount < 255) {
      stat.networkCount++;
    }

    if (net.rssi > stat.strongestRssi) {
      stat.strongestRssi = net.rssi;
    }

    rssiSums[idx] += net.rssi;
  }

  busiestChannel_ = 0;
  quietestChannel_ = 0;

  uint16_t busiestScore = 0;
  uint16_t quietestScore = 65535;

  for (uint8_t i = 0; i < kWifiChannelCount; ++i) {
    ChannelStat& stat = channelStats_[i];

    if (stat.networkCount > 0) {
      stat.averageRssi =
          static_cast<int16_t>(rssiSums[i] / stat.networkCount);

      // Simple passive density heuristic:
      // network count dominates, while stronger RF adds extra congestion weight.
      int16_t strengthWeight = stat.strongestRssi + 100;
      if (strengthWeight < 0) strengthWeight = 0;
      if (strengthWeight > 70) strengthWeight = 70;

      stat.densityScore =
          static_cast<uint16_t>(stat.networkCount * 20 + strengthWeight);
    } else {
      stat.averageRssi = -127;
      stat.strongestRssi = -127;
      stat.densityScore = 0;
    }

    if (stat.densityScore > busiestScore) {
      busiestScore = stat.densityScore;
      busiestChannel_ = stat.channel;
    }

    if (stat.densityScore < quietestScore) {
      quietestScore = stat.densityScore;
      quietestChannel_ = stat.channel;
    }
  }

  Serial.printf(
      "[RF] channel intelligence busy=%u quiet=%u busyScore=%u quietScore=%u\n",
      static_cast<unsigned>(busiestChannel_),
      static_cast<unsigned>(quietestChannel_),
      static_cast<unsigned>(busiestScore),
      static_cast<unsigned>(quietestScore));
}

void PassiveWifiAwareness::consumeScanResults(int16_t count, uint32_t nowMs) {
  snapshot_.networkCount = count > 0 ? static_cast<uint16_t>(count) : 0;
  snapshot_.openCount = 0;
  snapshot_.hiddenCount = 0;
  snapshot_.strongestRssi = -127;
  snapshot_.strongestChannel = 0;
  snapshot_.strongestSsid[0] = '\0';

  for (int16_t i = 0; i < count; ++i) {
    const String ssid = WiFi.SSID(i);
    const int32_t rssi = WiFi.RSSI(i);
    const uint8_t channel = static_cast<uint8_t>(WiFi.channel(i));
    const wifi_auth_mode_t auth = WiFi.encryptionType(i);
    const uint8_t* bssid = WiFi.BSSID(i);

    updateObservedNetwork(
        ssid.c_str(),
        bssid,
        static_cast<int16_t>(rssi),
        channel,
        static_cast<uint8_t>(auth),
        nowMs);

    if (ssid.length() == 0) {
      snapshot_.hiddenCount++;
    }

    if (auth == WIFI_AUTH_OPEN) {
      snapshot_.openCount++;
    }

    if (rssi > snapshot_.strongestRssi) {
      snapshot_.strongestRssi = static_cast<int16_t>(rssi);
      snapshot_.strongestChannel = channel;

      if (ssid.length() == 0) {
        strncpy(snapshot_.strongestSsid, "<hidden>",
                sizeof(snapshot_.strongestSsid) - 1);
      } else {
        strncpy(snapshot_.strongestSsid, ssid.c_str(),
                sizeof(snapshot_.strongestSsid) - 1);
      }
      snapshot_.strongestSsid[sizeof(snapshot_.strongestSsid) - 1] = '\0';
    }
  }

  sortObservedByRssi();
  rebuildChannelStats();

  snapshot_.scanGeneration++;
  snapshot_.lastScanCompleteMs = nowMs;
  snapshot_.scanning = false;

  Serial.printf(
      "[RF] scan complete gen=%u nets=%u open=%u hidden=%u observed=%u strongest=%ddBm ch=%u ssid=%s\n",
      static_cast<unsigned>(snapshot_.scanGeneration),
      static_cast<unsigned>(snapshot_.networkCount),
      static_cast<unsigned>(snapshot_.openCount),
      static_cast<unsigned>(snapshot_.hiddenCount),
      static_cast<unsigned>(observedCount_),
      static_cast<int>(snapshot_.strongestRssi),
      static_cast<unsigned>(snapshot_.strongestChannel),
      snapshot_.strongestSsid[0] ? snapshot_.strongestSsid : "-");

  WiFi.scanDelete();
}

void PassiveWifiAwareness::clearTransientScan() {
  snapshot_.scanning = false;
  WiFi.scanDelete();
}

void PassiveWifiAwareness::update(uint32_t nowMs) {
  if (!enabled_ || !snapshot_.initialized) return;

  if (snapshot_.scanning) {
    const int16_t result = WiFi.scanComplete();

    if (result >= 0) {
      consumeScanResults(result, nowMs);
      return;
    }

    if (result == WIFI_SCAN_FAILED) {
      snapshot_.failedScans++;
      clearTransientScan();
      Serial.printf("[RF] scan failed failures=%lu\n",
                    static_cast<unsigned long>(snapshot_.failedScans));
      return;
    }

    return;
  }

  const bool initialReady =
      snapshot_.scanGeneration == 0 &&
      nowMs >= kInitialDelayMs;

  const bool periodicReady =
      snapshot_.scanGeneration > 0 &&
      nowMs - snapshot_.lastScanCompleteMs >= kScanIntervalMs;

  if (forceScan_ || initialReady || periodicReady) {
    beginScan(nowMs);
  }
}

}  // namespace specter::rf
