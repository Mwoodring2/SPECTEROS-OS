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
  uint32_t saveCounter = 0;
};

PetState pet;

enum class MoodState : uint8_t {
  Normal,
  Happy,
  LowCharge,
  Unstable
};

constexpr uint32_t kAnimMs = 280;
constexpr uint32_t kDecayTickMs = 30000;
constexpr uint32_t kAutoSaveMs = 30000;

uint32_t lastAnimMs = 0;
uint32_t lastDecayMs = 0;
uint32_t lastAutoSaveMs = 0;
uint32_t lastTouchMs = 0;
uint8_t animFrame = 0;
bool forceGhostRedraw = true;

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
  const bool exists = prefs.isKey("petstate");
  if (exists) {
    const size_t n = prefs.getBytes("petstate", &pet, sizeof(pet));
    prefs.end();
    if (n == sizeof(pet)) {
      Serial.println("[PET] Loaded persistent pet state.");
      return true;
    }
    return false;
  }
  prefs.end();
  Serial.println("[PET] No pet state yet; using defaults.");
  return false;
}

void savePetState() {
  pet.saveCounter++;
  prefs.begin("specter", false);
  const size_t n = prefs.putBytes("petstate", &pet, sizeof(pet));
  prefs.end();
  Serial.printf("[PET] Saved state (%u bytes), save #%lu\n",
                static_cast<unsigned>(n),
                static_cast<unsigned long>(pet.saveCounter));
}

MoodState currentMoodState() {
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
    default: return "ONLINE";
  }
}

uint16_t moodColor() {
  switch (currentMoodState()) {
    case MoodState::Happy: return kMagenta;
    case MoodState::LowCharge: return kYellow;
    case MoodState::Unstable: return kRed;
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
  String pct = String(value) + "%";
  tft.drawString(pct, x + w, y, 2);
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

void drawGhostFrame(uint8_t frame) {
  constexpr int kGhostX = 192;
  constexpr int kGhostY = 128;

  // Clear only the character stage so the rest of the UI remains stable.
  tft.fillRect(142, 108, 196, 150, kBg);

  // A subtle platform / aura.
  tft.drawEllipse(240, 224, 58, 9, kDim);
  tft.drawEllipse(240, 224, 40, 5, currentMoodState() == MoodState::Happy ? kMagenta : kCyan);

  const uint16_t* pixels = specter::assets::kGhostIdleFrames[frame];
  tft.pushImage(kGhostX, kGhostY,
                specter::assets::kGhostWidth,
                specter::assets::kGhostHeight,
                pixels,
                specter::assets::kGhostTransparent);

  // State-specific overlays make the first prototype expressive without
  // creating extra sprite atlases yet.
  if (currentMoodState() == MoodState::Happy) {
    tft.drawCircle(240, 175, 60, kMagenta);
    tft.drawPixel(183, 126, kCyan);
    tft.drawPixel(305, 142, kMagenta);
  } else if (currentMoodState() == MoodState::LowCharge) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(kYellow, kBg);
    tft.drawString("...", 240, 244, 2);
  } else if (currentMoodState() == MoodState::Unstable) {
    for (int i = 0; i < 5; ++i) {
      const int y = 124 + (i * 23);
      const int dx = (i % 2 == 0) ? 14 : -10;
      tft.drawFastHLine(175 + dx, y, 125, (i % 2) ? kMagenta : kCyan);
    }
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(moodColor(), kBg);
  tft.drawString("GHOST", 240, 248, 2);
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
    delay(55);
    tft.drawFastHLine(x, y, w, kBg);
  }

  tft.setTextColor(kCyan, kBg);
  tft.drawString("SPECTER//OS", 240, 108, 4);
  tft.setTextColor(kWhite, kBg);
  tft.drawString("GHOST SYSTEM ONLINE", 240, 148, 2);
  delay(700);

  // Simple phase-in using increasing visible height of the first Ghost frame.
  tft.fillScreen(kBg);
  tft.setTextColor(kCyan, kBg);
  tft.drawString("SPECTER//OS", 240, 32, 4);

  constexpr int gx = 192;
  constexpr int gy = 110;
  for (int rows = 8; rows <= specter::assets::kGhostHeight; rows += 8) {
    const uint16_t* pixels = specter::assets::kGhostIdleFrames[0];
    tft.pushImage(gx, gy,
                  specter::assets::kGhostWidth,
                  rows,
                  pixels,
                  specter::assets::kGhostTransparent);
    delay(45);
  }
  delay(350);
}

void flashButton(int idx) {
  if (idx < 0 || idx > 3) return;
  const Rect& b = kButtons[idx];
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, kWhite);
  delay(70);
  drawButtons();
}

void applyAction(int idx) {
  switch (idx) {
    case 0: // DATA
      pet.signal = clamp100(pet.signal + 14);
      pet.mood = clamp100(pet.mood + 2);
      Serial.println("[ACTION] DATA");
      break;

    case 1: // PLAY
      pet.mood = clamp100(pet.mood + 15);
      pet.charge = clamp100(pet.charge - 3);
      pet.signal = clamp100(pet.signal - 1);
      Serial.println("[ACTION] PLAY");
      break;

    case 2: // CHARGE
      pet.charge = clamp100(pet.charge + 18);
      pet.mood = clamp100(pet.mood + 1);
      Serial.println("[ACTION] CHARGE");
      break;

    case 3: // STABILIZE
      pet.integrity = clamp100(pet.integrity + 12);
      pet.signal = clamp100(pet.signal - 2);
      Serial.println("[ACTION] STABILIZE");
      break;

    default:
      return;
  }

  flashButton(idx);
  drawHeader();
  drawStats();
  forceGhostRedraw = true;
  savePetState();
}

int hitButton(uint16_t x, uint16_t y) {
  for (int i = 0; i < 4; ++i) {
    const Rect& b = kButtons[i];
    if (x >= b.x && x < b.x + b.w &&
        y >= b.y && y < b.y + b.h) {
      return i;
    }
  }
  return -1;
}

void decayPet() {
  // Phase 0B uses powered-on decay only. Offline/RTC decay belongs to a later
  // milestone once timekeeping is deliberately defined.
  pet.signal = clamp100(pet.signal - 1);
  pet.charge = clamp100(pet.charge - 1);

  static uint8_t slowTick = 0;
  slowTick++;

  if ((slowTick % 2) == 0) {
    pet.mood = clamp100(pet.mood - 1);
  }
  if ((slowTick % 4) == 0) {
    pet.integrity = clamp100(pet.integrity - 1);
  }

  Serial.printf("[PET] decay signal=%u integrity=%u mood=%u charge=%u\n",
                pet.signal, pet.integrity, pet.mood, pet.charge);

  drawHeader();
  drawStats();
  forceGhostRedraw = true;
}

void printBootInfo() {
  Serial.println();
  Serial.println("======================================");
  Serial.println(" SPECTER//OS - CYD4 PHASE 0B");
  Serial.println(" DIGITAL GHOST PET FOUNDATION");
  Serial.println("======================================");
  Serial.printf("Chip: %s rev %d @ %d MHz\n",
                ESP.getChipModel(),
                ESP.getChipRevision(),
                ESP.getCpuFreqMHz());
  Serial.printf("Heap free: %u bytes\n", ESP.getFreeHeap());
  Serial.println("Display: ST7796S 480x320 landscape");
  Serial.println("Touch: XPT2046");
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
  // Embedded sprite frames are stored as host-generated RGB565 words.
  // TFT_eSPI needs byte swapping enabled for correct pushImage() colors/order.
  tft.setSwapBytes(true);
  tft.setRotation(specter::board::kRotation);
  tft.fillScreen(kBg);
  Serial.println("[DISPLAY] RGB565 sprite byte-swap ENABLED");

  if (loadCalibration()) {
    tft.setTouch(touchCal);
    touchReady = true;
    Serial.printf("[TOUCH] Calibration loaded: {%u,%u,%u,%u,%u}\n",
                  touchCal[0], touchCal[1], touchCal[2], touchCal[3], touchCal[4]);
  } else {
    // Phase 0A should already have produced calibration. If NVS was erased,
    // deliberately fail visibly rather than silently accepting bad touch.
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

  lastAnimMs = millis();
  lastDecayMs = millis();
  lastAutoSaveMs = millis();
}

void loop() {
  if (!touchReady) {
    delay(100);
    return;
  }

  const uint32_t now = millis();

  uint16_t x = 0, y = 0;
  if (now - lastTouchMs > 120 && tft.getTouch(&x, &y, 350)) {
    lastTouchMs = now;
    const int button = hitButton(x, y);
    Serial.printf("[TOUCH] x=%u y=%u button=%d\n", x, y, button);
    if (button >= 0) {
      applyAction(button);
    }
  }

  if (now - lastAnimMs >= kAnimMs) {
    lastAnimMs = now;
    animFrame = (animFrame + 1) % specter::assets::kGhostIdleFrameCount;
    drawGhostFrame(animFrame);
    forceGhostRedraw = false;
  } else if (forceGhostRedraw) {
    drawGhostFrame(animFrame);
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
