// ============================================================
//   2-Player RGB LED Reaction Game 
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <avr/pgmspace.h>

#define OLED_ADDRESS 0x3C
Adafruit_SH1106G display(128, 64, &Wire, -1);

// --- RGB LED Pins ---
const int LED_R[3] = {2, 4, 6};
const int LED_G[3] = {3, 5, 7};

// --- Button Pins ---
const int BTN_P1[3] = {A1, A2, A3};
const int BTN_P2[3] = {A0, 12, 13};
const int BTN_START  = 11;
const int BUZZER_PIN = 8;

// --- Game Settings ---
const unsigned long GAME_DURATION   = 40000;
const unsigned long PHASE1_MIN_FLASH_DELAY = 260;
const unsigned long PHASE1_MAX_FLASH_DELAY = 420;
const unsigned long PHASE2_MIN_FLASH_DELAY = 180;
const unsigned long PHASE2_MAX_FLASH_DELAY = 300;
const unsigned long PHASE3_MIN_FLASH_DELAY = 100;
const unsigned long PHASE3_MAX_FLASH_DELAY = 190;
const unsigned long PHASE4_MIN_FLASH_DELAY = 60;
const unsigned long PHASE4_MAX_FLASH_DELAY = 115;
const unsigned long PHASE1_LED_ON_DURATION  = 620;
const unsigned long PHASE2_LED_ON_DURATION  = 470;
const unsigned long PHASE3_LED_ON_DURATION  = 330;
const unsigned long PHASE4_LED_ON_DURATION  = 220;

// --- Game State ---
int scoreP1 = 0;
int scoreP2 = 0;
bool gameRunning = false;
unsigned long gameStartTime = 0;

// --- LED State: 0=off, 1=red, 2=green ---
int ledState[3] = {0, 0, 0};
unsigned long ledOnTime[3] = {0, 0, 0};
unsigned long ledGreenTime[3] = {0, 0, 0};
unsigned long nextEventTime = 0;

// --- Button Debounce ---
bool lastBtnP1[3] = {false, false, false};
bool lastBtnP2[3] = {false, false, false};

// --- Last Reaction Time ---
float lastReactionSec = -1.0;

// --- Reaction Time Tracking ---
float totalReactionP1 = 0.0;
float totalReactionP2 = 0.0;
int countP1 = 0;
int countP2 = 0;

// --- Sound / Music ---
const uint16_t bgMelody[] PROGMEM = {
  220, 294, 294, 370, 494, 370, 440, 0,
  440, 494, 440, 370, 392, 370, 330, 0
};
const uint16_t bgDurations[] PROGMEM = {
  210, 140, 240, 140, 240, 140, 260, 120,
  210, 140, 240, 140, 240, 140, 280, 140
};
const uint8_t BG_NOTES = sizeof(bgMelody) / sizeof(bgMelody[0]);
uint8_t bgNoteIndex = 0;
unsigned long bgNextNoteTime = 0;
bool bgMusicEnabled = false;

unsigned long getMinFlashDelay(unsigned long elapsed) {
  if (elapsed < 12000UL) return PHASE1_MIN_FLASH_DELAY;
  if (elapsed < 24000UL) return PHASE2_MIN_FLASH_DELAY;
  if (elapsed < 33000UL) return PHASE3_MIN_FLASH_DELAY;
  return PHASE4_MIN_FLASH_DELAY;
}

unsigned long getMaxFlashDelay(unsigned long elapsed) {
  if (elapsed < 12000UL) return PHASE1_MAX_FLASH_DELAY;
  if (elapsed < 24000UL) return PHASE2_MAX_FLASH_DELAY;
  if (elapsed < 33000UL) return PHASE3_MAX_FLASH_DELAY;
  return PHASE4_MAX_FLASH_DELAY;
}

unsigned long getLedOnDuration(unsigned long elapsed) {
  if (elapsed < 12000UL) return PHASE1_LED_ON_DURATION;
  if (elapsed < 24000UL) return PHASE2_LED_ON_DURATION;
  if (elapsed < 33000UL) return PHASE3_LED_ON_DURATION;
  return PHASE4_LED_ON_DURATION;
}

float getMusicSpeedFactor(unsigned long elapsed) {
  if (elapsed < 12000UL) return 1.12;
  if (elapsed < 24000UL) return 0.95;
  if (elapsed < 33000UL) return 0.75;
  return 0.58;
}

// ============================================================
void setup() {
  Serial.begin(9600);
  Serial.println(F("Starting..."));

  Wire.begin();
  Wire.setClock(400000);
  delay(500);
  display.begin(OLED_ADDRESS, true);
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  Serial.println(F("OLED ready!"));
  oledIdle();

  for (int i = 0; i < 3; i++) {
    pinMode(LED_R[i], OUTPUT);
    pinMode(LED_G[i], OUTPUT);
    setLED(i, 0);
  }

  for (int i = 0; i < 3; i++) {
    pinMode(BTN_P1[i], INPUT_PULLUP);
    pinMode(BTN_P2[i], INPUT_PULLUP);
  }
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  Serial.println(F("====================================="));
  Serial.println(F("  2-Player Reaction Game Ready!"));
  Serial.println(F("  Press START to begin."));
  Serial.println(F("====================================="));
}

// ============================================================
void loop() {
  if (!gameRunning) {
    if (digitalRead(BTN_START) == LOW) {
      delay(50);
      if (digitalRead(BTN_START) == LOW) {
        countdown();
        startGame();
        while (digitalRead(BTN_START) == LOW);
      }
    }
  } else {
    updateBackgroundMusic();
    runGame();
  }
}

// ============================================================
void countdown() {
  for (int i = 3; i >= 1; i--) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(25, 5);
    display.println(F("Get ready..."));
    display.setTextSize(4);
    display.setCursor(54, 24);
    display.println(i);
    display.display();
    tone(BUZZER_PIN, 1000 + (3 - i) * 180, 140);
    delay(800);
  }
  display.clearDisplay();
  display.setTextSize(4);
  display.setCursor(22, 16);
  display.println(F("GO!"));
  display.display();
  tone(BUZZER_PIN, 1600, 220);
  delay(500);
  noTone(BUZZER_PIN);
  Serial.println(F("  3... 2... 1... GO!"));
}

// ============================================================
void startGame() {
  scoreP1 = 0;
  scoreP2 = 0;
  gameRunning = true;
  gameStartTime = millis();
  nextEventTime = millis() + random(getMinFlashDelay(0), getMaxFlashDelay(0));
  lastReactionSec = -1.0;
  bgNoteIndex = 0;
  bgNextNoteTime = millis();
  bgMusicEnabled = true;

  totalReactionP1 = 0.0;
  totalReactionP2 = 0.0;
  countP1 = 0;
  countP2 = 0;

  for (int i = 0; i < 3; i++) {
    setLED(i, 0);
    ledState[i] = 0;
  }

  oledGame(60);

  Serial.println(F("\n====================================="));
  Serial.println(F("  GAME STARTED!"));
  Serial.println(F("  GREEN=score! RED=penalty -1"));
  Serial.println(F("====================================="));
}

// ============================================================
void runGame() {
  unsigned long now = millis();
  unsigned long elapsed = now - gameStartTime;
  unsigned long remaining = (elapsed >= GAME_DURATION) ? 0 : (GAME_DURATION - elapsed);
  unsigned long secondsLeft = remaining / 1000;

  if (elapsed >= GAME_DURATION) {
    endGame();
    return;
  }

  static unsigned long lastSecond = 61;
  if (secondsLeft != lastSecond) {
    lastSecond = secondsLeft;
    Serial.print(F("  Time: "));
    Serial.print(secondsLeft);
    Serial.print(F("s | P1:"));
    Serial.print(scoreP1);
    Serial.print(F(" P2:"));
    Serial.println(scoreP2);
    oledGame(secondsLeft);
  }

  if (now >= nextEventTime) {
    int candidates[3];
    int count = 0;
    for (int i = 0; i < 3; i++) {
      if (ledState[i] == 0) candidates[count++] = i;
    }
    if (count > 0) {
      int pick = candidates[random(0, count)];
      int color = (random(0, 2) == 0) ? 1 : 2;
      ledState[pick] = color;
      ledOnTime[pick] = now;
      if (color == 2) ledGreenTime[pick] = now;
      setLED(pick, color);
    }
    nextEventTime = now + random(getMinFlashDelay(elapsed), getMaxFlashDelay(elapsed));
  }

  for (int i = 0; i < 3; i++) {
    if (ledState[i] != 0 && (now - ledOnTime[i] >= getLedOnDuration(elapsed))) {
      setLED(i, 0);
      ledState[i] = 0;
    }
  }

  for (int i = 0; i < 3; i++) {
    bool p1Pressed = (digitalRead(BTN_P1[i]) == LOW);
    bool p2Pressed = (digitalRead(BTN_P2[i]) == LOW);

    if (p1Pressed && !lastBtnP1[i]) handlePress(1, i, secondsLeft);
    if (p2Pressed && !lastBtnP2[i]) handlePress(2, i, secondsLeft);

    lastBtnP1[i] = p1Pressed;
    lastBtnP2[i] = p2Pressed;
  }
}

// ============================================================
void handlePress(int player, int ledIndex, unsigned long timeLeft) {
  int state = ledState[ledIndex];
  if (state == 0) return;

  if (state == 2) {
    unsigned long reactionMs = millis() - ledGreenTime[ledIndex];
    lastReactionSec = reactionMs / 1000.0;

    if (player == 1) {
      scoreP1++;
      totalReactionP1 += lastReactionSec;
      countP1++;
    } else {
      scoreP2++;
      totalReactionP2 += lastReactionSec;
      countP2++;
    }

    Serial.print(F("  >> P"));
    Serial.print(player);
    Serial.print(F(" SCORED! "));
    Serial.print(lastReactionSec);
    Serial.print(F("s P1="));
    Serial.print(scoreP1);
    Serial.print(F(" P2="));
    Serial.println(scoreP2);

    setLED(ledIndex, 0);
    ledState[ledIndex] = 0;
    oledGame(timeLeft);

  } else if (state == 1) {
    if (player == 1) scoreP1--;
    else scoreP2--;

    lastReactionSec = -1.0;

    Serial.print(F("  !! P"));
    Serial.print(player);
    Serial.print(F(" PENALTY! P1="));
    Serial.print(scoreP1);
    Serial.print(F(" P2="));
    Serial.println(scoreP2);

    setLED(ledIndex, 0);
    ledState[ledIndex] = 0;
    oledGame(timeLeft);
  }
}

// ============================================================
void endGame() {
  gameRunning = false;
  bgMusicEnabled = false;
  noTone(BUZZER_PIN);

  for (int i = 0; i < 3; i++) {
    setLED(i, 0);
    ledState[i] = 0;
  }

  float avgP1 = (countP1 > 0) ? totalReactionP1 / countP1 : 0.0;
  float avgP2 = (countP2 > 0) ? totalReactionP2 / countP2 : 0.0;

  Serial.println(F(""));
  Serial.println(F("====================================="));
  Serial.println(F("  GAME OVER!"));
  Serial.print(F("  P1:"));
  Serial.print(scoreP1);
  Serial.print(F(" | P2:"));
  Serial.println(scoreP2);

  setAllLEDsRed();
  delay(1000);
  setAllLEDsOff();

  if (scoreP1 > scoreP2) {
    Serial.println(F("  *** PLAYER 1 WINS! ***"));
    oledWinnerScreen(1, avgP1);
    playWinSound(1);
  } else if (scoreP2 > scoreP1) {
    Serial.println(F("  *** PLAYER 2 WINS! ***"));
    oledWinnerScreen(2, avgP2);
    playWinSound(2);
  } else {
    Serial.println(F("  *** TIE! ***"));
    oledTie();
    playTieSound();
  }

  Serial.println(F("  Press START to play again!"));
  Serial.println(F("====================================="));
  Serial.println();

  while (digitalRead(BTN_START) == LOW) {
    delay(10);
  }

  while (digitalRead(BTN_START) == HIGH) {
    delay(10);
  }

  delay(200);
}

// ============================================================
void setAllLEDsRed() {
  for (int i = 0; i < 3; i++) setLED(i, 1);
}

void setAllLEDsOff() {
  for (int i = 0; i < 3; i++) setLED(i, 0);
}

// ============================================================
void updateBackgroundMusic() {
  if (!bgMusicEnabled) return;

  unsigned long now = millis();
  if (!gameRunning) return;
  if (now < bgNextNoteTime) return;

  unsigned long elapsed = now - gameStartTime;
  float speedFactor = getMusicSpeedFactor(elapsed);

  uint16_t freq = pgm_read_word(&bgMelody[bgNoteIndex]);
  uint16_t baseDur = pgm_read_word(&bgDurations[bgNoteIndex]);
  uint16_t dur = (uint16_t)(baseDur * speedFactor);
  if (dur < 65) dur = 65;

  if (freq > 0) {
    uint16_t playFor = (dur > 30) ? (dur - 22) : dur;
    tone(BUZZER_PIN, freq, playFor);
  } else {
    noTone(BUZZER_PIN);
  }

  bgNextNoteTime = now + dur;
  bgNoteIndex++;
  if (bgNoteIndex >= BG_NOTES) bgNoteIndex = 0;
}

void playWinSound(int winner) {
  const uint16_t notes[] = {659, 784, 988, 1319, 1047};
  const uint16_t durations[] = {100, 100, 120, 220, 180};
  for (uint8_t i = 0; i < 5; i++) {
    tone(BUZZER_PIN, notes[i], durations[i]);
    delay(durations[i] + 25);
  }
  noTone(BUZZER_PIN);
}

void playTieSound() {
  const uint16_t notes[] = {659, 784, 659};
  const uint16_t durations[] = {110, 110, 170};
  for (uint8_t i = 0; i < 3; i++) {
    tone(BUZZER_PIN, notes[i], durations[i]);
    delay(durations[i] + 20);
  }
  noTone(BUZZER_PIN);
}

// ============================================================
// OLED SCREENS
// ============================================================

void oledIdle() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 0);
  display.println(F("REACTION GAME"));
  display.drawLine(0, 10, 128, 10, SH110X_WHITE);
  display.setCursor(0, 16);
  display.println(F("GREEN LED = PRESS!"));
  display.setCursor(0, 28);
  display.println(F("RED LED   = AVOID!"));
  display.drawLine(0, 42, 128, 42, SH110X_WHITE);
  display.setCursor(15, 50);
  display.println(F("Press START!"));
  display.display();
}

void oledGame(unsigned long secondsLeft) {
  display.clearDisplay();

  display.setTextSize(3);
  display.setCursor(44, 0);
  if (secondsLeft < 10) display.print(F("0"));
  display.println(secondsLeft);

  display.drawLine(0, 26, 128, 26, SH110X_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 32);
  display.print(F("P1:"));
  display.print(scoreP1);

  display.setCursor(70, 32);
  display.print(F("P2:"));
  display.print(scoreP2);

  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(F("RT: "));
  if (lastReactionSec >= 0) {
    display.print(lastReactionSec, 3);
    display.print(F("s"));
  } else {
    display.print(F("---"));
  }

  display.display();
}

void drawCuteStar(int x, int y) {
  display.drawPixel(x, y, SH110X_WHITE);
  display.drawPixel(x - 1, y, SH110X_WHITE);
  display.drawPixel(x + 1, y, SH110X_WHITE);
  display.drawPixel(x, y - 1, SH110X_WHITE);
  display.drawPixel(x, y + 1, SH110X_WHITE);
}

void drawHeart(int x, int y) {
  display.drawPixel(x + 1, y, SH110X_WHITE);
  display.drawPixel(x + 3, y, SH110X_WHITE);
  display.drawPixel(x, y + 1, SH110X_WHITE);
  display.drawPixel(x + 1, y + 1, SH110X_WHITE);
  display.drawPixel(x + 2, y + 1, SH110X_WHITE);
  display.drawPixel(x + 3, y + 1, SH110X_WHITE);
  display.drawPixel(x + 4, y + 1, SH110X_WHITE);
  display.drawPixel(x + 1, y + 2, SH110X_WHITE);
  display.drawPixel(x + 2, y + 2, SH110X_WHITE);
  display.drawPixel(x + 3, y + 2, SH110X_WHITE);
  display.drawPixel(x + 2, y + 3, SH110X_WHITE);
}

void drawTrophyCute(int x, int y) {
  display.fillRoundRect(x + 8, y + 10, 18, 12, 3, SH110X_WHITE);
  display.fillRect(x + 15, y + 22, 4, 7, SH110X_WHITE);
  display.fillRoundRect(x + 10, y + 29, 14, 4, 1, SH110X_WHITE);
  display.drawCircle(x + 7, y + 14, 4, SH110X_WHITE);
  display.drawCircle(x + 27, y + 14, 4, SH110X_WHITE);
  display.fillCircle(x + 7, y + 14, 1, SH110X_WHITE);
  display.fillCircle(x + 27, y + 14, 1, SH110X_WHITE);
}

void oledWinnerScreen(int winner, float avgRT) {
  display.clearDisplay();

  drawCuteStar(16, 8);
  drawCuteStar(108, 8);
  drawCuteStar(28, 18);
  drawCuteStar(99, 18);
  drawHeart(19, 3);
  drawHeart(101, 3);

  drawTrophyCute(46, -7);

  display.setTextSize(2);
  display.setCursor(18, 28);
  display.print(F("PLAYER "));
  display.print(winner);

  display.setTextSize(1);
  display.setCursor(24, 46);
  display.print(F("P1:"));
  display.print(scoreP1);
  display.print(F("  P2:"));
  display.print(scoreP2);

  display.setCursor(18, 56);
  display.print(F("Avg RT: "));
  display.print(avgRT, 3);
  display.print(F("s"));

  display.display();
}

void oledTie() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(30, 0);
  display.println(F("GAME OVER!"));
  display.drawLine(0, 10, 128, 10, SH110X_WHITE);
  display.setTextSize(3);
  display.setCursor(28, 20);
  display.println(F("TIE!"));
  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print(F("P1:"));
  display.print(scoreP1);
  display.print(F("  P2:"));
  display.println(scoreP2);
  display.display();
}

// ============================================================
void setLED(int idx, int color) {
  digitalWrite(LED_R[idx], color == 1 ? HIGH : LOW);
  digitalWrite(LED_G[idx], color == 2 ? HIGH : LOW);
}
