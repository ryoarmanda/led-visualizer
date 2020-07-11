#include <FastLED.h>

/* LED Essentials */
#define DATA_OUT           17
#define LED_TYPE           WS2812B
#define COLOR_TYPE         GRB
#define NUM_STRIPS         1
#define NUM_LEDS_PER_STRIP 150
#define NUM_LEDS           NUM_STRIPS * NUM_LEDS_PER_STRIP

CRGB leds[NUM_LEDS];

void setup() {
  FastLED.addLeds<NUM_STRIPS, LED_TYPE, DATA_OUT, COLOR_TYPE>(leds, NUM_LEDS_PER_STRIP);
  FastLED.clear();
  FastLED.show();
}

void loop() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(0, 255, 50);
    FastLED.show();
    delay(10);
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(32, 255, 50);
    FastLED.show();
    delay(10);
  }
}
