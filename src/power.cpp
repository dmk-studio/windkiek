#include "power.h"

#include <esp_sleep.h>
#include "config.h"

int readBatteryPercent() {
  pinMode(BATTERY_EN_PIN, OUTPUT);
  digitalWrite(BATTERY_EN_PIN, HIGH);
  delay(10);
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(BATTERY_ADC_PIN);
  digitalWrite(BATTERY_EN_PIN, LOW);
  mv = mv / 8 * 2;  // 2:1 voltage divider
  Serial.printf("Battery: %lu mV\n", (unsigned long)mv);
  if (mv < 2500) return -1;  // no battery connected
  // rough linear approximation for LiPo: 3.3 V = 0 %, 4.15 V = 100 %
  return constrain(map(mv, 3300, 4150, 0, 100), 0, 100);
}

void led(bool on) {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, on ? LOW : HIGH);
}

uint32_t keyHoldMs(int pin, uint32_t maxMs) {
  const uint32_t start = millis();
  uint32_t held = 0;
  while (digitalRead(pin) == LOW && held < maxMs) {
    delay(20);
    held = millis() - start;
    if (held >= RESET_PRESS_MS) led((held / 150) % 2);
    else led(held >= LONG_PRESS_MS);
  }
  led(false);
  return held;
}

bool anyKeyPressed() {
  return digitalRead(KEY_NEXT) == LOW || digitalRead(KEY_REFRESH) == LOW || digitalRead(KEY_PREV) == LOW;
}

void waitForButtonsReleased() {
  // A key that is still pressed would trigger the ext1 wakeup right away.
  uint32_t start = millis();
  while (anyKeyPressed() && millis() - start < 15000) delay(20);
}

void deepSleepSeconds(uint32_t seconds) {
  Serial.printf("Deep sleep for %lu s\n", (unsigned long)seconds);
  Serial.flush();
  if (seconds) esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
  const uint64_t mask = (1ULL << KEY_NEXT) | (1ULL << KEY_REFRESH) | (1ULL << KEY_PREV);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}
