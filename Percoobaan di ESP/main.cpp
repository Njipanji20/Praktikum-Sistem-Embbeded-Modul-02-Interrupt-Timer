#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ===== PIN =====
#define LED_PIN   2    // LED internal ESP32 (built-in)
#define BTN_PIN   4    // Push Button (PB) → GPIO4 ke GND

#define I2C_SDA   21
#define I2C_SCL   22

// ===== OLED =====
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===== DURASI MODE =====
#define ACTIVE_DURATION_MS  10000UL   // Mode Active : 10 detik
#define IDLE_DURATION_MS    15000UL   // Mode Idle   : 15 detik

// ===== MODE =====
enum Mode { ACTIVE, IDLE };
volatile Mode currentMode = ACTIVE;

// ===== STATE =====
volatile bool ledState    = false;
volatile bool btnEvent    = false;
uint32_t      modeStartMs = 0;

// ===== DEBOUNCE =====
volatile uint32_t lastBtnMs = 0;
const uint32_t    DEBOUNCE_MS = 80;

// ===== ISR Push Button =====
void IRAM_ATTR buttonISR() {
  uint32_t now = millis();
  if (now - lastBtnMs < DEBOUNCE_MS) return;
  lastBtnMs = now;

  // Hanya aktif saat Mode ACTIVE
  if (currentMode == ACTIVE) {
    btnEvent = true;
  }
}

// ===== Tampilan OLED =====
void updateOLED(uint32_t remaining) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);

  display.println(" ESP32  INTERRUPT ");
  display.println("------------------");
  display.print("Mode  : ");
  display.println(currentMode == ACTIVE ? "ACTIVE" : "IDLE");

  display.print("LED   : ");
  display.println(ledState ? "ON" : "OFF");

  display.print("Sisa  : ");
  display.print(remaining / 1000);
  display.println(" detik");

  display.print("PB    : ");
  display.println(currentMode == ACTIVE ? "AKTIF" : "NON-AKTIF");

  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);

  // I2C + OLED
  Wire.begin(I2C_SDA, I2C_SCL);

  // ===== I2C SCANNER =====
  Serial.println("Scanning I2C...");
  uint8_t foundAddr = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Device found at 0x%02X\n", addr);
      foundAddr = addr;
    }
  }
  if (foundAddr == 0) {
    Serial.println("  No I2C device found! Cek wiring SDA/SCL.");
    while (1) delay(1000);
  }
  Serial.printf("Pakai alamat: 0x%02X\n", foundAddr);
  // ===== END SCANNER =====

  if (!display.begin(foundAddr, true)) {
    Serial.println("OLED tidak terdeteksi! Cek wiring SDA/SCL.");
    while (1) delay(1000);
  }

  delay(100);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Starting...");
  display.display();

  // Interrupt PB: FALLING = saat tombol ditekan (aktif LOW)
  attachInterrupt(digitalPinToInterrupt(BTN_PIN), buttonISR, FALLING);

  // Mulai di Mode ACTIVE
  currentMode = ACTIVE;
  ledState    = false;
  modeStartMs = millis();

  Serial.println("=== START: Mode ACTIVE (10s) ===");
  Serial.println("Tekan PB untuk ON/OFF LED. Setelah 10s -> IDLE.");
}

void loop() {
  uint32_t now     = millis();
  uint32_t elapsed = now - modeStartMs;

  // ===== HANDLE BUTTON EVENT (hanya di ACTIVE) =====
  if (btnEvent) {
    noInterrupts();
    btnEvent = false;
    interrupts();

    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    Serial.printf("PB ditekan -> LED %s\n", ledState ? "ON" : "OFF");
  }

  // ===== TRANSISI MODE =====
  if (currentMode == ACTIVE && elapsed >= ACTIVE_DURATION_MS) {
    // ACTIVE -> IDLE
    currentMode = IDLE;
    ledState    = true;
    digitalWrite(LED_PIN, HIGH);  // LED menyala terus saat IDLE
    modeStartMs = now;
    Serial.println("=== Mode IDLE: LED ON terus, PB non-aktif. (15s) ===");

  } else if (currentMode == IDLE && elapsed >= IDLE_DURATION_MS) {
    // IDLE -> ACTIVE
    currentMode = ACTIVE;
    ledState    = false;
    digitalWrite(LED_PIN, LOW);
    modeStartMs = now;
    Serial.println("=== Mode ACTIVE: PB aktif toggle LED. (10s) ===");
  }

  // ===== HITUNG SISA WAKTU =====
  uint32_t duration  = (currentMode == ACTIVE) ? ACTIVE_DURATION_MS : IDLE_DURATION_MS;
  uint32_t remaining = (elapsed >= duration) ? 0 : (duration - elapsed);

  // ===== UPDATE OLED setiap 500ms =====
  static uint32_t lastOledMs = 0;
  if (now - lastOledMs >= 500) {
    lastOledMs = now;
    updateOLED(remaining);
    Serial.printf("[%s] LED=%s | Sisa=%lu s\n",
      currentMode == ACTIVE ? "ACTIVE" : "IDLE",
      ledState ? "ON" : "OFF",
      (unsigned long)(remaining / 1000));
  }
}