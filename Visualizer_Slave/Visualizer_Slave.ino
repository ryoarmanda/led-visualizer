// Originally written for Teensy 4.0

#include <FastLED.h>
#include <arduinoFFT.h>
#include "Visualizer_Commons.h"

// --- Macros ---

/* Hardware */
#define INFO_PIN 13

/* Serial Input Essentials */
#define Slave Serial4
#define BUF_SIZE 100

/* FFT Essentials */
#define SAMPLES          512    // power of 2
#define SAMPLING_RATE    10000  // Hertz

/* LED Essentials */
#define DATA_OUT           7
#define LED_TYPE           WS2812B
#define COLOR_TYPE         GRB
#define NUM_STRIPS         1
#define NUM_LEDS_PER_STRIP 75
#define NUM_LEDS           NUM_STRIPS * NUM_LEDS_PER_STRIP

/* Animation Settings */
#define REFRESH_INTERVAL  0 // microseconds
#define LOWER_BOUND_MAG   80.0
#define TRANSITION_LENGTH 0
#define COLOR_LENGTH      7
#define ANIMATION_TIME    200 // milliseconds
#define ANIMATION_DELAY   round(ANIMATION_TIME / (TRANSITION_LENGTH + COLOR_LENGTH))
#define FADE_TRANSITION   false
#define FADE_COLOR        false
#define PEAK_BRIGHTNESS   15 // out of 256
#define WAVE_AMP          255

/* Debug */
#define PLOT_WIDTH  500
#define BIN_START   2                // zero-based
#define BIN_END     SAMPLES / 2 - 1  // zero-based

#define DEBUG_VOLTAGE      false
#define DEBUG_MAG          false
#define DEBUG_PEAK         false
#define DEBUG_PROCESS_TIME false
#define DEBUG_CYCLE_TIME   false

/* Color Breakpoints */
#define COLOR_BREAKPOINTS 5
#define FREQ_START        30
#define FREQ_END          SAMPLING_RATE / 2

typedef struct {
  int freq;
  CRGB color;
  double freqLogged;
} ColorBreakpoint;

ColorBreakpoint colors[COLOR_BREAKPOINTS] = {
  {FREQ_START, CHSV(0, 0, 0), 0},
  {150, CHSV(0, 247, 255), 0},
  {1000, CHSV(208, 214, 255), 0},
  {2000, CHSV(145, 217, 255), 0},
  {FREQ_END, CHSV(0, 0, 255), 0},
};

/* General */
unsigned long refreshTime;

/* Serial Input */
char bufInput[BUF_SIZE];
char bufConfig[BUF_SIZE];

/* Audio */
double vReal[SAMPLES];
double vImag[SAMPLES];
arduinoFFT fft;
double peak;
double peakMag;

/* Animation */
CRGB leds[NUM_LEDS];
CRGB color;
CRGB fadeFrom;
CRGB fadeTo;
double fadeFactor[COLOR_LENGTH];

void setup() {
  Slave.begin(BAUD_RATE);

  // Indicator Setup
  pinMode(INFO_PIN, OUTPUT);
  digitalWrite(INFO_PIN, LOW);

  // FFT Setup
  fft = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_RATE);

  // LED Setup
  FastLED.addLeds<NUM_STRIPS, LED_TYPE, DATA_OUT, COLOR_TYPE>(leds, NUM_LEDS_PER_STRIP);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 9000);
  FastLED.clear();
  FastLED.show();

  // Animation Setup
  for (int i = 0; i < COLOR_LENGTH; i++) {
    fadeFactor[i] = cubicwave8(round((i + 1) * WAVE_AMP * (1.0 / (COLOR_LENGTH + 1)))) * (1.0 / WAVE_AMP);
  }

  for (int i = 0; i < COLOR_BREAKPOINTS; i++) {
    colors[i].freqLogged = log10(colors[i].freq);
  }

  // Connect with Master
  establishConnection();
}

void loop() {
  refreshTime = micros();
  
  /* Input */
  getSamples();

  if (DEBUG_VOLTAGE) {
    plotvRealCentered();
  }

  /* FFT */
  dcRemoval();
  fft.Windowing(FFT_WIN_TYP_HANN, FFT_FORWARD);
  fft.Compute(FFT_FORWARD);
  fft.ComplexToMagnitude();

  if (DEBUG_MAG) {
    plotvRealCentered();
  }

  fft.MajorPeak(&peak, &peakMag);
  if (peakMag < LOWER_BOUND_MAG) {
    peak = 0;
  }

  if (DEBUG_PEAK) {
    plotPeakFreq();
  }

  if (DEBUG_PROCESS_TIME) {
    printElapsedTime("Process time");
  }

  /* ANIMATION */
  color = getColor(peak);

  fadeFrom = leds[0];
  fadeTo = color;
  if (FADE_COLOR) {
    fadeTo.nscale8_video(PEAK_BRIGHTNESS * fadeFactor[0]);
  } else {
    fadeTo.nscale8_video(PEAK_BRIGHTNESS);
  }

  for (int i = 0; i < TRANSITION_LENGTH; i++) {
    memmove(&leds[1], &leds[0], (NUM_LEDS - 1) * sizeof(CRGB));

    if (FADE_TRANSITION) {
      leds[0] = transitionRGB(fadeFrom, fadeTo, i * (1.0 / TRANSITION_LENGTH));
    } else {
      leds[0] = CRGB::Black;
    }
    
    FastLED.show();
    delay(ANIMATION_DELAY);
  }

  for (int i = 0; i < COLOR_LENGTH; i++) {
    memmove(&leds[1], &leds[0], (NUM_LEDS - 1) * sizeof(CRGB));
    
    leds[0] = color;
    if (FADE_COLOR) {
      leds[0].nscale8(PEAK_BRIGHTNESS * fadeFactor[i]);
    } else {
      leds[0].nscale8(PEAK_BRIGHTNESS);
    }

    FastLED.show();
    if (i < COLOR_LENGTH - 1) {
      delay(ANIMATION_DELAY);
    }
  }

  if (DEBUG_CYCLE_TIME) {
    printElapsedTime("Cycle time");
  }

  while (micros() - refreshTime < REFRESH_INTERVAL) {}
}

// --- Functions ---

/* Error */

void raiseBlockingError(char *msg) {
  Serial.print("ERROR: ");
  Serial.println(msg);

  while (true) {
    digitalWrite(INFO_PIN, HIGH);
    delay(200);
    digitalWrite(INFO_PIN, LOW);
    delay(200);
  }
}

/* Serial Input */

bool readToBuf(bool *bufComplete, int *bufIdx) {
  if (*bufIdx >= BUF_SIZE) return false; // Buffer full
  if (*bufComplete) return false;        // Buffer already contains a complete input

  digitalWrite(INFO_PIN, HIGH);
  while (!Slave.available()); // Block until data is available
  digitalWrite(INFO_PIN, LOW);
  char c = Slave.read();
  if (c == DELIM_CHAR) {
    c = '\0';
    *bufComplete = true;
  }

  bufInput[*bufIdx] = c;
  *bufIdx = *bufIdx + 1;
  return true;
}

bool readToBufComplete() {
  bool bufComplete = false;
  int bufIdx = 0;
  while (!bufComplete) {
    if (!readToBuf(&bufComplete, &bufIdx)) return false;
  }

  return bufComplete;
}

char readChar() {
  if (!readToBufComplete()) return '\0';
  return bufInput[0];
}

int readUInt() {
  if (!readToBufComplete()) return -1;
  return atoi(bufInput);
}

/* Setup & Config */

void establishConnection() {
  while (!Slave.available()) {
    sendConfig(false);
    delay(1000);
  }

  char c = readChar();
  if (c == '\0') raiseBlockingError("Establish connection");
}

void sendConfig(bool includeCmd) {
  sprintf(bufConfig, "%s%d%c%d%c", includeCmd ? "C " : "",
    SAMPLES, DELIM_CHAR, SAMPLING_RATE, DELIM_CHAR);
  Slave.write(bufConfig);
  Slave.flush();
  Serial.println("Sent config");
}

/* FFT */

void getSamples() {
  Slave.write("A ");
  Slave.flush();
  Serial.println("Request samples");
  int sample;
  for (int i = 0; i < SAMPLES; i++) {
    if ((sample = readUInt()) == -1) raiseBlockingError("Sampling");
    vReal[i] = sample;
    vImag[i] = 0;
  }
}

void dcRemoval() {
  double vMean = 0;
  for (int i = 0; i < SAMPLES; i++) {
    vMean += vReal[i];
  }
  vMean /= SAMPLES;
  for (int i = 0; i < SAMPLES; i++) {
    vReal[i] -= vMean;
  }
}

/* Animation */

int transition(int from, int to, double factor) {
  return round((1 - factor) * from + factor * to);
}

CRGB transitionRGB(CRGB from, CRGB to, double factor) {
  return CRGB(
    transition(from.r, to.r, factor),
    transition(from.g, to.g, factor),
    transition(from.b, to.b, factor)
  );
}

CRGB getColor(double freq) {
  // Below range
  if (freq < FREQ_START) {
    return CRGB::Black;
  }

  // In range
  for (int i = 1; i < COLOR_BREAKPOINTS; i++) {
    if (freq <= colors[i].freq) {
      double factor = (freq - colors[i - 1].freq) * (1.0 / (colors[i].freq - colors[i - 1].freq));
      return transitionRGB(colors[i - 1].color, colors[i].color, factor);
    }
  }

  // Above range
  return CRGB::Black;
}

/* Debug */

void plotvRealCentered() {
  int whitespace = (PLOT_WIDTH - (BIN_END - BIN_START + 1)) / 2;
  for (int i = 0; i < PLOT_WIDTH; i++) {
    if (DEBUG_MAG) {
      Serial.print(LOWER_BOUND_MAG);
      Serial.print(" ");
    }

    if (i < whitespace + BIN_START || i > whitespace + BIN_END) {
      Serial.println(0);
    } else {
      Serial.println(vReal[i - whitespace]);
    }
  }
}

void plotPeakFreq() {
  for (int i = 0; i < COLOR_BREAKPOINTS; i++) {
    Serial.print(colors[i].freq);
    Serial.print(" ");
  }

  Serial.println(peak);
}

void printElapsedTime(char *header) {
  Serial.print(header);
  Serial.print(": ");
  Serial.print(micros() - refreshTime);
  Serial.println("us");
}
