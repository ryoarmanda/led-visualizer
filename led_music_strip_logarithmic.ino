#include <FastLED.h>
#include <arduinoFFT.h>

// --- Macros ---

/* FFT Essentials */
#define AUDIO_IN        15
#define SAMPLES         512  // power of 2
#define SAMPLING_RATE   5000 // Hz
#define REFRESH_INTERVAL 500000

/* FFT Additionals */
#define FREQ_PER_BIN round(SAMPLING_RATE * (1.0 / SAMPLES))
#define LOWER_BOUND  100.0
#define UPPER_BOUND  5000.0

/* Frequency Breakpoints */
#define FREQ_START     150  // Hz
#define FREQ_BASS_PEAK 300  // Hz
#define FREQ_MID_PEAK  1000 // Hz
#define FREQ_HIGH_PEAK 2000 // Hz

/* Sample Breakpoints */
#define BIN_START     FREQ_START     / FREQ_PER_BIN
#define BIN_BASS_PEAK FREQ_BASS_PEAK / FREQ_PER_BIN
#define BIN_MID_PEAK  FREQ_MID_PEAK  / FREQ_PER_BIN
#define BIN_HIGH_PEAK FREQ_HIGH_PEAK / FREQ_PER_BIN
#define BIN_END       SAMPLES / 2 - 1

/* LED Essentials */
#define DATA_OUT           7
#define LED_TYPE           WS2812B
#define COLOR_TYPE         GRB
#define NUM_STRIPS         1
#define NUM_LEDS_PER_STRIP 150
#define NUM_LEDS           NUM_STRIPS * NUM_LEDS_PER_STRIP

/* Animation Settings */
#define ANIMATION_STEPS 16
#define ANIMATION_DELAY 25 // milliseconds

/* Color Settings */
#define BASE_BRIGHTNESS 15  // out of 256
#define PEAK_BRIGHTNESS 128 // out of 256
#define COLOR_START     CHSV(25, 221, 255)
#define COLOR_BASS      CHSV(0, 247, 255)
#define COLOR_MID       CHSV(208, 214, 255)
#define COLOR_HIGH      CHSV(145, 217, 255)
#define COLOR_END       CHSV(0, 0, 255)

/* Utilities */
#define ONE_SECOND    1000000
#define WAVE_AMP      256
#define DEBUG_MAGS    false
#define DEBUG_FACTORS false

// --- Global Variables ---

unsigned long samplingPeriod; // microseconds
unsigned long samplingTime;   // microseconds
unsigned long refreshTime;    // microseconds
double vReal[SAMPLES];
double vImag[SAMPLES];
arduinoFFT fft = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_RATE);
CRGB colors[NUM_LEDS];
double mags[NUM_LEDS];
double peakMag;
double multipliers[ANIMATION_STEPS];
double factors[NUM_LEDS];
CRGB leds[NUM_LEDS];
double luma;

void setup() {
  Serial.begin(115200);
  
  samplingPeriod = round(ONE_SECOND * (1.0 / SAMPLING_RATE));

  FastLED.addLeds<NUM_STRIPS, LED_TYPE, DATA_OUT, COLOR_TYPE>(leds, NUM_LEDS_PER_STRIP);
  FastLED.clear();
  FastLED.show();

  for (int i = 0; i < NUM_LEDS; i++) {
    colors[i] = getColorByBin(idx2bin(i));
  }

  for (int i = 0; i < ANIMATION_STEPS; i++) {
    multipliers[i] = cubicwave8(round(i * WAVE_AMP * (1.0 / ANIMATION_STEPS))) * (1.0 / WAVE_AMP);
  }
}

void loop() {
  refreshTime = micros();
  
  /* INPUT */
  for (int i = 0; i < SAMPLES; i++) {
    samplingTime = micros();

    vReal[i] = analogRead(AUDIO_IN);
    vImag[i] = 0;

    while (micros() - samplingTime < samplingPeriod) {}
  }

  /* FFT */
  fft.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  fft.Compute(FFT_FORWARD);
  fft.ComplexToMagnitude();

  /* PREPROCESSING */
  for (int i = 0; i < NUM_LEDS; i++) {
    mags[i] = 0;
  }

  for (int i = BIN_START; i <= BIN_END; i++) {
    for (int j = bin2idx(i); j < min(bin2idx(i + 1), NUM_LEDS); j++) {
      mags[j] += vReal[i];
    }
  }

  peakMag = 0;
  for (int i = 0; i < NUM_LEDS; i++) {
    if (mags[i] > peakMag) {
      peakMag = mags[i];
    }
  }

  if (DEBUG_MAGS) {
    for (int i = 0; i < 500; i++) {
      Serial.print(LOWER_BOUND);
      Serial.print(" ");
      Serial.print(UPPER_BOUND);
      Serial.print(" ");
      if (i < 175 || i >= 175 + NUM_LEDS) {
        Serial.println(0);
      } else {
        Serial.println(mags[i - 175]);
      }
    }
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    factors[i] = 0;
    if (mags[i] > peakMag) {
      factors[i] = 1;
    } else if (mags[i] > LOWER_BOUND) {
      factors[i] = (mags[i] - LOWER_BOUND) / (peakMag - LOWER_BOUND);
//      factors[i] = log(mags[i] - LOWER_BOUND + 1) / log(UPPER_BOUND - LOWER_BOUND + 1);
    }
  }

  if (DEBUG_FACTORS) {
    for (int i = 0; i < 500; i++) {
      Serial.print(100);
      Serial.print(" ");
      if (i < 175 || i >= 175 + NUM_LEDS) {
        Serial.println(0);
      } else {
        Serial.println(factors[i - 175] * 100);
      }
    }
  }

  /* LED */
  for (int k = 0; k < ANIMATION_STEPS; k++) {    
    for (int i = 0; i < NUM_LEDS; i++) {
      luma = BASE_BRIGHTNESS + round((PEAK_BRIGHTNESS - BASE_BRIGHTNESS) * factors[i] * multipliers[k]);
      leds[i] = colors[i];
      leds[i].nscale8_video(luma);
    }

    FastLED.show();
    if (k < ANIMATION_STEPS - 1) {
      delay(ANIMATION_DELAY);
    }
  }

  while (micros() - refreshTime < REFRESH_INTERVAL) {}
}

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

CRGB getColorByBin(int bin) {
  double factor;

  if (bin < BIN_START) {
    return CRGB::Black;
  } else if (bin <= BIN_BASS_PEAK) {
    factor = (bin - BIN_START) * (1.0 / (BIN_BASS_PEAK - BIN_START));
    return transitionRGB(COLOR_START, COLOR_BASS, factor);    
  } else if (bin <= BIN_MID_PEAK) {
    factor = (bin - BIN_BASS_PEAK) * (1.0 / (BIN_MID_PEAK - BIN_BASS_PEAK));
    return transitionRGB(COLOR_BASS, COLOR_MID, factor);    
  } else if (bin <= BIN_HIGH_PEAK) {
    factor = (bin - BIN_MID_PEAK) * (1.0 / (BIN_HIGH_PEAK - BIN_MID_PEAK));
    return transitionRGB(COLOR_MID, COLOR_HIGH, factor);    
  } else if (bin <= BIN_END) {
    factor = (bin - BIN_HIGH_PEAK) * (1.0 / (BIN_END - BIN_HIGH_PEAK));
    return transitionRGB(COLOR_HIGH, COLOR_END, factor);    
  } else {
    return CRGB::Black;
  }

  return CRGB::Black;
}

int bin2idx(int bin) {
  return floor((NUM_LEDS - 1) * (log(bin - BIN_START + 1) / log(BIN_END - BIN_START + 1)));
}

int idx2bin(int idx) {
  return floor(pow(BIN_END - BIN_START + 1, idx * (1.0 / (NUM_LEDS - 1))) + BIN_START - 1);
}
