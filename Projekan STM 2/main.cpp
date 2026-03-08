#include <Arduino.h>
#include "stm32f1xx_hal.h"
#include <DHT.h>
#include <cstdio>
#include <cstring>

// ====== DHT22 ======
#define DHTPIN  PA1
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ====== I2C HAL handle ======
I2C_HandleTypeDef hi2c1;

// ================= OLED SSD1306 (I2C) =================
static uint8_t OLED_ADDR = 0x3C << 1; // kalau blank coba 0x3D<<1
static constexpr uint8_t OLED_W = 128;
static constexpr uint8_t OLED_H = 64;
static uint8_t fb[OLED_W * OLED_H / 8];

static void oled_cmd(uint8_t c) {
  uint8_t d[2] = {0x00, c};
  HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, d, 2, 100);
}

static void oled_data(const uint8_t* p, uint16_t len) {
  uint8_t buf[17];
  buf[0] = 0x40;
  while (len) {
    uint16_t n = (len > 16) ? 16 : len;
    memcpy(&buf[1], p, n);
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, n + 1, 100);
    p += n;
    len -= n;
  }
}

static void oled_clear() { memset(fb, 0, sizeof(fb)); }

static void oled_update() {
  oled_cmd(0x21); oled_cmd(0); oled_cmd(OLED_W - 1);
  oled_cmd(0x22); oled_cmd(0); oled_cmd((OLED_H / 8) - 1);
  oled_data(fb, sizeof(fb));
}

static void oled_pixel(uint8_t x, uint8_t y, bool on) {
  if (x >= OLED_W || y >= OLED_H) return;
  uint16_t i = x + (y / 8) * OLED_W;
  uint8_t bit = 1u << (y & 7);
  if (on) fb[i] |= bit;
  else    fb[i] &= ~bit;
}

// ================= FONT MINIMAL (cukup untuk "Counter: 3", "Temp: 29", "Hum : 70") =================

// digits 0-9 (5x7)
static const uint8_t DIG[10][5] = {
  {0x3E,0x51,0x49,0x45,0x3E}, //0
  {0x00,0x42,0x7F,0x40,0x00}, //1
  {0x42,0x61,0x51,0x49,0x46}, //2
  {0x21,0x41,0x45,0x4B,0x31}, //3
  {0x18,0x14,0x12,0x7F,0x10}, //4
  {0x27,0x45,0x45,0x45,0x39}, //5
  {0x3C,0x4A,0x49,0x49,0x30}, //6
  {0x01,0x71,0x09,0x05,0x03}, //7
  {0x36,0x49,0x49,0x49,0x36}, //8
  {0x06,0x49,0x49,0x29,0x1E}, //9
};

// glyphs minimal (5x7)
static const uint8_t SP[5]    = {0x00,0x00,0x00,0x00,0x00}; // space
static const uint8_t COL[5]   = {0x00,0x36,0x36,0x00,0x00}; // :
static const uint8_t C_[5]    = {0x3E,0x41,0x41,0x41,0x22}; // C
static const uint8_t H_[5]    = {0x7F,0x08,0x08,0x08,0x7F}; // H
static const uint8_t T_[5]    = {0x01,0x01,0x7F,0x01,0x01}; // T
static const uint8_t a_[5]    = {0x20,0x54,0x54,0x54,0x78}; // a
static const uint8_t e_[5]    = {0x38,0x54,0x54,0x54,0x18}; // e
static const uint8_t m_[5]    = {0x7C,0x04,0x18,0x04,0x78}; // m
static const uint8_t n_[5]    = {0x7C,0x08,0x04,0x04,0x78}; // n
static const uint8_t o_[5]    = {0x38,0x44,0x44,0x44,0x38}; // o
static const uint8_t p_[5]    = {0x7C,0x14,0x14,0x14,0x08}; // p
static const uint8_t r_[5]    = {0x7C,0x08,0x04,0x04,0x08}; // r
static const uint8_t t_[5]    = {0x04,0x3F,0x44,0x40,0x20}; // t
static const uint8_t u_[5]    = {0x3C,0x40,0x40,0x20,0x7C}; // u

static const uint8_t* glyph_for(char c) {
  if (c >= '0' && c <= '9') return DIG[c - '0'];
  switch (c) {
    case ' ': return SP;
    case ':': return COL;
    case 'C': return C_;
    case 'H': return H_;
    case 'T': return T_;
    case 'a': return a_;
    case 'e': return e_;
    case 'm': return m_;
    case 'n': return n_;
    case 'o': return o_;
    case 'p': return p_;
    case 'r': return r_;
    case 't': return t_;
    case 'u': return u_;
    default:  return SP; // karakter lain jadi spasi
  }
}

static void draw_char(uint8_t x, uint8_t y, char c) {
  const uint8_t* g = glyph_for(c);
  for (uint8_t col = 0; col < 5; col++) {
    uint8_t bits = g[col];
    for (uint8_t row = 0; row < 7; row++) {
      oled_pixel(x + col, y + row, (bits & (1u << row)) != 0);
    }
  }
}

static void draw_text(uint8_t x, uint8_t y, const char* s) {
  while (*s && x <= (OLED_W - 6)) {
    draw_char(x, y, *s++);
    x += 6;
  }
}

static void oled_init() {
  HAL_Delay(50);
  oled_cmd(0xAE);
  oled_cmd(0x20); oled_cmd(0x00);
  oled_cmd(0xA1);
  oled_cmd(0xC8);
  oled_cmd(0xA8); oled_cmd(0x3F);
  oled_cmd(0xD3); oled_cmd(0x00);
  oled_cmd(0x40);
  oled_cmd(0x8D); oled_cmd(0x14);
  oled_cmd(0xAF);
  oled_clear();
  oled_update();
}

// ================= I2C init (PB6/PB7) =================
static void I2C1_Init_Custom() {
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_I2C1_CLK_ENABLE();

  GPIO_InitTypeDef gi{};
  gi.Pin = GPIO_PIN_6 | GPIO_PIN_7;     // PB6 SCL, PB7 SDA
  gi.Mode = GPIO_MODE_AF_OD;
  gi.Pull = GPIO_PULLUP;
  gi.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gi);

  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;

  HAL_I2C_Init(&hi2c1);
}

// ================= Button interrupt (PC13) =================
volatile uint32_t counter = 0;
volatile uint32_t lastTick = 0;

void btn_isr() {
  uint32_t now = millis();
  if (now - lastTick >= 25) { // debounce cepat
    lastTick = now;
    counter++;
  }
}

// ================= MAIN =================
int main() {
  init();       // init Arduino core
  HAL_Init();

  // Button PC13 -> GND
  pinMode(PC13, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PC13), btn_isr, FALLING);

  // Sensors / OLED
  dht.begin();
  I2C1_Init_Custom();
  oled_init();

  uint32_t lastDrawCounter = 0xFFFFFFFF;
  uint32_t lastDhtMs = 0;
  int lastTemp = 0;
  int lastHum  = 0;

  while (1) {
    // baca DHT tiap 2 detik (DHT22 memang butuh pelan)
    if (millis() - lastDhtMs >= 2000) {
      lastDhtMs = millis();

      float t = dht.readTemperature();
      float h = dht.readHumidity();
      if (!isnan(t)) lastTemp = (int)(t + 0.5f);
      if (!isnan(h)) lastHum  = (int)(h + 0.5f);
    }

    // redraw kalau counter berubah atau tiap update DHT
    uint32_t snap = counter;
    if (snap != lastDrawCounter || (millis() - lastDhtMs) < 50) {
      lastDrawCounter = snap;

      char line1[24], line2[24], line3[24];
      snprintf(line1, sizeof(line1), "Counter: %lu", (unsigned long)snap);
      snprintf(line2, sizeof(line2), "Temp: %d", lastTemp);
      snprintf(line3, sizeof(line3), "Hum : %d", lastHum);

      oled_clear();
      draw_text(0, 0,  line1);
      draw_text(0, 16, line2);
      draw_text(0, 32, line3);
      oled_update();
    }
  }
}