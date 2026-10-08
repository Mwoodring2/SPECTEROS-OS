#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "board_config.h"
#include "ghost_idle_frames.h"

namespace {

TFT_eSPI tft;
Preferences prefs;

constexpr uint16_t kBg       = TFT_BLACK;
constexpr uint16_t kPanel    = 0x0841;
constexpr uint16_t kPanel2   = 0x1082;
constexpr uint16_t kCyan     = TFT_CYAN;
constexpr uint16_t kMagenta  = TFT_MAGENTA;
constexpr uint16_t kWhite    = TFT_WHITE;
constexpr uint16_t kDim      = 0x4208;
constexpr uint16_t kGreen    = TFT_GREEN;
constexpr uint16_t kYellow   = TFT_YELLOW;
constexpr uint16_t kRed      = TFT_RED;

uint16_t touchCal[5] = {0, 0, 0, 0, 0};
bool touchReady = false;

struct PetState {
  uint8_t signal = 78;
  uint8_t integrity = 88;
  uint8_t mood = 74;
  uint8_t charge = 86;
  uint8_t affection = 25;
  uint32_t interactions = 0;
  uint32_t saveCounter = 0;
};

PetState pet;

enum class MoodState : uint8_t {
  Normal,
  Happy,
  LowCharge,
  Unstable,
  Sleepy
};

enum class ReactionState : uint8_t {
  None,
  TapHappy,
  TapAnnoyed,
  Data,
  Play,
  Charge,
  Stabilize,
  SleepEnter,
  Wake
};

constexpr uint32_t kAnimMs = 260;
constexpr uint32_t kDecayTickMs = 30000;
constexpr uint32_t kAutoSaveMs = 30000;
constexpr uint32_t kReactionMs = 900;
constexpr uint32_t kIdleEventMinMs = 5000;
constexpr uint32_t kIdleEventMaxMs = 10000;

uint32_t lastAnimMs = 0;
uint32_t lastDecayMs = 0;
uint32_t lastAutoSaveMs = 0;
uint32_t lastTouchMs = 0;
uint32_t reactionUntilMs = 0;
uint32_t nextIdleEventMs = 0;
uint8_t animFrame = 0;
bool sleeping = false;
bool forceGhostRedraw = true;
ReactionState reaction = ReactionState::None;

struct Rect {
  int16_t x, y, w, h;
  const char* label;
};

constexpr Rect kButtons[4] = {
  {  8, 266, 112, 46, "DATA"      },
  {124, 266, 112, 46, "PLAY"      },
  {240, 266, 112, 46, "CHARGE"    },
  {356, 266, 116, 46, "STABILIZE" },
};

constexpr Rect kGhostHit = {160, 110, 160, 140};

uint8_t clamp100(int value) {
  if (value < 0) return 0;
  if (value > 100) return 100;
  return static_cast<uint8_t>(value);
}

bool loadCalibration() {
  prefs.begin("specter", true);
  const size_t n = prefs.getBytesLength("touchcal");
  if (n == sizeof(touchCal)) {
    const size_t read = prefs.getBytes("touchcal", touchCal, sizeof(touchCal));
    prefs.end();
    return read == sizeof(touchCal);
  }
  prefs.end();
  return false;
}

bool loadPetState() {
  prefs.begin("specter", true);
  const bool exists = prefs.isKey("petstate0c");
  if (exists) {
    const size_t n = prefs.getBytes("petstate0c", &pet, sizeof(pet));
    prefs.end();
    if (n == sizeof(pet)) {
      Serial.println("[PET] Loaded Phase 0C state.");
      return true;
    }
    return false;
  }
  prefs.end();
  Serial.println("[PET] No Phase 0C state yet; using defaults.");
  return false;
}

void savePetState() {
  pet.saveCounter++;
  prefs.begin("specter", false);
  const size_t n = prefs.putBytes("petstate0c", &pet, sizeof(pet));
  prefs.end();
  Serial.printf("[PET] Saved state (%u bytes), save #%lu\n",
                static_cast<unsigned>(n),
                static_cast<unsigned long>(pet.saveCounter));
}

MoodState currentMoodState() {
  if (sleeping) return MoodState::Sleepy;
  if (pet.charge <= 24) return MoodState::LowCharge;
  if (pet.integrity <= 29) return MoodState::Unstable;
  if (pet.mood >= 82 && pet.signal >= 55) return MoodState::Happy;
  return MoodState::Normal;
}

const char* moodLabel() {
  switch (currentMoodState()) {
    case MoodState::Happy: return "CONTENT";
    case MoodState::LowCharge: return "LOW CHARGE";
    case MoodState::Unstable: return "UNSTABLE";
    case MoodState::Sleepy: return "DORMANT";
    default: return "ONLINE";
  }
}

uint16_t moodColor() {
  switch (currentMoodState()) {
    case MoodState::Happy: return kMagenta;
    case MoodState::LowCharge: return kYellow;
    case MoodState::Unstable: return kRed;
    case MoodState::Sleepy: return kDim;
    default: return kCyan;
  }
}

void drawHeader() {
  tft.fillRect(0, 0, 480, 40, kBg);
  tft.drawFastHLine(8, 38, 464, kDim);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(kCyan, kBg);
  tft.drawString("SPECTER//OS", 10, 8, 4);

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(moodColor(), kBg);
  tft.drawString(moodLabel(), 470, 13, 2);
}

void drawBar(int x, int y, int w, const char* label, uint8_t value, uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(kWhite, kBg);
  tft.drawString(label, x, y, 2);

  const int bx = x + 72;
  const int bw = w - 72;
  const int bh = 10;
  tft.drawRect(bx, y + 2, bw, bh, kDim);
  tft.fillRect(bx + 2, y + 4, bw - 4, bh - 4, kPanel);

  const int fill = ((bw - 4) * value) / 100;
  if (fill > 0) {
    tft.fillRect(bx + 2, y + 4, fill, bh - 4, color);
  }

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(kDim, kBg);
  tft.drawString(String(value) + "%", x + w, y, 2);
}

void drawStats() {
  tft.fillRect(8, 44, 464, 62, kBg);
  drawBar(10, 48, 220, "SIGNAL",    pet.signal,    kCyan);
  drawBar(250,48, 220, "INTEGRITY", pet.integrity, kGreen);
  drawBar(10, 76, 220, "MOOD",      pet.mood,      kMagenta);
  drawBar(250,76, 220, "CHARGE",    pet.charge,    kYellow);
}

void drawButtons() {
  for (int i = 0; i < 4; ++i) {
    const Rect& b = kButtons[i];
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, kPanel);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, kDim);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(i == 3 ? kMagenta : kCyan, kPanel);
    tft.drawString(b.label, b.x + b.w/2, b.y + b.h/2, 2);
  }
}

void drawReactionText(const char* msg, uint16_t color) {
  tft.fillRect(120, 238, 240, 22, kBg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, kBg);
  tft.drawString(msg, 240, 248, 2);
}

void drawGhostFrame(uint8_t frame) {
  constexpr int kGhostX = 184;
  constexpr int kGhostY = 120;

  tft.fillRect(136, 106, 208, 154, kBg);

  uint16_t aura = kCyan;
  if (currentMoodState() == MoodState::Happy) aura = kMagenta;
  if (currentMoodState() == MoodState::LowCharge) aura = kYellow;
  if (currentMoodState() == MoodState::Unstable) aura = kRed;
  if (currentMoodState() == MoodState::Sleepy) aura = kDim;

  tft.drawEllipse(240, 222, 66, 10, kDim);
  tft.drawEllipse(240, 222, 48, 6, aura);

  int bobY = 0;
  if (!sleeping) {
    static const int8_t bob[4] = {0, -2, -4, -2};
    bobY = bob[frame % 4];
  }

  const uint16_t* pixels = specter::assets::kGhostIdleFrames[frame];
  tft.pushImage(kGhostX, kGhostY + bobY,
                specter::assets::kGhostWidth,
                specter::assets::kGhostHeight,
                pixels,
                specter::assets::kGhostTransparent);

  if (sleeping) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(kDim, kBg);
    tft.drawString("z", 303, 137, 2);
    tft.drawString("Z", 318, 122, 2);
  }

  if (currentMoodState() == MoodState::Happy && !sleeping) {
    tft.drawCircle(240, 172, 64, kMagenta);
    tft.drawPixel(176, 126, kCyan);
    tft.drawPixel(307, 141, kMagenta);
  } else if (currentMoodState() == MoodState::LowCharge) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(kYellow, kBg);
    tft.drawString("...", 240, 236, 2);
  } else if (currentMoodState() == MoodState::Unstable) {
    for (int i = 0; i < 5; ++i) {
      const int y = 126 + (i * 22);
      const int dx = (i % 2 == 0) ? 18 : -12;
      tft.drawFastHLine(168 + dx, y, 132, (i % 2) ? kMagenta : kCyan);
    }
  }

  // Reaction overlays.
  switch (reaction) {
    case ReactionState::TapHappy:
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(kMagenta, kBg);
      tft.drawString("<3", 304, 128, 2);
      break;
    case ReactionState::TapAnnoyed:
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(kYellow, kBg);
      tft.drawString("!", 304, 128, 2);
      break;
    case ReactionState::Data:
      tft.drawFastHLine(184, 178, 112, kCyan);
      tft.drawFastHLine(190, 182, 100, kCyan);
      break;
    case ReactionState::Play:
      tft.drawCircle(240, 172, 72, kMagenta);
      tft.drawCircle(240, 172, 78, kCyan);
      break;
    case ReactionState::Charge:
      for (int i = 0; i < 4; ++i) {
        tft.drawFastVLine(200 + i*26, 120, 14 + i*4, kYellow);
      }
      break;
    case ReactionState::Stabilize:
      tft.drawRect(170, 120, 140, 110, kGreen);
      break;
    default:
      break;
  }

  const char* label = "GHOST";
  uint16_t labelColor = aura;
  switch (reaction) {
    case ReactionState::TapHappy: label = "HELLO"; labelColor = kMagenta; break;
    case ReactionState::TapAnnoyed: label = "HEY"; labelColor = kYellow; break;
    case ReactionState::Data: label = "DATA RECEIVED"; labelColor = kCyan; break;
    case ReactionState::Play: label = "PLAYING"; labelColor = kMagenta; break;
    case ReactionState::Charge: label = "CHARGING"; labelColor = kYellow; break;
    case ReactionState::Stabilize: label = "STABLE"; labelColor = kGreen; break;
    case ReactionState::SleepEnter: label = "DORMANT"; labelColor = kDim; break;
    case ReactionState::Wake: label = "ONLINE"; labelColor = kCyan; break;
    default: break;
  }
  drawReactionText(label, labelColor);
}

void drawPetScreen() {
  tft.fillScreen(kBg);
  tft.drawRect(3, 3, 474, 314, kCyan);
  drawHeader();
  drawStats();
  drawButtons();
  drawGhostFrame(animFrame);
}

void bootSplash() {
  tft.fillScreen(kBg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(kDim, kBg);
  tft.drawString("INITIALIZING SPECTRAL INTERFACE", 240, 210, 2);

  for (int i = 0; i < 8; ++i) {
    const int y = 40 + random(0, 180);
    const int x = random(10, 330);
    const int w = random(40, 140);
    tft.drawFastHLine(x, y, w, (i % 2) ? kCyan : kMagenta);
    delay(50);
    tft.drawFastHLine(x, y, w, kBg);
  }

  tft.setTextColor(kCyan, kBg);
  tft.drawString("SPECTER//OS", 240, 108, 4);
  tft.setTextColor(kWhite, kBg);
  tft.drawString("GHOST SYSTEM ONLINE", 240, 148, 2);
  delay(650);
}

void startReaction(ReactionState r) {
  reaction = r;
  reactionUntilMs = millis() + kReactionMs;
  forceGhostRedraw = true;
}

void flashButton(int idx) {
  if (idx < 0 || idx > 3) return;
  const Rect& b = kButtons[idx];
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, kWhite);
  delay(60);
  drawButtons();
}

void applyAction(int idx) {
  if (sleeping && idx != 2) {
    sleeping = false;
    startReaction(ReactionState::Wake);
  }

  switch (idx) {
    case 0:
      pet.signal = clamp100(pet.signal + 14);
      pet.mood = clamp100(pet.mood + 2);
      pet.affection = clamp100(pet.affection + 1);
      startReaction(ReactionState::Data);
      Serial.println("[ACTION] DATA");
      break;

    case 1:
      pet.mood = clamp100(pet.mood + 15);
      pet.charge = clamp100(pet.charge - 3);
      pet.signal = clamp100(pet.signal - 1);
      pet.affection = clamp100(pet.affection + 3);
      startReaction(ReactionState::Play);
      Serial.println("[ACTION] PLAY");
      break;

    case 2:
      pet.charge = clamp100(pet.charge + 18);
      pet.mood = clamp100(pet.mood + 1);
      pet.affection = clamp100(pet.affection + 1);
      startReaction(ReactionState::Charge);
      Serial.println("[ACTION] CHARGE");
      break;

    case 3:
      pet.integrity = clamp100(pet.integrity + 12);
      pet.signal = clamp100(pet.signal - 2);
      pet.affection = clamp100(pet.affection + 1);
      startReaction(ReactionState::Stabilize);
      Serial.println("[ACTION] STABILIZE");
      break;

    default:
      return;
  }

  pet.interactions++;
  flashButton(idx);
  drawHeader();
  drawStats();
  forceGhostRedraw = true;
  savePetState();
}

bool pointInRect(uint16_t x, uint16_t y, const Rect& r) {
  return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

int hitButton(uint16_t x, uint16_t y) {
  for (int i = 0; i < 4; ++i) {
    if (pointInRect(x, y, kButtons[i])) return i;
  }
  return -1;
}

void handleGhostTap() {
  if (sleeping) {
    sleeping = false;
    pet.mood = clamp100(pet.mood + 2);
    startReaction(ReactionState::Wake);
    Serial.println("[GHOST] Wake tap.");
  } else {
    pet.interactions++;
    const bool happyTap = ((pet.interactions % 4) != 0);
    if (happyTap) {
      pet.mood = clamp100(pet.mood + 3);
      pet.affection = clamp100(pet.affection + 2);
      startReaction(ReactionState::TapHappy);
      Serial.println("[GHOST] Friendly tap.");
    } else {
      pet.mood = clamp100(pet.mood - 1);
      startReaction(ReactionState::TapAnnoyed);
      Serial.println("[GHOST] Annoyed tap.");
    }
  }

  drawHeader();
  drawStats();
  savePetState();
}

void toggleSleep() {
  sleeping = !sleeping;
  if (sleeping) {
    startReaction(ReactionState::SleepEnter);
    Serial.println("[PET] Entering dormancy.");
  } else {
    startReaction(ReactionState::Wake);
    Serial.println("[PET] Waking.");
  }
  drawHeader();
  forceGhostRedraw = true;
}

void runIdleEvent() {
  nextIdleEventMs = millis() + random(kIdleEventMinMs, kIdleEventMaxMs);
  if (sleeping) return;
  const int event = random(0, 4);
  if (event == 0) {
    startReaction(ReactionState::TapHappy);
  } else if (event == 1) {
    startReaction(ReactionState::None);
    animFrame = (animFrame + 2) % specter::assets::kGhostIdleFrameCount;
    forceGhostRedraw = true;
  }
}

void decayPet() {
  if (sleeping) {
    pet.charge = clamp100(pet.charge + 1);
    if (pet.mood < 100 && (millis() / kDecayTickMs) % 2 == 0) {
      pet.mood = clamp100(pet.mood + 1);
    }
  } else {
    pet.signal = clamp100(pet.signal - 1);
    pet.charge = clamp100(pet.charge - 1);

    static uint8_t slowTick = 0;
    slowTick++;
    if ((slowTick % 2) == 0) pet.mood = clamp100(pet.mood - 1);
    if ((slowTick % 4) == 0) pet.integrity = clamp100(pet.integrity - 1);
  }

  Serial.printf("[PET] signal=%u integrity=%u mood=%u charge=%u affection=%u sleep=%d\n",
                pet.signal, pet.integrity, pet.mood, pet.charge, pet.affection, sleeping ? 1 : 0);

  drawHeader();
  drawStats();
  forceGhostRedraw = true;
}

void printBootInfo() {
  Serial.println();
  Serial.println("======================================");
  Serial.println(" SPECTER//OS - CYD4 PHASE 0C");
  Serial.println(" GHOST BEHAVIOR + PERSONALITY");
  Serial.println("======================================");
  Serial.printf("Chip: %s rev %d @ %d MHz\n",
                ESP.getChipModel(),
                ESP.getChipRevision(),
                ESP.getCpuFreqMHz());
  Serial.printf("Heap free: %u bytes\n", ESP.getFreeHeap());
  Serial.println("Display: ST7796S 480x320 landscape");
  Serial.println("Touch: XPT2046");
  Serial.println("Sprite byte swap: ENABLED");
  Serial.println("Port: COM4");
}

} // namespace

void setup() {
  Serial.begin(specter::board::kSerialBaud);
  delay(250);
  printBootInfo();

  pinMode(specter::board::kTftBl, OUTPUT);
  digitalWrite(specter::board::kTftBl, HIGH);

  tft.init();
  tft.setSwapBytes(true);
  tft.setRotation(specter::board::kRotation);
  tft.fillScreen(kBg);

  if (loadCalibration()) {
    tft.setTouch(touchCal);
    touchReady = true;
    Serial.printf("[TOUCH] Calibration loaded: {%u,%u,%u,%u,%u}\n",
                  touchCal[0], touchCal[1], touchCal[2], touchCal[3], touchCal[4]);
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(kRed, kBg);
    tft.drawString("TOUCH CALIBRATION MISSING", 240, 130, 2);
    tft.setTextColor(kWhite, kBg);
    tft.drawString("Reflash Phase 0A and calibrate first.", 240, 165, 2);
    Serial.println("[TOUCH] ERROR: Phase 0A calibration missing.");
    return;
  }

  loadPetState();
  randomSeed(esp_random());
  bootSplash();
  drawPetScreen();

  const uint32_t now = millis();
  lastAnimMs = now;
  lastDecayMs = now;
  lastAutoSaveMs = now;
  nextIdleEventMs = now + random(kIdleEventMinMs, kIdleEventMaxMs);
}

void loop() {
  if (!touchReady) {
    delay(100);
    return;
  }

  const uint32_t now = millis();

  if (reaction != ReactionState::None && now >= reactionUntilMs) {
    reaction = ReactionState::None;
    forceGhostRedraw = true;
  }

  uint16_t x = 0, y = 0;
  if (now - lastTouchMs > 120 && tft.getTouch(&x, &y, 350)) {
    lastTouchMs = now;
    const int button = hitButton(x, y);

    if (button >= 0) {
      Serial.printf("[TOUCH] action x=%u y=%u button=%d\n", x, y, button);
      applyAction(button);
    } else if (pointInRect(x, y, kGhostHit)) {
      Serial.printf("[TOUCH] ghost x=%u y=%u\n", x, y);
      handleGhostTap();
    } else if (y < 42 && x > 350) {
      // State label doubles as a simple dormancy toggle for Phase 0C.
      toggleSleep();
    }
  }

  if (now - lastAnimMs >= kAnimMs) {
    lastAnimMs = now;
    if (!sleeping) {
      animFrame = (animFrame + 1) % specter::assets::kGhostIdleFrameCount;
    }
    drawGhostFrame(animFrame);
    forceGhostRedraw = false;
  } else if (forceGhostRedraw) {
    drawGhostFrame(animFrame);
    forceGhostRedraw = false;
  }

  if (now >= nextIdleEventMs) {
    runIdleEvent();
  }

  if (now - lastDecayMs >= kDecayTickMs) {
    lastDecayMs = now;
    decayPet();
  }

  if (now - lastAutoSaveMs >= kAutoSaveMs) {
    lastAutoSaveMs = now;
    savePetState();
  }

  delay(4);
}
