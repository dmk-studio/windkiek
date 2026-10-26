#pragma once
#include <Arduino.h>

int readBatteryPercent();  // -1 if no battery is detected (USB powered)
void led(bool on);
// How long the key stays pressed (up to maxMs). The LED lights up once LONG_PRESS_MS
// is reached and starts blinking at RESET_PRESS_MS, so the user knows when to let go.
uint32_t keyHoldMs(int pin, uint32_t maxMs);
bool anyKeyPressed();
void waitForButtonsReleased();
// Sleeps until the timer runs out or a key is pressed. seconds = 0: keys only.
[[noreturn]] void deepSleepSeconds(uint32_t seconds);
