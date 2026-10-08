#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "board_config.h"
#include "ghost_idle_frames.h"

namespace {

TFT_eSPI tft;
TFT_eSprite stageSprite(&tft);
Preferences prefs;

constexpr uint16_t kBg        = TFT_BLACK;
constexpr uint16_t kPanel     = 0x0841;
constexpr uint16_t kStageBg   = 0x0862;
constexpr uint16_t kStageBg2  = 0x1083;
constexpr uint16_t kGrid      = 0x18C3;
constexpr uint16_t kChrome    = 0x2104;
constexpr uint16_t kCyan      = TFT_CYAN;
constexpr uint16_t kMagenta   = TFT_MAGENTA;
constexpr uint16_t kWhite     = TFT_WHITE;
constexpr uint16_t kDim       = 0x4208;
constexpr uint16_t kGreen     = TFT_GREEN;
constexpr uint16_t kYellow    = TFT_YELLOW;
constexpr uint16_t kRed       = TFT_RED;
constexpr uint16_t kBlueSoft  = 0x43DF;

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
  Dormant
};

enum class HabitatSpot : uint8_t {
  None,
  Random,
  DataTerminal,
  ChargeDock,
  SleepNode,
  Stabilizer
};

constexpr uint32_t kAnimMs = 260;
constexpr uint32_t kMoveMs = 50;
constexpr uint32_t kDecayTickMs = 30000;
constexpr uint32_t kAutoSaveMs = 30000;
constexpr uint32_t kReactionMs = 950;

constexpr int16_t kStageX = 16;
constexpr int16_t kStageY = 106;
constexpr int16_t kStageW = 448;
constexpr int16_t kStageH = 154;

constexpr int16_t kGhostW = specter::assets::kGhostWidth;
constexpr int16_t kGhostH = specter::assets::kGhostHeight;
constexpr int16_t kMinGhostX = kStageX + 6;
constexpr int16_t kMaxGhostX = kStageX + kStageW - kGhostW - 6;
constexpr int16_t kMinGhostY = kStageY + 4;
constexpr int16_t kMaxGhostY = kStageY + kStageH - kGhostH - 22;

constexpr int16_t kDefaultGhostX = 192;
constexpr int16_t kDefaultGhostY = 124;

// Habitat anchor points for Ghost top-left coordinates.
constexpr int16_t kDataGhostX = 52;
constexpr int16_t kDataGhostY = 124;
constexpr int16_t kChargeGhostX = 322;
constexpr int16_t kChargeGhostY = 142;
constexpr int16_t kSleepGhostX = 58;
constexpr int16_t kSleepGhostY = 146;
constexpr int16_t kStabilizeGhostX = 304;
constexpr int16_t kStabilizeGhostY = 118;

uint32_t lastAnimMs = 0;
uint32_t lastMoveMs = 0;
uint32_t lastDecayMs = 0;
uint32_t lastAutoSaveMs = 0;
uint32_t lastTouchMs = 0;
uint32_t reactionUntilMs = 0;
uint32_t pauseUntilMs = 0;

uint8_t animFrame = 0;
uint8_t animLoopIndex = 0;
bool facingRight = false;
bool sleeping = false;
bool forceGhostRedraw = true;
String statusText = "PATROL";
uint16_t statusColor = kDim;
ReactionState reaction = ReactionState::None;
MotionState motion = MotionState::Patrol;
HabitatSpot activeSpot = HabitatSpot::Random;

int16_t ghostX = kDefaultGhostX;
int16_t ghostY = kDefaultGhostY;
int16_t targetX = kDefaultGhostX;
int16_t targetY = kDefaultGhostY;

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

bool pointInRect(uint16_t x, uint16_t y, const Rect& r) {
  return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
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
  const bool exists = prefs.isKey("petstate0e");
  if (exists) {
    const size_t n = prefs.getBytes("petstate0e", &pet, sizeof(pet));
    prefs.end();
    if (n == sizeof(pet)) {
      Serial.println("[PET] Loaded Phase 0E state.");
      return true;
    }
    return false;
  }
  prefs.end();
  Serial.println("[PET] No Phase 0E state yet; using defaults.");
  return false;
}

void savePetState() {
  pet.saveCounter++;
  prefs.begin("specter", false);
  const size_t n = prefs.putBytes("petstate0e", &pet, sizeof(pet));
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
    tft.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2, 2);
  }
}

void drawStatusText() {
  // Status lives inside the off-screen stage buffer.
  // Mark the stage dirty instead of writing directly to the TFT.
  forceGhostRedraw = true;
}

void setStatusText(const char* msg, uint16_t color) {
  statusText = msg;
  statusColor = color;
  forceGhostRedraw = true;
}

uint16_t spotAccent(HabitatSpot spot) {
  switch (spot) {
    case HabitatSpot::DataTerminal: return kCyan;
    case HabitatSpot::ChargeDock: return kYellow;
    case HabitatSpot::SleepNode: return kBlueSoft;
    case HabitatSpot::Stabilizer: return kGreen;
    default: return kDim;
  }
}

bool spotActive(HabitatSpot spot) {
  return activeSpot == spot || sleeping && spot == HabitatSpot::SleepNode;
}

void drawStageFrameToSprite() {
  stageSprite.drawRoundRect(0, 0, kStageW, kStageH, 8, kChrome);
}

void drawTerminalPropToSprite() {
  const int x = 12;   // 28 - kStageX
  const int y = 22;   // 128 - kStageY
  const uint16_t accent = spotActive(HabitatSpot::DataTerminal) ? kCyan : 0x3A97;

  stageSprite.fillRoundRect(x, y, 50, 38, 4, kPanel);
  stageSprite.drawRoundRect(x, y, 50, 38, 4, accent);
  stageSprite.drawRect(x + 8, y + 8, 34, 15, accent);
  stageSprite.drawFastHLine(x + 12, y + 12, 18, accent);
  stageSprite.drawFastHLine(x + 12, y + 16, 12, accent);
  stageSprite.drawFastVLine(x + 24, y + 25, 5, accent);
  stageSprite.drawFastHLine(x + 17, y + 30, 14, accent);
  stageSprite.setTextDatum(TL_DATUM);
  stageSprite.setTextColor(kDim, kStageBg);
  stageSprite.drawString("DATA", x + 2, y + 42, 1);
}

void drawChargeDockPropToSprite() {
  const int x = 358;  // 374 - kStageX
  const int y = 78;   // 184 - kStageY
  const uint16_t accent = spotActive(HabitatSpot::ChargeDock) ? kYellow : 0x6B4D;

  stageSprite.fillRoundRect(x, y, 56, 26, 5, kPanel);
  stageSprite.drawRoundRect(x, y, 56, 26, 5, accent);
  stageSprite.drawFastHLine(x + 8, y + 18, 40, accent);
  stageSprite.drawFastVLine(x + 18, y + 7, 7, accent);
  stageSprite.drawFastVLine(x + 38, y + 7, 7, accent);
  stageSprite.drawLine(x + 26, y + 6, x + 22, y + 14, accent);
  stageSprite.drawLine(x + 22, y + 14, x + 30, y + 14, accent);
  stageSprite.drawLine(x + 30, y + 14, x + 26, y + 21, accent);
  stageSprite.setTextDatum(TR_DATUM);
  stageSprite.setTextColor(kDim, kStageBg);
  stageSprite.drawString("DOCK", x + 56, y + 32, 1);
}

void drawSleepNodePropToSprite() {
  const int x = 10;   // 26 - kStageX
  const int y = 90;   // 196 - kStageY
  const uint16_t accent = spotActive(HabitatSpot::SleepNode) ? kBlueSoft : 0x39C7;

  stageSprite.drawRoundRect(x, y, 54, 18, 8, accent);
  stageSprite.fillRoundRect(x + 3, y + 5, 48, 10, 6, kPanel);
  stageSprite.drawCircle(x + 17, y + 8, 6, accent);
  stageSprite.fillCircle(x + 20, y + 8, 5, kStageBg);
  stageSprite.drawCircle(x + 34, y + 9, 2, accent);
  stageSprite.setTextDatum(TL_DATUM);
  stageSprite.setTextColor(kDim, kStageBg);
  stageSprite.drawString("SLEEP", x + 2, y + 21, 1);
}

void drawStabilizerPropToSprite() {
  const int x = 374;  // 390 - kStageX
  const int y = 18;   // 124 - kStageY
  const uint16_t accent = spotActive(HabitatSpot::Stabilizer) ? kGreen : 0x3BE8;

  stageSprite.drawFastVLine(x + 16, y + 5, 34, accent);
  stageSprite.drawCircle(x + 16, y + 9, 9, accent);
  stageSprite.drawCircle(x + 16, y + 9, 4, accent);
  stageSprite.drawFastHLine(x + 8, y + 31, 17, accent);
  stageSprite.drawFastHLine(x + 4, y + 35, 25, accent);
  stageSprite.setTextDatum(TL_DATUM);
  stageSprite.setTextColor(kDim, kStageBg);
  stageSprite.drawString("STAB", x + 2, y + 41, 1);
}

void drawEnvironmentToSprite() {
  stageSprite.fillSprite(kStageBg);

  // Upper glow band.
  for (int i = 0; i < 10; ++i) {
    stageSprite.drawFastHLine(8, 8 + i, kStageW - 16,
                              (i % 2 == 0) ? kStageBg2 : kStageBg);
  }

  // Digital grid.
  for (int x = 18; x < kStageW - 10; x += 32) {
    stageSprite.drawFastVLine(x, 16, kStageH - 40, kGrid);
  }
  for (int y = 24; y < kStageH - 24; y += 20) {
    stageSprite.drawFastHLine(10, y, kStageW - 20, kGrid);
  }

  // Faint data-city bars.
  for (int i = 0; i < 8; ++i) {
    const int bx = 90 + i * 34;
    const int bh = 16 + (i % 3) * 10;
    stageSprite.drawRect(bx, 80 - bh, 14, bh, kChrome);
    stageSprite.drawRect(bx + 16, 86 - bh / 2, 10, bh / 2, kChrome);
  }

  drawTerminalPropToSprite();
  drawChargeDockPropToSprite();
  drawSleepNodePropToSprite();
  drawStabilizerPropToSprite();
  drawStageFrameToSprite();

  // Persistent status text lives inside the stage buffer.
  stageSprite.fillRect(104, 134, 240, 19, kStageBg);
  stageSprite.setTextDatum(MC_DATUM);
  stageSprite.setTextColor(statusColor, kStageBg);
  stageSprite.drawString(statusText, 224, 143, 2);
}

void drawGhostAtCurrentPosition() {
  // Render the whole habitat off-screen, then push it to the panel in one
  // transaction. This removes the visible erase/redraw cycle that caused
  // heavy flicker in Phase 0E.
  drawEnvironmentToSprite();

  int drawX = ghostX - kStageX;
  int drawY = ghostY - kStageY;

  if (!sleeping) {
    static const int8_t bob[4] = {0, -2, -4, -2};
    drawY += bob[animFrame % 4];
  }

  if (currentMoodState() == MoodState::Unstable && !sleeping) {
    drawX += ((millis() / 80) % 2 == 0) ? -3 : 3;
  }

  uint16_t aura = kCyan;
  if (currentMoodState() == MoodState::Happy) aura = kMagenta;
  if (currentMoodState() == MoodState::LowCharge) aura = kYellow;
  if (currentMoodState() == MoodState::Unstable) aura = kRed;
  if (currentMoodState() == MoodState::Sleepy) aura = kBlueSoft;

  const int auraCx = drawX + (kGhostW / 2);
  const int auraCy = drawY + kGhostH - 4;
  stageSprite.drawEllipse(auraCx, auraCy, 48, 7, kDim);
  stageSprite.drawEllipse(auraCx, auraCy, 36, 4, aura);

  const uint16_t* pixels = facingRight ? specter::assets::kGhostRightFrames[animFrame]
                                       : specter::assets::kGhostLeftFrames[animFrame];
  // TFT_eSprite's pushImage has no transparent-key overload. Composite
  // RGB565 words into RAM, leaving the habitat visible through keyed pixels.
  for (int y = 0; y < specter::assets::kGhostHeight; ++y) {
    for (int x = 0; x < specter::assets::kGhostWidth; ++x) {
      const uint16_t color = pgm_read_word(pixels + y * specter::assets::kGhostWidth + x);
      if (color != specter::assets::kGhostTransparent)
        stageSprite.drawPixel(drawX + x, drawY + y, color);
    }
  }

  if (sleeping) {
    stageSprite.setTextDatum(MC_DATUM);
    stageSprite.setTextColor(kBlueSoft, kStageBg);
    stageSprite.drawString("z", drawX + 100, drawY + 28, 2);
    stageSprite.drawString("Z", drawX + 113, drawY + 12, 2);
  }

  switch (reaction) {
    case ReactionState::TapHappy:
      stageSprite.setTextDatum(MC_DATUM);
      stageSprite.setTextColor(kMagenta, kStageBg);
      stageSprite.drawString("<3", drawX + 96, drawY + 18, 2);
      break;
    case ReactionState::TapAnnoyed:
      stageSprite.setTextDatum(MC_DATUM);
      stageSprite.setTextColor(kYellow, kStageBg);
      stageSprite.drawString("!", drawX + 98, drawY + 18, 2);
      break;
    case ReactionState::Data:
      stageSprite.drawFastHLine(drawX - 10, drawY + 53, 118, kCyan);
      break;
    case ReactionState::Play:
      stageSprite.drawCircle(auraCx, drawY + 48, 55, kMagenta);
      stageSprite.drawCircle(auraCx, drawY + 48, 61, kCyan);
      break;
    case ReactionState::Charge:
      for (int i = 0; i < 4; ++i) {
        stageSprite.drawFastVLine(drawX + 18 + i * 20, drawY + 2,
                                  12 + i * 3, kYellow);
      }
      break;
    case ReactionState::Stabilize:
      stageSprite.drawRect(drawX - 4, drawY - 4,
                           kGhostW + 8, kGhostH + 8, kGreen);
      break;
    default:
      break;
  }

  stageSprite.pushSprite(kStageX, kStageY);
}

void drawPetScreen() {
  tft.fillScreen(kBg);
  tft.drawRect(3, 3, 474, 314, kCyan);
  drawHeader();
  drawStats();
  drawButtons();
  drawGhostAtCurrentPosition();
}

void bootSplash() {
  tft.fillScreen(kBg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(kDim, kBg);
  tft.drawString("INITIALIZING SPECTRAL HABITAT", 240, 210, 2);

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
  tft.drawString("DIGITAL HABITAT ONLINE", 240, 148, 2);
  delay(650);
}

int hitButton(uint16_t x, uint16_t y) {
  for (int i = 0; i < 4; ++i) {
    if (pointInRect(x, y, kButtons[i])) return i;
  }
  return -1;
}

Rect currentGhostHitRect() {
  return Rect{
      static_cast<int16_t>(ghostX - 8),
      static_cast<int16_t>(ghostY - 8),
      static_cast<int16_t>(kGhostW + 16),
      static_cast<int16_t>(kGhostH + 16),
      "GHOST"};
}

void setTargetToSpot(HabitatSpot spot) {
  activeSpot = spot;
  switch (spot) {
    case HabitatSpot::DataTerminal:
      targetX = kDataGhostX;
      targetY = kDataGhostY;
      break;
    case HabitatSpot::ChargeDock:
      targetX = kChargeGhostX;
      targetY = kChargeGhostY;
      break;
    case HabitatSpot::SleepNode:
      targetX = kSleepGhostX;
      targetY = kSleepGhostY;
      break;
    case HabitatSpot::Stabilizer:
      targetX = kStabilizeGhostX;
      targetY = kStabilizeGhostY;
      break;
    case HabitatSpot::Random:
    case HabitatSpot::None:
    default:
      targetX = clamp16(random(kMinGhostX, kMaxGhostX + 1), kMinGhostX, kMaxGhostX);
      targetY = clamp16(random(kMinGhostY, kMaxGhostY + 1), kMinGhostY, kMaxGhostY);
      activeSpot = HabitatSpot::Random;
      break;
  }
  motion = MotionState::Patrol;
  Serial.printf("[MOVE] target=(%d,%d) spot=%d\n", targetX, targetY, static_cast<int>(activeSpot));
}

void choosePatrolTarget() {
  if (sleeping) {
    setTargetToSpot(HabitatSpot::SleepNode);
    return;
  }

  if (currentMoodState() == MoodState::LowCharge && random(0, 100) < 60) {
    setTargetToSpot(HabitatSpot::ChargeDock);
    return;
  }

  if (currentMoodState() == MoodState::Unstable && random(0, 100) < 60) {
    setTargetToSpot(HabitatSpot::Stabilizer);
    return;
  }

  if (random(0, 100) < 20) {
    const HabitatSpot spots[3] = {
        HabitatSpot::DataTerminal,
        HabitatSpot::ChargeDock,
        HabitatSpot::Stabilizer};
    setTargetToSpot(spots[random(0, 3)]);
    return;
  }

  setTargetToSpot(HabitatSpot::Random);

  if (currentMoodState() == MoodState::LowCharge) {
    targetX = clamp16(ghostX + random(-60, 61), kMinGhostX, kMaxGhostX);
    targetY = clamp16(ghostY + random(-30, 31), kMinGhostY, kMaxGhostY);
  }
}

int movementStep() {
  switch (currentMoodState()) {
    case MoodState::Happy: return 3;
    case MoodState::LowCharge: return 1;
    case MoodState::Unstable: return 2;
    case MoodState::Sleepy: return 2;
    default: return 2;
  }
}

bool nearTarget() {
  return abs(ghostX - targetX) <= 3 && abs(ghostY - targetY) <= 3;
}

void stepTowardTarget() {
  int step = movementStep();

  if (currentMoodState() == MoodState::Unstable && !sleeping) {
    if ((millis() / 250) % 5 == 0) {
      targetX = clamp16(targetX + random(-10, 11), kMinGhostX, kMaxGhostX);
      targetY = clamp16(targetY + random(-6, 7), kMinGhostY, kMaxGhostY);
    }
  }

  if (ghostX < targetX) {
    facingRight = true;
    ghostX = min<int16_t>(ghostX + step, targetX);
  } else if (ghostX > targetX) {
    facingRight = false;
    ghostX = max<int16_t>(ghostX - step, targetX);
  }

  if (ghostY < targetY) ghostY = min<int16_t>(ghostY + step, targetY);
  else if (ghostY > targetY) ghostY = max<int16_t>(ghostY - step, targetY);

  forceGhostRedraw = true;

  if (nearTarget()) {
    motion = MotionState::Pause;
    pauseUntilMs = millis() + random(900, 2500);
    Serial.printf("[MOVE] arrived=(%d,%d), pause\n", ghostX, ghostY);
  }
}

void updateMotion(uint32_t now) {
  if (sleeping) {
    motion = MotionState::Dormant;
    setTargetToSpot(HabitatSpot::SleepNode);
    stepTowardTarget();
    return;
  }

  if (motion == MotionState::Pause) {
    if (now >= pauseUntilMs) choosePatrolTarget();
    return;
  }

  stepTowardTarget();
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
      setTargetToSpot(HabitatSpot::DataTerminal);
      setStatusText("DATA SYNC", kCyan);
      Serial.println("[ACTION] DATA");
      break;

    case 1:
      pet.mood = clamp100(pet.mood + 15);
      pet.charge = clamp100(pet.charge - 3);
      pet.signal = clamp100(pet.signal - 1);
      pet.affection = clamp100(pet.affection + 3);
      startReaction(ReactionState::Play);
      setTargetToSpot(HabitatSpot::Random);
      setStatusText("PLAYING", kMagenta);
      Serial.println("[ACTION] PLAY");
      break;

    case 2:
      pet.charge = clamp100(pet.charge + 18);
      pet.mood = clamp100(pet.mood + 1);
      pet.affection = clamp100(pet.affection + 1);
      startReaction(ReactionState::Charge);
      setTargetToSpot(HabitatSpot::ChargeDock);
      setStatusText("CHARGING", kYellow);
      Serial.println("[ACTION] CHARGE");
      break;

    case 3:
      pet.integrity = clamp100(pet.integrity + 12);
      pet.signal = clamp100(pet.signal - 2);
      pet.affection = clamp100(pet.affection + 1);
      startReaction(ReactionState::Stabilize);
      setTargetToSpot(HabitatSpot::Stabilizer);
      setStatusText("STABILIZING", kGreen);
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

void handleGhostTap() {
  if (sleeping) {
    sleeping = false;
    pet.mood = clamp100(pet.mood + 2);
    startReaction(ReactionState::Wake);
    setStatusText("ONLINE", kCyan);
    choosePatrolTarget();
    Serial.println("[GHOST] Wake tap.");
  } else {
    pet.interactions++;
    const bool happyTap = ((pet.interactions % 4) != 0);
    if (happyTap) {
      pet.mood = clamp100(pet.mood + 3);
      pet.affection = clamp100(pet.affection + 2);
      startReaction(ReactionState::TapHappy);
      setStatusText("HELLO", kMagenta);
      Serial.println("[GHOST] Friendly tap.");
    } else {
      pet.mood = clamp100(pet.mood - 1);
      startReaction(ReactionState::TapAnnoyed);
      setStatusText("HEY", kYellow);
      Serial.println("[GHOST] Annoyed tap.");
    }
  }

  drawHeader();
  drawStats();
  savePetState();
}

void lureGhostToTouch(uint16_t x, uint16_t y) {
  if (sleeping) return;
  activeSpot = HabitatSpot::None;
  targetX = clamp16(static_cast<int>(x) - (kGhostW / 2), kMinGhostX, kMaxGhostX);
  targetY = clamp16(static_cast<int>(y) - (kGhostH / 2), kMinGhostY, kMaxGhostY);
  motion = MotionState::Patrol;
  setStatusText("SIGNAL PING", kCyan);
  Serial.printf("[MOVE] user lure target=(%d,%d)\n", targetX, targetY);
}

void toggleSleep() {
  sleeping = !sleeping;
  if (sleeping) {
    startReaction(ReactionState::SleepEnter);
    setStatusText("DORMANT", kBlueSoft);
    setTargetToSpot(HabitatSpot::SleepNode);
    Serial.println("[PET] Entering dormancy.");
  } else {
    startReaction(ReactionState::Wake);
    setStatusText("ONLINE", kCyan);
    choosePatrolTarget();
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

  drawHeader();
  drawStats();
  forceGhostRedraw = true;

  Serial.printf("[PET] signal=%u integrity=%u mood=%u charge=%u affection=%u sleep=%d\n",
                pet.signal, pet.integrity, pet.mood, pet.charge, pet.affection, sleeping ? 1 : 0);
}

void printBootInfo() {
  Serial.println();
  Serial.println("======================================");
  Serial.println(" SPECTER//OS - CYD4 PHASE 0E.1");
  Serial.println(" BUFFERED HABITAT FLICKER HOTFIX");
  Serial.println("======================================");
  Serial.printf("Chip: %s rev %d @ %d MHz\n",
                ESP.getChipModel(),
                ESP.getChipRevision(),
                ESP.getCpuFreqMHz());
  Serial.printf("Heap free: %u bytes\n", ESP.getFreeHeap());
  Serial.println("Display: ST7796S 480x320 landscape");
  Serial.println("Touch: XPT2046");
  Serial.println("Sprite byte swap: ENABLED");
  Serial.println("Directional flip: ENABLED");
  Serial.println("Animation loop: 0-1-2-1");
  Serial.println("Habitat layer: BUFFERED");
  Serial.println("Port: COM4");
}

}  // namespace

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

  // RGB332 halves the stage allocation (68,992 bytes plus a guard pixel),
  // fitting this ESP32's contiguous RAM without requiring PSRAM.
  stageSprite.setColorDepth(8);
  stageSprite.setSwapBytes(true);
  if (stageSprite.createSprite(kStageW, kStageH) == nullptr) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(kRed, kBg);
    tft.drawString("STAGE BUFFER ALLOC FAILED", 240, 130, 2);
    tft.setTextColor(kWhite, kBg);
    tft.drawString("Reboot and report serial heap.", 240, 165, 2);
    Serial.printf("[DISPLAY] ERROR: stage sprite allocation failed, heap=%u\n", ESP.getFreeHeap());
    return;
  }
  Serial.printf("[DISPLAY] Stage buffer allocated: %dx%d, heap=%u\n",
                kStageW, kStageH, ESP.getFreeHeap());

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
    setStatusText(sleeping ? "DORMANT" : "PATROL", sleeping ? kBlueSoft : kDim);
    if (!sleeping && motion == MotionState::Pause) {
      choosePatrolTarget();
    }
    forceGhostRedraw = true;
  }

  uint16_t x = 0;
  uint16_t y = 0;
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
      static const uint8_t kAnimSequence[4] = {0, 1, 2, 1};
      animLoopIndex = (animLoopIndex + 1) % 4;
      animFrame = kAnimSequence[animLoopIndex];
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
