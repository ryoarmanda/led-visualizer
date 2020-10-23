// Written to be used in Teensy 4.0

#include <FastLED.h>
#include <arduinoFFT.h>

// --- Macros ---

/* FFT Essentials */
#define AUDIO_IN         14
#define SAMPLES          512    // power of 2
#define SAMPLING_RATE    10000  // Hertz

/* LED Essentials */
#define DATA_OUT           17
#define LED_TYPE           WS2812B
#define COLOR_TYPE         GRB
#define NUM_STRIPS         1
#define NUM_LEDS_PER_STRIP 225
#define NUM_LEDS           NUM_STRIPS * NUM_LEDS_PER_STRIP

/* Animation Settings */
#define REFRESH_INTERVAL  0 // microseconds
#define LOWER_BOUND_MAG   80.0
#define UPPER_BOUND_MAG   30000.0
#define TRANSITION_LENGTH 3
#define COLOR_LENGTH      7
#define ANIMATION_LENGTH  COLOR_LENGTH + TRANSITION_LENGTH
#define ANIMATION_TIME    50  // milliseconds
#define ANIMATION_DELAY   round(ANIMATION_TIME / ANIMATION_LENGTH)
#define FADE_COLOR        false
#define TRANSITION_COLOR  true
#define FADE_TRANSITION   false
#define BASE_BRIGHTNESS   16 // out of 256
#define PEAK_BRIGHTNESS   32 // out of 256
#define MAX_BRIGHTNESS    255

/* Color Breakpoints */

#define COLOR_BREAKPOINTS 5
#define FREQ_START        20 
#define FREQ_END          SAMPLING_RATE / 2

typedef struct {
  int freq;
  CRGB color;
  double freqLog;
} ColorBreakpoint;

ColorBreakpoint colors[COLOR_BREAKPOINTS] = {
  {FREQ_START, CHSV(17, 245, 255), 0}, //orange
  {200, CHSV(0, 247, 255), 0}, //red
  {1500, CHSV(208, 214, 255), 0}, //violet
  {3000, CHSV(145, 217, 255), 0}, //blue
  {FREQ_END, CHSV(111, 237, 255), 0}, //pastel green
};

/* Utilities */
#define STATUS_LED_PIN 13
#define ONE_SECOND     1000000

/* Debug */
#define PLOT_WIDTH  500
#define BIN_START   2                // zero-based
#define BIN_END     SAMPLES / 2 - 1  // zero-based

#define DEBUG_VOLTAGE      true
#define DEBUG_MAG          false
#define DEBUG_PEAK         false
#define DEBUG_PEAK_MAG     false
#define DEBUG_PROCESS_TIME false
#define DEBUG_CYCLE_TIME   false

/* General */
unsigned long refreshTime;    // microseconds
int i, j;

/* Audio */
unsigned long samplingPeriod; // microseconds
unsigned long samplingTime;   // microseconds
double vReal[SAMPLES];
double vImag[SAMPLES];
double vMean;
arduinoFFT fft;
double peak;
double peakMag;

/* LED */
CRGB rawLeds[NUM_LEDS + ANIMATION_LENGTH];
CRGB leds[NUM_LEDS];
CRGB color;

/* Animation */
CRGB prevColor;
double colorFadeFactor[COLOR_LENGTH];
double transFadeFactor[COLOR_LENGTH];
int luma = 0;
int nextLuma = 0;
int deltaLuma;

void setup() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  Serial.begin(115200);

  // FFT Setup
  fft = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_RATE);
  samplingPeriod = round(ONE_SECOND * (1.0 / SAMPLING_RATE));

  // LED Setup
  FastLED.addLeds<NUM_STRIPS, LED_TYPE, DATA_OUT, COLOR_TYPE>(leds, NUM_LEDS_PER_STRIP);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 9000);
  FastLED.clear();
  FastLED.show();

  // Animation Setup
  for (i = 0; i < COLOR_LENGTH; i++) {
    colorFadeFactor[i] = cubicwave8(round((i + 1) * MAX_BRIGHTNESS * (1.0 / (COLOR_LENGTH + 1)))) * (1.0 / MAX_BRIGHTNESS);
  }

  for (i = 0; i < TRANSITION_LENGTH; i++) {
    transFadeFactor[i] = cubicwave8(round((i + 1) * MAX_BRIGHTNESS * (1.0 / (TRANSITION_LENGTH + 1)))) * (1.0 / MAX_BRIGHTNESS);
  }

  for (i = 0; i < COLOR_BREAKPOINTS; i++) {
    colors[i].freqLog = log(colors[i].freq);
  }
}

void loop() {
  refreshTime = micros();
  
  digitalWrite(STATUS_LED_PIN, HIGH);
  /* Input */
  getSamples();
  if (DEBUG_VOLTAGE) {
    plotvRealCentered();
  }

  /* FFT */
  computeFFT();
  if (DEBUG_MAG) {
    plotvRealCentered();
  }

  recordPeakFreq();
  if (DEBUG_PEAK) {
    plotPeakFreq();
  }
  if (DEBUG_PEAK_MAG) {
    plotPeakMag();
  }

  if (DEBUG_PROCESS_TIME) {
    printElapsedTime("Process time");
  }

  digitalWrite(STATUS_LED_PIN, LOW);
  /* Animation */
  prepColors();
  prepLuma();
  animate();
  if (DEBUG_CYCLE_TIME) {
    printElapsedTime("Cycle time");
  }

  while (micros() - refreshTime < REFRESH_INTERVAL) {}
}

/* Processing methods */

void getSamples() {
  for (i = 0; i < SAMPLES; i++) {
    samplingTime = micros();

    vReal[i] = analogRead(AUDIO_IN);
    vImag[i] = 0;

    while (micros() - samplingTime < samplingPeriod) {}
  }
}

void dcRemoval() {
  vMean = 0;
  for (i = 0; i < SAMPLES; i++) {
    vMean += vReal[i];
  }
  vMean /= SAMPLES;
  for (i = 0; i < SAMPLES; i++) {
    vReal[i] -= vMean;
  }
}

void computeFFT() {
  dcRemoval();
  fft.Windowing(FFT_WIN_TYP_HANN, FFT_FORWARD);
  fft.Compute(FFT_FORWARD);
  fft.ComplexToMagnitude();
}

void recordPeakFreq() {
  fft.MajorPeak(&peak, &peakMag);
  if (peakMag < LOWER_BOUND_MAG) {
    peak = 0;
  }
}

/* Animation methods */

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
  for (i = 1; i < COLOR_BREAKPOINTS; i++) {
    if (freq <= colors[i].freq) {
      double factor = (log(freq) - colors[i - 1].freqLog) / (colors[i].freqLog - colors[i - 1].freqLog);
      return transitionRGB(colors[i - 1].color, colors[i].color, factor);
    }
  }

  // Above range
  return CRGB::Black;
}

void prepColors() {
  prevColor = rawLeds[0]; // Design: First LED is always the previous base color
  color = getColor(peak);

  memmove(&rawLeds[ANIMATION_LENGTH], &rawLeds[0], NUM_LEDS * sizeof(CRGB));

  for (i = 0; i < ANIMATION_LENGTH; i++) {
    if (i < COLOR_LENGTH) {
      rawLeds[i] = color;
      if (FADE_COLOR) {
        rawLeds[i].nscale8(PEAK_BRIGHTNESS * colorFadeFactor[i]); // TODO: Change brightness
      }
    } else {
      rawLeds[i] = CRGB::Black;
      if (TRANSITION_COLOR) {
        rawLeds[i] = transitionRGB(color, prevColor, (i - COLOR_LENGTH) * (1.0 / TRANSITION_LENGTH));
        if (FADE_TRANSITION) {
          rawLeds[i].nscale8(PEAK_BRIGHTNESS * transFadeFactor[i]); // TODO: Change brightness
        }
      }
    }
  }
}

void prepLuma() {
//  luma = BASE_BRIGHTNESS;
  if (peakMag < LOWER_BOUND_MAG) {
    nextLuma = 0;
  } else if (peakMag > UPPER_BOUND_MAG) {
    nextLuma = PEAK_BRIGHTNESS;
  } else {
    nextLuma = round(
      (PEAK_BRIGHTNESS - BASE_BRIGHTNESS)
//      * ((peakMag - LOWER_BOUND_MAG) / (UPPER_BOUND_MAG - LOWER_BOUND_MAG))
      * (log(peakMag - LOWER_BOUND_MAG + 1) / log(UPPER_BOUND_MAG - LOWER_BOUND_MAG + 1))
      + BASE_BRIGHTNESS
    );
  }
}

void animate() {
  deltaLuma = (nextLuma - luma) / int(ANIMATION_LENGTH / 2);

  for (i = 0; i < ANIMATION_LENGTH; i++) {
    luma += deltaLuma;
    if (i == ANIMATION_LENGTH - 1) {
      // Last frame uses exact next luma
      luma = nextLuma;
    }

//    if (i < (int(ANIMATION_LENGTH) / 2 - 1)) {
//      luma += deltaLuma;
//    } else if (i > (int(ANIMATION_LENGTH) / 2)) {
//      luma -= deltaLuma;
//    } else {
//      luma = nextLuma;
//    }
    
    for (j = 0; j < NUM_LEDS; j++) {
      leds[j] = rawLeds[j + int(ANIMATION_LENGTH) - i - 1];
      leds[j].nscale8(luma);
    }

    FastLED.show();

    if (i < (int(ANIMATION_LENGTH) - 1)) {
      // Every frame before last needs delay
      delay(ANIMATION_DELAY);
    }
  }
}

/* Debug methods */

void plotvRealCentered() {
  int whitespace = (PLOT_WIDTH - (BIN_END - BIN_START + 1)) / 2;
  for (i = 0; i < PLOT_WIDTH; i++) {
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
  for (i = 0; i < COLOR_BREAKPOINTS; i++) {
    Serial.print(colors[i].freq);
    Serial.print(" ");
  }

  Serial.println(peak);
}

void plotPeakMag() {
  Serial.println(peakMag);
}

void printElapsedTime(char *header) {
  Serial.print(header);
  Serial.print(": ");
  Serial.print(micros() - refreshTime);
  Serial.println("us");
}
