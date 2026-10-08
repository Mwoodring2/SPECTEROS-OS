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

enum class MotionState : uint8_t {
  Patrol,
  Pause,
  ActionReturn,
  Dormant
};

constexpr uint32_t kAnimMs = 260;
constexpr uint32_t kMoveMs = 42;
constexpr uint32_t kDecayTickMs = 30000;
constexpr uint32_t kAutoSaveMs = 30000;
constexpr uint32_t kReactionMs = 900;

// The visible patrol field: between stats and action row.
constexpr int16_t kStageX = 16;
constexpr int16_t kStageY = 106;
constexpr int16_t kStageW = 448;
constexpr int16_t kStageH = 154;

// Sprite is 96x96. Use top-left coordinates.
constexpr int16_t kGhostW = specter::assets::kGhostWidth;
constexpr int16_t kGhostH = specter::assets::kGhostHeight;
constexpr int16_t kMinGhostX = kStageX + 6;
constexpr int16_t kMaxGhostX = kStageX + kStageW - kGhostW - 6;
constexpr int16_t kMinGhostY = kStageY + 4;
constexpr int16_t kMaxGhostY = kStageY + kStageH - kGhostH - 22;

// Center staging point for actions.
constexpr int16_t kCenterGhostX = 192;
constexpr int16_t kCenterGhostY = 124;

// Resting zone near bottom-left of stage.
constexpr int16_t kSleepGhostX = 52;
constexpr int16_t kSleepGhostY = 142;

uint32_t lastAnimMs = 0;
uint32_t lastMoveMs = 0;
uint32_t lastDecayMs = 0;
uint32_t lastAutoSaveMs = 0;
uint32_t lastTouchMs = 0;
uint32_t reactionUntilMs = 0;
uint32_t pauseUntilMs = 0;

uint8_t animFrame = 0;
bool sleeping = false;
bool forceGhostRedraw = true;
ReactionState reaction = ReactionState::None;
MotionState motion = MotionState::Patrol;

// Current and target sprite top-left coordinates.
int16_t ghostX = kCenterGhostX;
int16_t ghostY = kCenterGhostY;
int16_t targetX = kCenterGhostX;
int16_t targetY = kCenterGhostY;

// Last painted bounds for clean erase.
int16_t lastPaintX = ghostX;
int16_t lastPaintY = ghostY;
bool ghostPainted = false;

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

uint8_t clamp100(int value) {
  if (value < 0) return 0;
  if (value > 100) return 100;
  return static_cast<uint8_t>(value);
}

int16_t clamp16(int value, int16_t lo, int16_t hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return static_cast<int16_t>(value);
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
  const bool exists = prefs.isKey("petstate0d");
  if (exists) {
    const size_t n = prefs.getBytes("petstate0d", &pet, sizeof(pet));
    prefs.end();
    if (n == sizeof(pet)) {
      Serial.println("[PET] Loaded Phase 0D state.");
      return true;
    }
    return false;
  }
  prefs.end();
  Serial.println("[PET] No Phase 0D state yet; using defaults.");
  return false;
}

void savePetState() {
  pet.saveCounter++;
  prefs.begin("specter", false);
  const size_t n = prefs.putBytes("petstate0d", &pet, sizeof(pet));
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

void drawStageFrame() {
  tft.drawRoundRect(kStageX, kStageY, kStageW, kStageH, 8, 0x2104);
}

void drawReactionText(const char* msg, uint16_t color) {
  tft.fillRect(120, 240, 240, 19, kBg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, kBg);
  tft.drawString(msg, 240, 249, 2);
}

void clearOldGhost() {
  if (!ghostPainted) return;
  // Add margin for aura/effects.
  const int16_t x = max<int16_t>(kStageX + 1, lastPaintX - 18);
  const int16_t y = max<int16_t>(kStageY + 1, lastPaintY - 18);
  const int16_t x2 = min<int16_t>(kStageX + kStageW - 1, lastPaintX + kGhostW + 18);
  const int16_t y2 = min<int16_t>(kStageY + kStageH - 1, lastPaintY + kGhostH + 18);
  if (x2 > x && y2 > y) {
    tft.fillRect(x, y, x2 - x, y2 - y, kBg);
  }
}

void drawGhostAtCurrentPosition() {
  clearOldGhost();

  int drawX = ghostX;
  int drawY = ghostY;

  // Hover bob.
  if (!sleeping) {
    static const int8_t bob[4] = {0, -2, -4, -2};
    drawY += bob[animFrame % 4];
  }

  // Unstable Ghost gets a small lateral jitter.
  if (currentMoodState() == MoodState::Unstable && !sleeping) {
    drawX += ((millis() / 80) % 2 == 0) ? -3 : 3;
  }

  uint16_t aura = kCyan;
  if (currentMoodState() == MoodState::Happy) aura = kMagenta;
  if (currentMoodState() == MoodState::LowCharge) aura = kYellow;
  if (currentMoodState() == MoodState::Unstable) aura = kRed;
  if (currentMoodState() == MoodState::Sleepy) aura = kDim;

  const int auraCx = drawX + (kGhostW / 2);
  const int auraCy = drawY + kGhostH - 4;

  // Compact moving aura so Ghost can traverse the whole field.
  tft.drawEllipse(auraCx, auraCy, 48, 7, kDim);
  tft.drawEllipse(auraCx, auraCy, 36, 4, aura);

  const uint16_t* pixels = specter::assets::kGhostIdleFrames[animFrame];
  tft.pushImage(drawX, drawY,
                specter::assets::kGhostWidth,
                specter::assets::kGhostHeight,
                pixels,
                specter::assets::kGhostTransparent);

  if (sleeping) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(kDim, kBg);
    tft.drawString("z", drawX + 100, drawY + 28, 2);
    tft.drawString("Z", drawX + 113, drawY + 12, 2);
  }

  // Minimal reaction overlays that follow Ghost.
  switch (reaction) {
    case ReactionState::TapHappy:
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(kMagenta, kBg);
      tft.drawString("<3", drawX + 96, drawY + 18, 2);
      break;
    case ReactionState::TapAnnoyed:
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(kYellow, kBg);
      tft.drawString("!", drawX + 98, drawY + 18, 2);
      break;
    case ReactionState::Data:
      tft.drawFastHLine(drawX - 10, drawY + 53, 118, kCyan);
      break;
    case ReactionState::Play:
      tft.drawCircle(auraCx, drawY + 48, 55, kMagenta);
      tft.drawCircle(auraCx, drawY + 48, 61, kCyan);
      break;
    case ReactionState::Charge:
      for (int i = 0; i < 4; ++i) {
        tft.drawFastVLine(drawX + 18 + i*20, drawY + 2, 12 + i*3, kYellow);
      }
      break;
    case ReactionState::Stabilize:
      tft.drawRect(drawX - 4, drawY - 4, kGhostW + 8, kGhostH + 8, kGreen);
      break;
    default:
      break;
  }

  lastPaintX = ghostX;
  lastPaintY = ghostY;
  ghostPainted = true;

  // Repaint field border because a moving erase may touch it.
  drawStageFrame();
}

void drawPetScreen() {
  tft.fillScreen(kBg);
  tft.drawRect(3, 3, 474, 314, kCyan);
  drawHeader();
  drawStats();
  drawButtons();
  drawStageFrame();
  drawGhostAtCurrentPosition();
  drawReactionText("PATROL", kDim);
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

void choosePatrolTarget() {
  targetX = random(kMinGhostX, kMaxGhostX + 1);
  targetY = random(kMinGhostY, kMaxGhostY + 1);

  // Low charge favors shorter moves near current position.
  if (currentMoodState() == MoodState::LowCharge) {
    targetX = clamp16(ghostX + random(-60, 61), kMinGhostX, kMaxGhostX);
    targetY = clamp16(ghostY + random(-30, 31), kMinGhostY, kMaxGhostY);
  }

  // Happy Ghost uses the full field and changes altitude more.
  if (currentMoodState() == MoodState::Happy) {
    targetY = random(kMinGhostY, kMaxGhostY + 1);
  }

  motion = MotionState::Patrol;
  Serial.printf("[MOVE] target=(%d,%d)\n", targetX, targetY);
}

int movementStep() {
  switch (currentMoodState()) {
    case MoodState::Happy: return 3;
    case MoodState::LowCharge: return 1;
    case MoodState::Unstable: return 2;
    default: return 2;
  }
}

bool nearTarget() {
  return abs(ghostX - targetX) <= 3 && abs(ghostY - targetY) <= 3;
}

void stepTowardTarget() {
  if (sleeping) return;

  int step = movementStep();
  if (currentMoodState() == MoodState::Unstable) {
    // Slightly erratic destination perturbation.
    if ((millis() / 250) % 5 == 0) {
      targetX = clamp16(targetX + random(-10, 11), kMinGhostX, kMaxGhostX);
      targetY = clamp16(targetY + random(-6, 7), kMinGhostY, kMaxGhostY);
    }
  }

  if (ghostX < targetX) ghostX = min<int16_t>(ghostX + step, targetX);
  else if (ghostX > targetX) ghostX = max<int16_t>(ghostX - step, targetX);

  if (ghostY < targetY) ghostY = min<int16_t>(ghostY + step, targetY);
  else if (ghostY > targetY) ghostY = max<int16_t>(ghostY - step, targetY);

  forceGhostRedraw = true;

  if (nearTarget()) {
    motion = MotionState::Pause;
    pauseUntilMs = millis() + random(900, 2600);
    Serial.printf("[MOVE] arrived=(%d,%d), pause\n", ghostX, ghostY);
  }
}

void updateMotion(uint32_t now) {
  if (sleeping) {
    motion = MotionState::Dormant;
    targetX = kSleepGhostX;
    targetY = kSleepGhostY;
    if (ghostX != targetX || ghostY != targetY) {
      int step = 2;
      if (ghostX < targetX) ghostX = min<int16_t>(ghostX + step, targetX);
      else if (ghostX > targetX) ghostX = max<int16_t>(ghostX - step, targetX);
      if (ghostY < targetY) ghostY = min<int16_t>(ghostY + step, targetY);
      else if (ghostY > targetY) ghostY = max<int16_t>(ghostY - step, targetY);
      forceGhostRedraw = true;
    }
    return;
  }

  if (reaction != ReactionState::None) {
    // During action reactions, Ghost returns toward center and holds there.
    motion = MotionState::ActionReturn;
    targetX = kCenterGhostX;
    targetY = kCenterGhostY;
    stepTowardTarget();
    return;
  }

  if (motion == MotionState::Pause) {
    if (now >= pauseUntilMs) choosePatrolTarget();
    return;
  }

  if (motion == MotionState::ActionReturn && reaction == ReactionState::None) {
    choosePatrolTarget();
    return;
  }

  if (motion == MotionState::Patrol) {
    stepTowardTarget();
  }
}

void startReaction(ReactionState r) {
  reaction = r;
  reactionUntilMs = millis() + kReactionMs;
  motion = MotionState::ActionReturn;
  targetX = kCenterGhostX;
  targetY = kCenterGhostY;
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
      drawReactionText("DATA RECEIVED", kCyan);
      Serial.println("[ACTION] DATA");
      break;
    case 1:
      pet.mood = clamp100(pet.mood + 15);
      pet.charge = clamp100(pet.charge - 3);
      pet.signal = clamp100(pet.signal - 1);
      pet.affection = clamp100(pet.affection + 3);
      startReaction(ReactionState::Play);
      drawReactionText("PLAYING", kMagenta);
      Serial.println("[ACTION] PLAY");
      break;
    case 2:
      pet.charge = clamp100(pet.charge + 18);
      pet.mood = clamp100(pet.mood + 1);
      pet.affection = clamp100(pet.affection + 1);
      startReaction(ReactionState::Charge);
      drawReactionText("CHARGING", kYellow);
      Serial.println("[ACTION] CHARGE");
      break;
    case 3:
      pet.integrity = clamp100(pet.integrity + 12);
      pet.signal = clamp100(pet.signal - 2);
      pet.affection = clamp100(pet.affection + 1);
      startReaction(ReactionState::Stabilize);
      drawReactionText("STABLE", kGreen);
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

Rect currentGhostHitRect() {
  return Rect{static_cast<int16_t>(ghostX - 8),
              static_cast<int16_t>(ghostY - 8),
              static_cast<int16_t>(kGhostW + 16),
              static_cast<int16_t>(kGhostH + 16),
              "GHOST"};
}

void handleGhostTap() {
  if (sleeping) {
    sleeping = false;
    pet.mood = clamp100(pet.mood + 2);
    startReaction(ReactionState::Wake);
    drawReactionText("ONLINE", kCyan);
    Serial.println("[GHOST] Wake tap.");
  } else {
    pet.interactions++;
    const bool happyTap = ((pet.interactions % 4) != 0);
    if (happyTap) {
      pet.mood = clamp100(pet.mood + 3);
      pet.affection = clamp100(pet.affection + 2);
      startReaction(ReactionState::TapHappy);
      drawReactionText("HELLO", kMagenta);
      Serial.println("[GHOST] Friendly tap.");
    } else {
      pet.mood = clamp100(pet.mood - 1);
      startReaction(ReactionState::TapAnnoyed);
      drawReactionText("HEY", kYellow);
      Serial.println("[GHOST] Annoyed tap.");
    }
  }
  drawHeader();
  drawStats();
  savePetState();
}

void lureGhostToTouch(uint16_t x, uint16_t y) {
  if (sleeping) return;
  targetX = clamp16(static_cast<int>(x) - (kGhostW / 2), kMinGhostX, kMaxGhostX);
  targetY = clamp16(static_cast<int>(y) - (kGhostH / 2), kMinGhostY, kMaxGhostY);
  motion = MotionState::Patrol;
  drawReactionText("SIGNAL PING", kCyan);
  Serial.printf("[MOVE] user lure target=(%d,%d)\n", targetX, targetY);
}

void toggleSleep() {
  sleeping = !sleeping;
  if (sleeping) {
    reaction = ReactionState::SleepEnter;
    reactionUntilMs = millis() + kReactionMs;
    motion = MotionState::Dormant;
    targetX = kSleepGhostX;
    targetY = kSleepGhostY;
    drawReactionText("DORMANT", kDim);
    Serial.println("[PET] Entering dormancy.");
  } else {
    startReaction(ReactionState::Wake);
    drawReactionText("ONLINE", kCyan);
    Serial.println("[PET] Waking.");
  }
  drawHeader();
  forceGhostRedraw = true;
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
  Serial.println(" SPECTER//OS - CYD4 PHASE 0D");
  Serial.println(" AUTONOMOUS PATROL + MOVEMENT");
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
  choosePatrolTarget();

  const uint32_t now = millis();
  lastAnimMs = now;
  lastMoveMs = now;
  lastDecayMs = now;
  lastAutoSaveMs = now;
}

void loop() {
  if (!touchReady) {
    delay(100);
    return;
  }

  const uint32_t now = millis();

  if (reaction != ReactionState::None && now >= reactionUntilMs) {
    reaction = ReactionState::None;
    drawReactionText(sleeping ? "DORMANT" : "PATROL", sleeping ? kDim : kCyan);
    if (!sleeping) choosePatrolTarget();
    forceGhostRedraw = true;
  }

  uint16_t x = 0, y = 0;
  if (now - lastTouchMs > 120 && tft.getTouch(&x, &y, 350)) {
    lastTouchMs = now;
    const int button = hitButton(x, y);
    const Rect ghostHit = currentGhostHitRect();

    if (button >= 0) {
      Serial.printf("[TOUCH] action x=%u y=%u button=%d\n", x, y, button);
      applyAction(button);
    } else if (pointInRect(x, y, ghostHit)) {
      Serial.printf("[TOUCH] ghost x=%u y=%u\n", x, y);
      handleGhostTap();
    } else if (y < 42 && x > 350) {
      toggleSleep();
    } else if (x >= kStageX && x < kStageX + kStageW &&
               y >= kStageY && y < kStageY + kStageH) {
      lureGhostToTouch(x, y);
    }
  }

  if (now - lastMoveMs >= kMoveMs) {
    lastMoveMs = now;
    updateMotion(now);
  }

  if (now - lastAnimMs >= kAnimMs) {
    lastAnimMs = now;
    if (!sleeping) {
      animFrame = (animFrame + 1) % specter::assets::kGhostIdleFrameCount;
    }
    forceGhostRedraw = true;
  }

  if (forceGhostRedraw) {
    drawGhostAtCurrentPosition();
    forceGhostRedraw = false;
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
