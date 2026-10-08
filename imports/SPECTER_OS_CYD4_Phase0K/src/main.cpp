#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "board_config.h"
#include "ghost_idle_frames.h"
#include "ghost_layer_frames.h"
#include "layered_sprite_compositor.h"

namespace {
enum class HabitatSpot : uint8_t;
enum class ReactionState : uint8_t;
void choosePatrolTarget();
void beginHabitatInteraction(HabitatSpot spot);
void startReaction(ReactionState r);

TFT_eSPI tft;
TFT_eSprite stageSprite(&tft);
void compositeFrame(int x0, int y0, int w, int h, const uint16_t* pixels, uint16_t key) {
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const uint16_t color = pgm_read_word(pixels + y * w + x);
      if (color != key) stageSprite.drawPixel(x0 + x, y0 + y, color);
    }
}
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

enum class HabitatInteraction : uint8_t {
  None,
  DataSync,
  Charging,
  Stabilizing,
  Sleeping
};

enum class AutonomousNeed : uint8_t {
  None,
  Data,
  Charge,
  Stabilize,
  Play,
  Sleep
};

enum class IdlePersonality : uint8_t {
  None,
  Scan,
  Chirp,
  Glitch,
  Observe
};

enum class BodyMotion : uint8_t { None, Data, Charge, Stabilize, Play, Observe, Chirp, Sleep };
struct BodyPose { int8_t x; int8_t y; uint8_t frame; };
struct MotionKey { uint16_t atMs; BodyPose pose; };

constexpr uint32_t kAnimMs = 260;
constexpr uint32_t kMoveMs = 50;
constexpr uint32_t kDecayTickMs = 30000;
constexpr uint32_t kAutoSaveMs = 30000;
constexpr uint32_t kReactionMs = 950;
constexpr uint32_t kHabitatInteractionMs = 2600;
constexpr uint32_t kHabitatPulseMs = 180;
constexpr uint32_t kTeleportMs = 420;
constexpr int16_t kTeleportThreshold = 100;

// Phase 0K motion timelines. Scale never changes; only body frame + small offsets.
constexpr MotionKey kDataMotion[] = {
 {0,{0,0,0}}, {110,{2,-1,1}}, {220,{4,-2,2}}, {610,{4,-2,2}}, {790,{2,-1,1}}, {950,{0,0,0}} };
constexpr MotionKey kChargeMotion[] = {
 {0,{0,0,0}}, {120,{-2,1,1}}, {250,{-4,2,2}}, {520,{-4,1,2}}, {690,{-1,-2,1}}, {950,{0,0,0}} };
constexpr MotionKey kStabilizeMotion[] = {
 {0,{0,0,0}}, {130,{0,2,1}}, {280,{0,-3,2}}, {530,{0,-5,2}}, {720,{0,-2,1}}, {950,{0,0,0}} };
constexpr MotionKey kPlayMotion[] = {
 {0,{0,0,0}}, {100,{0,3,1}}, {210,{2,-5,2}}, {350,{-2,-8,1}}, {500,{3,-4,2}}, {690,{-1,2,1}}, {950,{0,0,0}} };
constexpr MotionKey kObserveMotion[] = {
 {0,{0,0,0}}, {180,{2,-1,1}}, {420,{3,-2,2}}, {1100,{3,-2,2}}, {1450,{1,-1,1}}, {1700,{0,0,0}} };
constexpr MotionKey kChirpMotion[] = {
 {0,{0,0,0}}, {140,{0,-4,1}}, {280,{2,-7,2}}, {430,{-2,-3,1}}, {620,{0,0,0}}, {900,{0,-2,1}}, {1200,{0,0,0}}, {1700,{0,0,0}} };
constexpr MotionKey kSleepMotion[] = {
 {0,{0,0,0}}, {400,{0,1,0}}, {800,{0,2,0}}, {1200,{0,1,0}}, {1600,{0,0,0}} };

// Phase 0G autonomous routine.
constexpr uint32_t kAutonomyPollMs = 4000;
constexpr uint32_t kAutonomyUserGraceMs = 7000;
constexpr uint32_t kAutonomyCooldownMs = 9000;
constexpr uint32_t kAutonomousNapIdleMs = 60000;
constexpr uint32_t kAutonomousNapDurationMs = 15000;

// Phase 0H ambient/personality cadence.
constexpr uint32_t kAmbientFrameMs = 220;
constexpr uint32_t kIdlePersonalityPollMs = 7000;
constexpr uint32_t kIdlePersonalityDurationMs = 1700;
constexpr uint32_t kIdlePersonalityCooldownMs = 10000;

constexpr uint8_t kNeedSignal = 22;
constexpr uint8_t kNeedIntegrity = 25;
constexpr uint8_t kNeedMood = 30;
constexpr uint8_t kNeedCharge = 20;
constexpr uint8_t kCriticalSignal = 10;
constexpr uint8_t kCriticalIntegrity = 12;
constexpr uint8_t kCriticalCharge = 8;

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
HabitatInteraction habitatInteraction = HabitatInteraction::None;
uint32_t habitatInteractionUntilMs = 0;
uint32_t lastHabitatPulseMs = 0;
uint8_t habitatPulseFrame = 0;
bool teleportActive = false;
uint32_t teleportUntilMs = 0;
HabitatSpot teleportSpot = HabitatSpot::None;
int16_t teleportFromX = 0;
int16_t teleportFromY = 0;
int16_t teleportToX = 0;
int16_t teleportToY = 0;

AutonomousNeed autonomousNeed = AutonomousNeed::None;
uint32_t lastAutonomyPollMs = 0;
uint32_t autonomyCooldownUntilMs = 0;
uint32_t lastUserInteractionMs = 0;
bool autonomousSleepRequested = false;
uint32_t autonomousWakeAtMs = 0;

// Ambient habitat + idle personality state.
uint32_t lastAmbientFrameMs = 0;
uint8_t ambientFrame = 0;
uint32_t lastIdlePersonalityPollMs = 0;
uint32_t idlePersonalityUntilMs = 0;
uint32_t idlePersonalityCooldownUntilMs = 0;
IdlePersonality idlePersonality = IdlePersonality::None;
uint8_t validationIdleIndex = 0;

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
  {124, 266, 112, 46, "CHARGE"    },
  {240, 266, 112, 46, "STABILIZE" },
  {356, 266, 116, 46, "PLAY"      },
};

constexpr Rect kSleepNodeTouch = {
  20, 188, 74, 42, "SLEEP NODE"
};

constexpr Rect kHeaderSleepTouch = {
  330, 0, 150, 44, "STATUS"
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
    uint16_t buttonColor = kCyan;
    if (i == 2) buttonColor = kGreen;
    if (i == 3) buttonColor = kMagenta;
    tft.setTextColor(buttonColor, kPanel);
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
  return activeSpot == spot ||
         (sleeping && spot == HabitatSpot::SleepNode) ||
         (spot == HabitatSpot::SleepNode && activeSpot == HabitatSpot::SleepNode);
}


bool habitatInteractionActive() {
  return habitatInteraction != HabitatInteraction::None;
}

const char* habitatInteractionLabel() {
  switch (habitatInteraction) {
    case HabitatInteraction::DataSync: return "SYNCING";
    case HabitatInteraction::Charging: return "DOCKED";
    case HabitatInteraction::Stabilizing: return "REPAIRING";
    case HabitatInteraction::Sleeping: return "DORMANT";
    default: return "PATROL";
  }
}

uint16_t habitatInteractionColor() {
  switch (habitatInteraction) {
    case HabitatInteraction::DataSync: return kCyan;
    case HabitatInteraction::Charging: return kYellow;
    case HabitatInteraction::Stabilizing: return kGreen;
    case HabitatInteraction::Sleeping: return kBlueSoft;
    default: return kDim;
  }
}


const char* autonomousNeedLabel(AutonomousNeed need) {
  switch (need) {
    case AutonomousNeed::Data: return "SEEKING DATA";
    case AutonomousNeed::Charge: return "SEEKING CHARGE";
    case AutonomousNeed::Stabilize: return "SEEKING STABILITY";
    case AutonomousNeed::Play: return "SELF PLAY";
    case AutonomousNeed::Sleep: return "SEEKING REST";
    default: return "PATROL";
  }
}

uint16_t autonomousNeedColor(AutonomousNeed need) {
  switch (need) {
    case AutonomousNeed::Data: return kCyan;
    case AutonomousNeed::Charge: return kYellow;
    case AutonomousNeed::Stabilize: return kGreen;
    case AutonomousNeed::Play: return kMagenta;
    case AutonomousNeed::Sleep: return kBlueSoft;
    default: return kDim;
  }
}

bool autonomyBusy() {
  return teleportActive ||
         habitatInteractionActive() ||
         reaction != ReactionState::None ||
         sleeping;
}


bool idlePersonalityBusy() {
  return idlePersonality != IdlePersonality::None;
}

const char* idlePersonalityLabel() {
  switch (idlePersonality) {
    case IdlePersonality::Scan: return "SCANNING";
    case IdlePersonality::Chirp: return "CHIRP";
    case IdlePersonality::Glitch: return "GLITCH";
    case IdlePersonality::Observe: return "OBSERVING";
    default: return "PATROL";
  }
}

uint16_t idlePersonalityColor() {
  switch (idlePersonality) {
    case IdlePersonality::Scan: return kCyan;
    case IdlePersonality::Chirp: return kMagenta;
    case IdlePersonality::Glitch: return kYellow;
    case IdlePersonality::Observe: return kBlueSoft;
    default: return kDim;
  }
}




void drawStageFrameToSprite() {
  stageSprite.drawRoundRect(0, 0, kStageW, kStageH, 8, kChrome);
}

void drawTerminalPropToSprite() {
  const int x = 12;   // 28 - kStageX
  const int y = 22;   // 128 - kStageY
  const uint16_t accent = spotActive(HabitatSpot::DataTerminal) ? kCyan : 0x3A97;
  const bool pulsing = habitatInteraction == HabitatInteraction::DataSync;

  stageSprite.fillRoundRect(x, y, 50, 38, 4, kPanel);
  stageSprite.drawRoundRect(x, y, 50, 38, 4, accent);
  stageSprite.drawRect(x + 8, y + 8, 34, 15, accent);
  stageSprite.drawFastHLine(x + 12, y + 12, pulsing ? 26 : 18, accent);
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
  const bool pulsing = habitatInteraction == HabitatInteraction::Charging;

  stageSprite.fillRoundRect(x, y, 56, 26, 5, kPanel);
  stageSprite.drawRoundRect(x, y, 56, 26, 5, accent);
  stageSprite.drawFastHLine(x + 8, y + 18, pulsing ? 48 : 40, accent);
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
  const bool pulsing = habitatInteraction == HabitatInteraction::Sleeping;

  stageSprite.drawRoundRect(x, y, 54, 18, 8, accent);
  stageSprite.fillRoundRect(x + 3, y + 5, 48, 10, 6, kPanel);
  stageSprite.drawCircle(x + 17, y + 8, 6, accent);
  stageSprite.fillCircle(x + 20, y + 8, 5, kStageBg);
  stageSprite.drawCircle(x + 34, y + 9, pulsing ? 4 : 2, accent);
  stageSprite.setTextDatum(TL_DATUM);
  stageSprite.setTextColor(kDim, kStageBg);
  stageSprite.drawString(sleeping ? "WAKE" : "SLEEP", x + 2, y + 21, 1);
}

void drawStabilizerPropToSprite() {
  const int x = 374;  // 390 - kStageX
  const int y = 18;   // 124 - kStageY
  const uint16_t accent = spotActive(HabitatSpot::Stabilizer) ? kGreen : 0x3BE8;
  const bool pulsing = habitatInteraction == HabitatInteraction::Stabilizing;

  stageSprite.drawFastVLine(x + 16, y + 5, 34, accent);
  stageSprite.drawCircle(x + 16, y + 9, pulsing ? 12 : 9, accent);
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

  // Phase 0H ambient life: sparse digital motes and a slow scanline.
  // All drawn inside the accepted off-screen habitat buffer.
  const int scanY = 18 + ((ambientFrame * 7) % (kStageH - 40));
  stageSprite.drawFastHLine(8, scanY, kStageW - 16, 0x18E4);

  for (int i = 0; i < 6; ++i) {
    const int px = 22 + ((i * 71 + ambientFrame * (i + 2) * 3) % (kStageW - 44));
    const int py = 18 + ((i * 29 + ambientFrame * (i + 3) * 2) % (kStageH - 46));
    const uint16_t pc = (i % 3 == 0) ? kCyan : ((i % 3 == 1) ? kMagenta : kDim);
    stageSprite.drawPixel(px, py, pc);
    if ((ambientFrame + i) % 4 == 0) {
      stageSprite.drawPixel(px + 1, py, pc);
    }
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


uint8_t sequenceFrame(uint8_t count, uint32_t durationMs, uint32_t untilMs) {
  if (count <= 1 || durationMs == 0) return 0;
  const uint32_t now = millis();
  const uint32_t remaining = (untilMs > now) ? (untilMs - now) : 0;
  const uint32_t elapsed = (remaining >= durationMs) ? 0 : (durationMs - remaining);
  uint32_t idx = (elapsed * count) / durationMs;
  if (idx >= count) idx = count - 1;
  return static_cast<uint8_t>(idx);
}

void drawBodyFrame(int ghostTopLeftX, int ghostTopLeftY, uint8_t bodyFrame) {
  if (bodyFrame >= specter::assets::kGhostFacingFrameCount) bodyFrame = 0;
  const uint16_t* pixels = facingRight
      ? specter::assets::kGhostRightFrames[bodyFrame]
      : specter::assets::kGhostLeftFrames[bodyFrame];
  compositeFrame(ghostTopLeftX, ghostTopLeftY,
                        specter::assets::kGhostWidth,
                        specter::assets::kGhostHeight, pixels,
                        specter::assets::kGhostTransparent);
}

void drawPatrolBody(int ghostTopLeftX, int ghostTopLeftY) {
  drawBodyFrame(ghostTopLeftX, ghostTopLeftY, animFrame);
}

void drawRestBody(int ghostTopLeftX, int ghostTopLeftY) {
  const uint16_t* pixels = facingRight
      ? specter::layers::kBodyRestRight0
      : specter::layers::kBodyRestLeft0;

  compositeFrame(ghostTopLeftX, ghostTopLeftY,
                        specter::layers::kBodyWidth,
                        specter::layers::kBodyHeight,
                        pixels,
                        specter::layers::kTransparent);
}

void drawOverlayFrame(const uint16_t* frame,
                      int ghostTopLeftX,
                      int ghostTopLeftY,
                      int16_t offsetX = 0,
                      int16_t offsetY = 0) {
  const int fxX = ghostTopLeftX + specter::layers::kBodyAnchorX
      - specter::layers::kOverlayAnchorX + offsetX;
  const int fxY = ghostTopLeftY + specter::layers::kBodyAnchorY
      - specter::layers::kOverlayAnchorY + offsetY;

  compositeFrame(fxX, fxY,
                        specter::layers::kOverlayWidth,
                        specter::layers::kOverlayHeight,
                        frame,
                        specter::layers::kTransparent);
}

void drawReplacementFrame(const uint16_t* frame, int ghostTopLeftX, int ghostTopLeftY) {
  drawOverlayFrame(frame, ghostTopLeftX, ghostTopLeftY);
}

template <size_t N>
BodyPose sampleMotion(const MotionKey (&keys)[N], uint32_t elapsedMs) {
  BodyPose pose = keys[0].pose;
  for (size_t i=1;i<N;++i) { if (elapsedMs < keys[i].atMs) break; pose = keys[i].pose; }
  return pose;
}
uint32_t elapsedFromUntil(uint32_t durationMs, uint32_t untilMs) {
  const uint32_t now=millis();
  const uint32_t remaining=(untilMs>now)?(untilMs-now):0;
  return remaining>=durationMs?0:durationMs-remaining;
}
BodyMotion currentBodyMotion() {
  if (habitatInteraction==HabitatInteraction::DataSync || reaction==ReactionState::Data) return BodyMotion::Data;
  if (habitatInteraction==HabitatInteraction::Charging || reaction==ReactionState::Charge) return BodyMotion::Charge;
  if (habitatInteraction==HabitatInteraction::Stabilizing || reaction==ReactionState::Stabilize) return BodyMotion::Stabilize;
  if (reaction==ReactionState::Play || autonomousNeed==AutonomousNeed::Play) return BodyMotion::Play;
  if (idlePersonality==IdlePersonality::Observe || idlePersonality==IdlePersonality::Scan) return BodyMotion::Observe;
  if (idlePersonality==IdlePersonality::Chirp) return BodyMotion::Chirp;
  if (sleeping || habitatInteraction==HabitatInteraction::Sleeping) return BodyMotion::Sleep;
  return BodyMotion::None;
}
BodyPose currentBodyPose() {
  switch (currentBodyMotion()) {
    case BodyMotion::Data: { uint32_t e=habitatInteraction==HabitatInteraction::DataSync?elapsedFromUntil(kHabitatInteractionMs,habitatInteractionUntilMs):elapsedFromUntil(kReactionMs,reactionUntilMs); return sampleMotion(kDataMotion,e%951); }
    case BodyMotion::Charge: { uint32_t e=habitatInteraction==HabitatInteraction::Charging?elapsedFromUntil(kHabitatInteractionMs,habitatInteractionUntilMs):elapsedFromUntil(kReactionMs,reactionUntilMs); return sampleMotion(kChargeMotion,e%951); }
    case BodyMotion::Stabilize: { uint32_t e=habitatInteraction==HabitatInteraction::Stabilizing?elapsedFromUntil(kHabitatInteractionMs,habitatInteractionUntilMs):elapsedFromUntil(kReactionMs,reactionUntilMs); return sampleMotion(kStabilizeMotion,e%951); }
    case BodyMotion::Play: { uint32_t e=elapsedFromUntil(kReactionMs,reactionUntilMs); return sampleMotion(kPlayMotion,e%951); }
    case BodyMotion::Observe: { uint32_t e=elapsedFromUntil(kIdlePersonalityDurationMs,idlePersonalityUntilMs); return sampleMotion(kObserveMotion,e); }
    case BodyMotion::Chirp: { uint32_t e=elapsedFromUntil(kIdlePersonalityDurationMs,idlePersonalityUntilMs); return sampleMotion(kChirpMotion,e); }
    case BodyMotion::Sleep: return sampleMotion(kSleepMotion,millis()%1601);
    default: return {0,0,animFrame};
  }
}

forge::sprite::LayerPlan currentLayerPlan() {
  forge::sprite::LayerPlan plan;

  if (teleportActive) {
    plan.replacement = forge::sprite::ReplacementMode::Teleport;
    return plan;
  }

  if (idlePersonality == IdlePersonality::Glitch) {
    plan.replacement = forge::sprite::ReplacementMode::Dissolve;
    return plan;
  }

  if (habitatInteraction == HabitatInteraction::Sleeping || sleeping) {
    plan.body = forge::sprite::BodyMode::Rest;
  }

  if (habitatInteraction == HabitatInteraction::DataSync ||
      reaction == ReactionState::Data ||
      idlePersonality == IdlePersonality::Scan ||
      idlePersonality == IdlePersonality::Observe) {
    plan.fx = forge::sprite::FxMode::Scan;
  } else if (habitatInteraction == HabitatInteraction::Charging ||
             reaction == ReactionState::Charge) {
    plan.fx = forge::sprite::FxMode::Charge;
  } else if (habitatInteraction == HabitatInteraction::Stabilizing ||
             reaction == ReactionState::Stabilize) {
    plan.fx = forge::sprite::FxMode::Stabilize;
  } else if (reaction == ReactionState::Play ||
             autonomousNeed == AutonomousNeed::Play) {
    plan.fx = forge::sprite::FxMode::Play;
  } else if (idlePersonality == IdlePersonality::Chirp) {
    plan.fx = forge::sprite::FxMode::Chirp;
  }

  return plan;
}

void drawGhostAtCurrentPosition() {
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

  const forge::sprite::LayerPlan plan = currentLayerPlan();

  // Full replacement sequences own the character completely.
  if (plan.replacement == forge::sprite::ReplacementMode::Teleport) {
    const uint32_t now = millis();
    const uint32_t remaining = (teleportUntilMs > now) ? (teleportUntilMs - now) : 0;
    const bool arrivalPhase = remaining < (kTeleportMs / 2);
    const int rx = (arrivalPhase ? teleportToX : teleportFromX) - kStageX;
    const int ry = (arrivalPhase ? teleportToY : teleportFromY) - kStageY;
    const uint8_t idx = sequenceFrame(
        (facingRight ? specter::layers::kReplaceTeleportRightCount : specter::layers::kReplaceTeleportLeftCount),
        kTeleportMs,
        teleportUntilMs);
    drawReplacementFrame((facingRight ? specter::layers::kReplaceTeleportRightFrames[idx] : specter::layers::kReplaceTeleportLeftFrames[idx]), rx, ry);
    stageSprite.pushSprite(kStageX, kStageY);
    return;
  }

  if (plan.replacement == forge::sprite::ReplacementMode::Dissolve) {
    const uint8_t idx = sequenceFrame(
        (facingRight ? specter::layers::kReplaceDissolveRightCount : specter::layers::kReplaceDissolveLeftCount),
        kIdlePersonalityDurationMs,
        idlePersonalityUntilMs);
    drawReplacementFrame((facingRight ? specter::layers::kReplaceDissolveRightFrames[idx] : specter::layers::kReplaceDissolveLeftFrames[idx]), drawX, drawY);
    stageSprite.pushSprite(kStageX, kStageY);
    return;
  }

  // Phase 0K body motion: fixed scale, fixed canonical anchor, timed pose offsets.
  const BodyPose bodyPose = currentBodyPose();
  const int bodyX = drawX + (facingRight ? bodyPose.x : -bodyPose.x);
  const int bodyY = drawY + bodyPose.y;
  if (plan.body == forge::sprite::BodyMode::Rest) drawRestBody(bodyX, bodyY);
  else drawBodyFrame(bodyX, bodyY, bodyPose.frame);

  // FX-only overlay layer. These frames contain NO central Ghost body.
  switch (plan.fx) {
    case forge::sprite::FxMode::Scan: {
      uint8_t idx = 0;
      if (habitatInteraction == HabitatInteraction::DataSync) {
        idx = (habitatPulseFrame / 2) % (facingRight ? specter::layers::kFxScanRightCount : specter::layers::kFxScanLeftCount);
      } else if (reaction == ReactionState::Data) {
        idx = sequenceFrame((facingRight ? specter::layers::kFxScanRightCount : specter::layers::kFxScanLeftCount), kReactionMs, reactionUntilMs);
      } else {
        idx = sequenceFrame(
            (facingRight ? specter::layers::kFxScanRightCount : specter::layers::kFxScanLeftCount),
            kIdlePersonalityDurationMs,
            idlePersonalityUntilMs);
      }
      drawOverlayFrame((facingRight ? specter::layers::kFxScanRightFrames[idx] : specter::layers::kFxScanLeftFrames[idx]), bodyX, bodyY,
                       0,
                       0);
      break;
    }

    case forge::sprite::FxMode::Charge: {
      uint8_t idx = 0;
      if (habitatInteraction == HabitatInteraction::Charging) {
        idx = (habitatPulseFrame / 2) % (facingRight ? specter::layers::kFxChargeRightCount : specter::layers::kFxChargeLeftCount);
      } else {
        idx = sequenceFrame((facingRight ? specter::layers::kFxChargeRightCount : specter::layers::kFxChargeLeftCount), kReactionMs, reactionUntilMs);
      }
      drawOverlayFrame((facingRight ? specter::layers::kFxChargeRightFrames[idx] : specter::layers::kFxChargeLeftFrames[idx]), bodyX, bodyY,
                       0,
                       0);
      break;
    }

    case forge::sprite::FxMode::Stabilize: {
      const uint8_t idx = habitatInteraction == HabitatInteraction::Stabilizing
          ? (habitatPulseFrame / 2) % (facingRight ? specter::layers::kFxPlayRightCount : specter::layers::kFxPlayLeftCount)
          : sequenceFrame((facingRight ? specter::layers::kFxPlayRightCount : specter::layers::kFxPlayLeftCount), kReactionMs, reactionUntilMs);
      drawOverlayFrame((facingRight ? specter::layers::kFxPlayRightFrames[idx] : specter::layers::kFxPlayLeftFrames[idx]), bodyX, bodyY,
                       0,
                       0);
      stageSprite.drawCircle(bodyX + (kGhostW/2), bodyY + 48, 52, kGreen);
      break;
    }

    case forge::sprite::FxMode::Play: {
      const uint8_t idx = sequenceFrame(
          (facingRight ? specter::layers::kFxPlayRightCount : specter::layers::kFxPlayLeftCount), kReactionMs, reactionUntilMs);
      drawOverlayFrame((facingRight ? specter::layers::kFxPlayRightFrames[idx] : specter::layers::kFxPlayLeftFrames[idx]), bodyX, bodyY,
                       0,
                       0);
      break;
    }

    case forge::sprite::FxMode::Chirp: {
      const uint8_t idx = sequenceFrame(
          (facingRight ? specter::layers::kFxPlayRightCount : specter::layers::kFxPlayLeftCount),
          kIdlePersonalityDurationMs,
          idlePersonalityUntilMs);
      drawOverlayFrame((facingRight ? specter::layers::kFxPlayRightFrames[idx] : specter::layers::kFxPlayLeftFrames[idx]), bodyX, bodyY,
                       0,
                       0);
      break;
    }

    default:
      break;
  }

  // Small expressive UI accents stay above the composite.
  if (reaction == ReactionState::TapHappy) {
    stageSprite.setTextDatum(MC_DATUM);
    stageSprite.setTextColor(kMagenta, kStageBg);
    stageSprite.drawString("<3", bodyX + 96, bodyY + 18, 2);
  } else if (reaction == ReactionState::TapAnnoyed) {
    stageSprite.setTextDatum(MC_DATUM);
    stageSprite.setTextColor(kYellow, kStageBg);
    stageSprite.drawString("!", bodyX + 98, bodyY + 18, 2);
  }

  if (plan.body == forge::sprite::BodyMode::Rest) {
    stageSprite.setTextDatum(MC_DATUM);
    stageSprite.setTextColor(kBlueSoft, kStageBg);
    stageSprite.drawString("Z", bodyX + 108, bodyY + 14, 2);
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


void getSpotCoordinates(HabitatSpot spot, int16_t& outX, int16_t& outY) {
  switch (spot) {
    case HabitatSpot::DataTerminal:
      outX = kDataGhostX;
      outY = kDataGhostY;
      break;
    case HabitatSpot::ChargeDock:
      outX = kChargeGhostX;
      outY = kChargeGhostY;
      break;
    case HabitatSpot::SleepNode:
      outX = kSleepGhostX;
      outY = kSleepGhostY;
      break;
    case HabitatSpot::Stabilizer:
      outX = kStabilizeGhostX;
      outY = kStabilizeGhostY;
      break;
    default:
      outX = ghostX;
      outY = ghostY;
      break;
  }
}


void faceTowardSpot(HabitatSpot spot) {
  switch (spot) {
    case HabitatSpot::DataTerminal:
    case HabitatSpot::SleepNode:
      facingRight = false;
      break;
    case HabitatSpot::ChargeDock:
    case HabitatSpot::Stabilizer:
      facingRight = true;
      break;
    default:
      break;
  }
}


int16_t distanceToSpot(HabitatSpot spot) {
  int16_t sx = ghostX;
  int16_t sy = ghostY;
  getSpotCoordinates(spot, sx, sy);
  const int dx = sx - ghostX;
  const int dy = sy - ghostY;
  return static_cast<int16_t>(sqrtf(static_cast<float>(dx * dx + dy * dy)));
}

void startTeleportToSpot(HabitatSpot spot) {
  teleportActive = true;
  teleportSpot = spot;
  teleportUntilMs = millis() + kTeleportMs;
  teleportFromX = ghostX;
  teleportFromY = ghostY;
  getSpotCoordinates(spot, teleportToX, teleportToY);
  motion = MotionState::Pause;
  activeSpot = spot;
  forceGhostRedraw = true;

  switch (spot) {
    case HabitatSpot::DataTerminal:
      setStatusText("WARP -> DATA", kCyan);
      break;
    case HabitatSpot::ChargeDock:
      setStatusText("WARP -> DOCK", kYellow);
      break;
    case HabitatSpot::Stabilizer:
      setStatusText("WARP -> STAB", kGreen);
      break;
    case HabitatSpot::SleepNode:
      setStatusText("WARP -> SLEEP", kBlueSoft);
      break;
    default:
      setStatusText("WARP", kCyan);
      break;
  }

  Serial.printf("[TELEPORT] start spot=%d from=(%d,%d) to=(%d,%d)\n",
                static_cast<int>(spot),
                teleportFromX, teleportFromY,
                teleportToX, teleportToY);
}

void finishTeleportIfReady() {
  if (!teleportActive || millis() < teleportUntilMs) return;

  teleportActive = false;

  // Resolve facing before overwriting the source position.
  if (teleportToX > teleportFromX) facingRight = true;
  else if (teleportToX < teleportFromX) facingRight = false;

  ghostX = teleportToX;
  ghostY = teleportToY;
  targetX = teleportToX;
  targetY = teleportToY;

  // Functional interactions deliberately face their station.
  faceTowardSpot(teleportSpot);

  Serial.printf("[TELEPORT] complete spot=%d at=(%d,%d)\n",
                static_cast<int>(teleportSpot), ghostX, ghostY);

  beginHabitatInteraction(teleportSpot);
  teleportSpot = HabitatSpot::None;
  forceGhostRedraw = true;
}

void setTargetToSpot(HabitatSpot spot) {
  activeSpot = spot;

  if (spot == HabitatSpot::DataTerminal ||
      spot == HabitatSpot::ChargeDock ||
      spot == HabitatSpot::Stabilizer ||
      spot == HabitatSpot::SleepNode) {
    Serial.printf("[ROUTE] functional spot=%d stats S=%u I=%u M=%u C=%u\n",
                  static_cast<int>(spot),
                  pet.signal, pet.integrity, pet.mood, pet.charge);
  }
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


void beginHabitatInteraction(HabitatSpot spot) {
  activeSpot = spot;
  faceTowardSpot(spot);
  habitatInteractionUntilMs = millis() + kHabitatInteractionMs;
  pauseUntilMs = habitatInteractionUntilMs;
  motion = MotionState::Pause;

  switch (spot) {
    case HabitatSpot::DataTerminal:
      habitatInteraction = HabitatInteraction::DataSync;
      pet.signal = clamp100(pet.signal + 3);
      pet.mood = clamp100(pet.mood + 1);
      autonomousNeed = AutonomousNeed::None;
      break;
    case HabitatSpot::ChargeDock:
      habitatInteraction = HabitatInteraction::Charging;
      pet.charge = clamp100(pet.charge + 5);
      autonomousNeed = AutonomousNeed::None;
      break;
    case HabitatSpot::Stabilizer:
      habitatInteraction = HabitatInteraction::Stabilizing;
      pet.integrity = clamp100(pet.integrity + 4);
      autonomousNeed = AutonomousNeed::None;
      break;
    case HabitatSpot::SleepNode:
      habitatInteraction = HabitatInteraction::Sleeping;
      sleeping = true;
      motion = MotionState::Pause;
      pauseUntilMs = 0xFFFFFFFF;
      autonomousNeed = AutonomousNeed::None;
      if (autonomousSleepRequested) {
        autonomousWakeAtMs = millis() + kAutonomousNapDurationMs;
      }
      break;
    default:
      habitatInteraction = HabitatInteraction::None;
      return;
  }

  setStatusText(habitatInteractionLabel(), habitatInteractionColor());
  drawHeader();
  drawStats();
  forceGhostRedraw = true;
  Serial.printf("[HABITAT] interaction start spot=%d type=%d\\n",
                static_cast<int>(spot), static_cast<int>(habitatInteraction));
}

void endHabitatInteraction() {
  if (habitatInteraction == HabitatInteraction::None) return;

  Serial.printf("[HABITAT] interaction end type=%d\\n",
                static_cast<int>(habitatInteraction));

  if (habitatInteraction != HabitatInteraction::Sleeping) {
    habitatInteraction = HabitatInteraction::None;
    activeSpot = HabitatSpot::Random;
    setStatusText("PATROL", kDim);
    choosePatrolTarget();
  } else {
    // Sleeping persists until the user wakes Ghost.
    setStatusText("DORMANT", kBlueSoft);
  }

  autonomyCooldownUntilMs = millis() + kAutonomyCooldownMs;
  savePetState();
  forceGhostRedraw = true;
}


void startAutonomousTravel(AutonomousNeed need, HabitatSpot spot, bool critical) {
  autonomousNeed = need;
  autonomyCooldownUntilMs = millis() + kAutonomyCooldownMs;
  setStatusText(autonomousNeedLabel(need), autonomousNeedColor(need));

  if (critical && distanceToSpot(spot) >= kTeleportThreshold) {
    startTeleportToSpot(spot);
  } else {
    setTargetToSpot(spot);
  }

  Serial.printf("[AUTO] need=%d critical=%d target=%d\n",
                static_cast<int>(need), critical ? 1 : 0, static_cast<int>(spot));
}

void startAutonomousPlay() {
  autonomousNeed = AutonomousNeed::Play;
  activeSpot = HabitatSpot::None;
  motion = MotionState::Pause;
  pauseUntilMs = millis() + 1400;
  targetX = ghostX;
  targetY = ghostY;

  pet.mood = clamp100(pet.mood + 8);
  pet.charge = clamp100(pet.charge - 2);
  pet.affection = clamp100(pet.affection + 1);

  startReaction(ReactionState::Play);
  setStatusText("SELF PLAY", kMagenta);
  autonomyCooldownUntilMs = millis() + kAutonomyCooldownMs;
  Serial.println("[AUTO] self play");
  drawStats();
  savePetState();
}

void requestAutonomousSleep() {
  autonomousNeed = AutonomousNeed::Sleep;
  autonomousSleepRequested = true;
  activeSpot = HabitatSpot::SleepNode;
  setStatusText("SEEKING REST", kBlueSoft);

  // Routine naps normally walk to bed. Only an urgent/very long trip uses warp.
  if (distanceToSpot(HabitatSpot::SleepNode) >= (kTeleportThreshold + 70)) {
    startTeleportToSpot(HabitatSpot::SleepNode);
  } else {
    setTargetToSpot(HabitatSpot::SleepNode);
  }

  autonomyCooldownUntilMs = millis() + kAutonomyCooldownMs;
  Serial.println("[AUTO] routine nap requested");
}

void wakeFromAutonomousSleep() {
  if (!sleeping || !autonomousSleepRequested) return;

  sleeping = false;
  autonomousSleepRequested = false;
  autonomousWakeAtMs = 0;
  habitatInteraction = HabitatInteraction::None;
  activeSpot = HabitatSpot::Random;
  pet.mood = clamp100(pet.mood + 2);
  pet.charge = clamp100(pet.charge + 2);

  startReaction(ReactionState::Wake);
  setStatusText("ONLINE", kCyan);
  choosePatrolTarget();
  drawHeader();
  drawStats();
  savePetState();
  forceGhostRedraw = true;
  autonomyCooldownUntilMs = millis() + 15000;
  Serial.println("[AUTO] routine nap complete");
}


void cancelIdlePersonalityByUser(const char* source) {
  if (idlePersonality == IdlePersonality::None) return;

  Serial.printf("[VALIDATE] user cancelled idle event via %s: %s\n",
                source, idlePersonalityLabel());

  idlePersonality = IdlePersonality::None;
  idlePersonalityUntilMs = 0;
  idlePersonalityCooldownUntilMs = millis() + 1200;
  setStatusText("PATROL", kDim);
  forceGhostRedraw = true;
}

void endIdlePersonality() {
  if (idlePersonality == IdlePersonality::None) return;

  Serial.printf("[IDLE] personality end type=%d\n",
                static_cast<int>(idlePersonality));

  idlePersonality = IdlePersonality::None;
  setStatusText("PATROL", kDim);
  idlePersonalityCooldownUntilMs = millis() + kIdlePersonalityCooldownMs;
  forceGhostRedraw = true;
}

void startIdlePersonality(IdlePersonality next) {
  idlePersonality = next;
  idlePersonalityUntilMs = millis() + kIdlePersonalityDurationMs;
  setStatusText(idlePersonalityLabel(), idlePersonalityColor());
  forceGhostRedraw = true;

  Serial.printf("[IDLE] personality start type=%d\n",
                static_cast<int>(idlePersonality));
}

void runIdlePersonality(uint32_t now) {
  if (idlePersonalityBusy()) {
    if (now >= idlePersonalityUntilMs) {
      endIdlePersonality();
    }
    return;
  }

  if (now - lastIdlePersonalityPollMs < kIdlePersonalityPollMs) return;
  lastIdlePersonalityPollMs = now;

  if (now < idlePersonalityCooldownUntilMs) return;
  if (now - lastUserInteractionMs < kAutonomyUserGraceMs) return;
  if (autonomyBusy()) return;
  if (autonomousNeed != AutonomousNeed::None) return;
  if (motion != MotionState::Pause) return;
  if (random(0, 100) >= 45) return;

  IdlePersonality next = IdlePersonality::Scan;

  if (pet.mood >= 75) {
    const IdlePersonality happy[4] = {
        IdlePersonality::Chirp,
        IdlePersonality::Observe,
        IdlePersonality::Scan,
        IdlePersonality::Chirp};
    next = happy[random(0, 4)];
  } else if (pet.integrity <= 45) {
    const IdlePersonality strained[3] = {
        IdlePersonality::Glitch,
        IdlePersonality::Scan,
        IdlePersonality::Observe};
    next = strained[random(0, 3)];
  } else {
    const IdlePersonality normal[4] = {
        IdlePersonality::Scan,
        IdlePersonality::Observe,
        IdlePersonality::Chirp,
        IdlePersonality::Glitch};
    next = normal[random(0, 4)];
  }

  startIdlePersonality(next);
}

void runAutonomousNeeds(uint32_t now) {
  if (now - lastAutonomyPollMs < kAutonomyPollMs) return;
  lastAutonomyPollMs = now;

  if (autonomyBusy()) return;
  if (teleportActive) return;
  if (now < autonomyCooldownUntilMs) return;
  if (now - lastUserInteractionMs < kAutonomyUserGraceMs) return;

  // Strong priority order: survival/energy, integrity, signal, mood.
  if (pet.charge <= kNeedCharge) {
    Serial.printf("[AUTO] CHARGE need triggered: %u <= %u\n",
                  pet.charge, kNeedCharge);
    startAutonomousTravel(AutonomousNeed::Charge,
                          HabitatSpot::ChargeDock,
                          pet.charge <= kCriticalCharge);
    return;
  }

  if (pet.integrity <= kNeedIntegrity) {
    Serial.printf("[AUTO] INTEGRITY need triggered: %u <= %u\n",
                  pet.integrity, kNeedIntegrity);
    startAutonomousTravel(AutonomousNeed::Stabilize,
                          HabitatSpot::Stabilizer,
                          pet.integrity <= kCriticalIntegrity);
    return;
  }

  if (pet.signal <= kNeedSignal) {
    Serial.printf("[AUTO] SIGNAL need triggered: %u <= %u\n",
                  pet.signal, kNeedSignal);
    startAutonomousTravel(AutonomousNeed::Data,
                          HabitatSpot::DataTerminal,
                          pet.signal <= kCriticalSignal);
    return;
  }

  if (pet.mood <= kNeedMood) {
    Serial.printf("[AUTO] MOOD need triggered: %u <= %u\n",
                  pet.mood, kNeedMood);
    startAutonomousPlay();
    return;
  }

  // Healthy Ghost occasionally takes a routine nap after a long quiet period.
  if ((now - lastUserInteractionMs) >= kAutonomousNapIdleMs &&
      pet.charge >= 45 &&
      pet.integrity >= 45 &&
      pet.mood >= 45 &&
      random(0, 100) < 35) {
    requestAutonomousSleep();
    return;
  }
}

void choosePatrolTarget() {
  // STRICT STATION GATING:
  // Ordinary patrol is NEVER allowed to select DATA / CHARGE / STABILIZE.
  // Functional stations may only be targeted by:
  //   1) explicit user actions
  //   2) runAutonomousNeeds() after its threshold check
  //   3) the sleep routine for SleepNode
  //
  // MoodState may change movement style/aura, but not patrol destinations.

  if (sleeping) {
    targetX = ghostX;
    targetY = ghostY;
    motion = MotionState::Dormant;
    return;
  }

  activeSpot = HabitatSpot::Random;
  targetX = clamp16(random(kMinGhostX, kMaxGhostX + 1), kMinGhostX, kMaxGhostX);
  targetY = clamp16(random(kMinGhostY, kMaxGhostY + 1), kMinGhostY, kMaxGhostY);
  motion = MotionState::Patrol;

  Serial.printf("[PATROL] random target=(%d,%d) stats S=%u I=%u M=%u C=%u\n",
                targetX, targetY,
                pet.signal, pet.integrity, pet.mood, pet.charge);
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
    if (activeSpot == HabitatSpot::DataTerminal ||
        activeSpot == HabitatSpot::ChargeDock ||
        activeSpot == HabitatSpot::SleepNode ||
        activeSpot == HabitatSpot::Stabilizer) {
      faceTowardSpot(activeSpot);
      beginHabitatInteraction(activeSpot);
    } else {
      motion = MotionState::Pause;
      pauseUntilMs = millis() + random(900, 2500);
      Serial.printf("[MOVE] arrived=(%d,%d), pause\n", ghostX, ghostY);
    }
  }
}

void updateMotion(uint32_t now) {
  if (teleportActive) {
    return;
  }

  if (sleeping) {
    motion = MotionState::Dormant;
    // Once dormant, remain parked at the node until explicitly woken.
    forceGhostRedraw = true;
    return;
  }

  if (motion == MotionState::Pause) {
    if (habitatInteractionActive()) {
      if (habitatInteraction == HabitatInteraction::Sleeping) {
        return;
      }
      if (now >= habitatInteractionUntilMs) {
        endHabitatInteraction();
      }
      return;
    }

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
  lastUserInteractionMs = millis();
  cancelIdlePersonalityByUser("BUTTON");
  autonomousNeed = AutonomousNeed::None;
  autonomousSleepRequested = false;
  autonomyCooldownUntilMs = millis() + kAutonomyCooldownMs;

  if (sleeping) {
    sleeping = false;
    habitatInteraction = HabitatInteraction::None;
    activeSpot = HabitatSpot::Random;
    startReaction(ReactionState::Wake);
  }

  switch (idx) {
    case 0:  // DATA
      pet.signal = clamp100(pet.signal + 10);
      pet.mood = clamp100(pet.mood + 2);
      pet.affection = clamp100(pet.affection + 1);
      startReaction(ReactionState::Data);
      if (distanceToSpot(HabitatSpot::DataTerminal) >= kTeleportThreshold) {
        startTeleportToSpot(HabitatSpot::DataTerminal);
      } else {
        setTargetToSpot(HabitatSpot::DataTerminal);
        setStatusText("DATA SYNC", kCyan);
      }
      Serial.println("[ACTION] DATA");
      break;

    case 1:  // CHARGE
      pet.charge = clamp100(pet.charge + 12);
      pet.mood = clamp100(pet.mood + 1);
      pet.affection = clamp100(pet.affection + 1);
      startReaction(ReactionState::Charge);
      if (distanceToSpot(HabitatSpot::ChargeDock) >= kTeleportThreshold) {
        startTeleportToSpot(HabitatSpot::ChargeDock);
      } else {
        setTargetToSpot(HabitatSpot::ChargeDock);
        setStatusText("CHARGING", kYellow);
      }
      Serial.println("[ACTION] CHARGE");
      break;

    case 2:  // STABILIZE
      pet.integrity = clamp100(pet.integrity + 8);
      pet.signal = clamp100(pet.signal - 2);
      pet.affection = clamp100(pet.affection + 1);
      startReaction(ReactionState::Stabilize);
      if (distanceToSpot(HabitatSpot::Stabilizer) >= kTeleportThreshold) {
        startTeleportToSpot(HabitatSpot::Stabilizer);
      } else {
        setTargetToSpot(HabitatSpot::Stabilizer);
        setStatusText("STABILIZING", kGreen);
      }
      Serial.println("[ACTION] STABILIZE");
      break;

    case 3:  // PLAY — animation only, no habitat destination
      pet.mood = clamp100(pet.mood + 15);
      pet.charge = clamp100(pet.charge - 3);
      pet.signal = clamp100(pet.signal - 1);
      pet.affection = clamp100(pet.affection + 3);
      startReaction(ReactionState::Play);
      activeSpot = HabitatSpot::None;
      motion = MotionState::Pause;
      pauseUntilMs = millis() + kReactionMs;
      targetX = ghostX;
      targetY = ghostY;
      setStatusText("PLAYING", kMagenta);
      Serial.println("[ACTION] PLAY");
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
  lastUserInteractionMs = millis();
  cancelIdlePersonalityByUser("GHOST");
  autonomousNeed = AutonomousNeed::None;
  autonomousSleepRequested = false;
  autonomyCooldownUntilMs = millis() + kAutonomyCooldownMs;

  if (sleeping) {
    sleeping = false;
    habitatInteraction = HabitatInteraction::None;
    activeSpot = HabitatSpot::Random;
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
  lastUserInteractionMs = millis();
  cancelIdlePersonalityByUser("FIELD");
  autonomousNeed = AutonomousNeed::None;
  autonomousSleepRequested = false;
  autonomyCooldownUntilMs = millis() + kAutonomyCooldownMs;
  activeSpot = HabitatSpot::None;
  targetX = clamp16(static_cast<int>(x) - (kGhostW / 2), kMinGhostX, kMaxGhostX);
  targetY = clamp16(static_cast<int>(y) - (kGhostH / 2), kMinGhostY, kMaxGhostY);
  motion = MotionState::Patrol;
  setStatusText("SIGNAL PING", kCyan);
  Serial.printf("[MOVE] user lure target=(%d,%d)\n", targetX, targetY);
}


void requestSleepAtNode() {
  lastUserInteractionMs = millis();
  cancelIdlePersonalityByUser("SLEEP");
  autonomousNeed = AutonomousNeed::None;
  autonomousSleepRequested = false;
  autonomyCooldownUntilMs = millis() + kAutonomyCooldownMs;

  if (sleeping) {
    // If already dormant, tapping the sleep node wakes Ghost.
    sleeping = false;
    habitatInteraction = HabitatInteraction::None;
    activeSpot = HabitatSpot::Random;
    startReaction(ReactionState::Wake);
    setStatusText("ONLINE", kCyan);
    choosePatrolTarget();
    drawHeader();
    forceGhostRedraw = true;
    savePetState();
    Serial.println("[SLEEP] Wake requested from sleep node.");
    return;
  }

  // Do NOT set sleeping immediately. Ghost should visibly travel to the node
  // first, then enter persistent dormancy only after arrival.
  habitatInteraction = HabitatInteraction::None;
  activeSpot = HabitatSpot::SleepNode;

  if (distanceToSpot(HabitatSpot::SleepNode) >= kTeleportThreshold) {
    startTeleportToSpot(HabitatSpot::SleepNode);
  } else {
    setStatusText("GOING DORMANT", kBlueSoft);
    setTargetToSpot(HabitatSpot::SleepNode);
  }

  Serial.println("[SLEEP] Routing Ghost to sleep node.");
  forceGhostRedraw = true;
}

void toggleSleep() {
  requestSleepAtNode();
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
  Serial.println(" SPECTER//OS - CYD4 PHASE 0K");
  Serial.println(" CHARACTER MOTION + TIMING");
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
  Serial.println("Sprite layers: BODY / FX-ONLY / REPLACEMENT");
  Serial.println("Habitat layer: 8-BIT BUFFERED + SPRITE FX");
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
    tft.drawString("TOUCH CALIBRATION", 240, 130, 2);
    tft.setTextColor(kWhite, kBg);
    tft.drawString("Tap each corner target when shown.", 240, 165, 2);
    Serial.println("[TOUCH] Calibration missing; starting corner calibration.");
    delay(1500);
    tft.fillScreen(kBg);
    tft.calibrateTouch(touchCal, kWhite, kBg, 15);
    tft.setTouch(touchCal);
    touchReady = true;
    const bool opened = prefs.begin("specter", false);
    const size_t saved = opened ? prefs.putBytes("touchcal", touchCal, sizeof(touchCal)) : 0;
    if (opened) prefs.end();
    Serial.printf("[TOUCH] Calibration complete: {%u,%u,%u,%u,%u}, saved=%u\n",
                  touchCal[0], touchCal[1], touchCal[2], touchCal[3], touchCal[4],
                  static_cast<unsigned>(saved));
    if (saved != sizeof(touchCal)) {
      Serial.println("[TOUCH] WARNING: calibration save failed; will recalibrate next boot.");
    }
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
  lastAutonomyPollMs = now;
  lastUserInteractionMs = now;
  autonomyCooldownUntilMs = now + kAutonomyCooldownMs;
  lastAmbientFrameMs = now;
  lastIdlePersonalityPollMs = now;
  idlePersonalityCooldownUntilMs = now + 8000;
}

void loop() {
  if (!touchReady) {
    delay(100);
    return;
  }

  const uint32_t now = millis();

  if (reaction != ReactionState::None && now >= reactionUntilMs) {
    reaction = ReactionState::None;

    if (autonomousNeed == AutonomousNeed::Play) {
      autonomousNeed = AutonomousNeed::None;
    }

    if (!teleportActive) {
      setStatusText(sleeping ? "DORMANT" : "PATROL", sleeping ? kBlueSoft : kDim);
    }

    if (!sleeping && !habitatInteractionActive() && !teleportActive) {
      choosePatrolTarget();
    }

    forceGhostRedraw = true;
  }

  finishTeleportIfReady();

  if (autonomousSleepRequested && sleeping &&
      autonomousWakeAtMs != 0 && now >= autonomousWakeAtMs) {
    wakeFromAutonomousSleep();
  }

  runAutonomousNeeds(now);

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
    } else if (pointInRect(x, y, kSleepNodeTouch)) {
      Serial.printf("[TOUCH] sleep node x=%u y=%u\n", x, y);
      requestSleepAtNode();
    } else if (pointInRect(x, y, kHeaderSleepTouch)) {
      Serial.printf("[TOUCH] status/sleep x=%u y=%u\n", x, y);
      requestSleepAtNode();
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

  if ((habitatInteractionActive() || teleportActive) &&
      now - lastHabitatPulseMs >= kHabitatPulseMs) {
    lastHabitatPulseMs = now;
    habitatPulseFrame = (habitatPulseFrame + 1) % 8;
    forceGhostRedraw = true;
  }

  if (now - lastAmbientFrameMs >= kAmbientFrameMs) {
    lastAmbientFrameMs = now;
    ambientFrame = (ambientFrame + 1) % 32;
    forceGhostRedraw = true;
  }

  runIdlePersonality(now);

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

