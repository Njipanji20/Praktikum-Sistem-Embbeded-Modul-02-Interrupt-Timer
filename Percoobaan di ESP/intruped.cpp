#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define LED_PIN PC13

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long previousMillis = 0;
const unsigned long interval = 2000; // 2 detik

uint32_t count = 0;
bool ledState = false;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Program Timer 2 Detik - STM32");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); // LED OFF (aktif LOW)

    // Inisialisasi OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED tidak terdeteksi!");
        while (1);
    }

    Serial.println("OLED terdeteksi.");

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Timer 2 Detik");
    display.println("LED: OFF");
    display.display();
}

void loop() {

    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        // Toggle LED
        ledState = !ledState;

        if (ledState) {
            digitalWrite(LED_PIN, LOW);  // LED ON (aktif LOW)
        } else {
            digitalWrite(LED_PIN, HIGH); // LED OFF
        }

        count++;

        // Serial Monitor
        Serial.print("Toggle ke- ");
        Serial.println(count);

        // Update OLED
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Timer 2 Detik");
        display.print("LED: ");
        display.println(ledState ? "ON" : "OFF");
        display.print("Count: ");
        display.println(count);
        display.display();
    }
}