#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =======================
// PIN BLUEPILL
// =======================
static const uint8_t PIN_LED = PC13; // LED internal (active LOW)
static const uint8_t PIN_PB  = PA0;  // tombol interrupt (pull-down eksternal)

// LED Bluepill active LOW
static inline void ledWrite(bool on) {
  digitalWrite(PIN_LED, on ? LOW : HIGH);
}

// =======================
// TIMER MODE
// =======================
static const uint32_t ACTIVE_MS = 10UL * 1000UL; // 10 detik
static const uint32_t IDLE_MS   = 20UL * 1000UL; // 20 detik (ubah ke 15 detik kalau perlu)

enum Mode : uint8_t { MODE_ACTIVE, MODE_IDLE };
volatile Mode mode = MODE_ACTIVE;

uint32_t modeStartMs = 0;
bool ledStateActive = false;

// =======================
// INTERRUPT BUTTON (pull-down eksternal)
// idle LOW, tekan HIGH => RISING
// =======================
volatile bool pbEvent = false;
volatile uint32_t lastIsrMs = 0;
static const uint32_t ISR_DEBOUNCE_MS = 50;

void onButtonISR() {
  uint32_t now = millis();
  if (now - lastIsrMs < ISR_DEBOUNCE_MS) return;
  lastIsrMs = now;
  pbEvent = true;
}

// =======================
// OLED SSD1306 I2C
// =======================
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// Banyak OLED SSD1306 pakai address 0x3C (kadang 0x3D)
static const uint8_t OLED_ADDR = 0x3C;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void oledDraw(Mode m, uint32_t remainingMs) {
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  // Judul mode
  display.setTextSize(2);
  display.setCursor(0, 0);
  if (m == MODE_ACTIVE) display.print("ACTIVE");
  else                 display.print("IDLE");

  // Countdown
  uint32_t sec = (remainingMs + 999) / 1000; // pembulatan ke atas
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("Sisa waktu: ");
  display.print(sec);
  display.print(" s");

  // Status LED (opsional biar jelas)
  display.setCursor(0, 45);
  display.print("LED: ");
  if (m == MODE_IDLE) {
    display.print("ON (forced)");
  } else {
    display.print(ledStateActive ? "ON" : "OFF");
  }

  display.display();
}

void setMode(Mode newMode) {
  mode = newMode;
  modeStartMs = millis();

  if (newMode == MODE_IDLE) {
    ledWrite(true); // IDLE: LED ON terus
  } else {
    ledWrite(ledStateActive); // ACTIVE: sesuai toggle terakhir
  }

  uint32_t dur = (newMode == MODE_ACTIVE) ? ACTIVE_MS : IDLE_MS;
  oledDraw(newMode, dur);
}

void setup() {
  pinMode(PIN_LED, OUTPUT);

  // Tombol pull-down eksternal: cukup INPUT
  pinMode(PIN_PB, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_PB), onButtonISR, RISING);

  // I2C start (default core STM32: PB6/PB7)
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // kalau OLED tidak terdeteksi, biarkan LED nyala sebagai indikasi error
    ledWrite(true);
    while (true) { delay(100); }
  }

  display.clearDisplay();
  display.display();

  setMode(MODE_ACTIVE);
}

void loop() {
  uint32_t now = millis();

  uint32_t dur = (mode == MODE_ACTIVE) ? ACTIVE_MS : IDLE_MS;
  uint32_t elapsed = now - modeStartMs;

  // Update OLED periodik (biar tidak berat)
  static uint32_t lastOledUpdate = 0;
  if (now - lastOledUpdate >= 200) {
    lastOledUpdate = now;
    uint32_t remaining = (elapsed >= dur) ? 0 : (dur - elapsed);
    oledDraw(mode, remaining);
  }

  // Pindah mode jika waktu habis
  if (elapsed >= dur) {
    if (mode == MODE_ACTIVE) setMode(MODE_IDLE);
    else setMode(MODE_ACTIVE);
  }

  // Handle tombol hanya saat ACTIVE
  if (pbEvent) {
    noInterrupts();
    bool ev = pbEvent;
    pbEvent = false;
    interrupts();

    if (ev && mode == MODE_ACTIVE) {
      ledStateActive = !ledStateActive;
      ledWrite(ledStateActive);
    }
  }

  // Safety: IDLE harus LED ON terus
  if (mode == MODE_IDLE) {
    ledWrite(true);
  }
}