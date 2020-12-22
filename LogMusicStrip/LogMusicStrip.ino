#include <FastLED.h>
#include <arduinoFFT.h>

// --- Macros ---

/* FFT Essentials */
#define AUDIO_IN         14
#define SAMPLES          512    // power of 2
#define SAMPLING_RATE    10000   // Hz
#define REFRESH_INTERVAL 0  // microseconds

/* FFT Additionals */
#define FREQ_PER_BIN round(SAMPLING_RATE * (1.0 / SAMPLES))
#define LOWER_BOUND_MAG 100.0
#define LOWER_BOUND_DB  -20.0

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
#define DATA_OUT           17
#define LED_TYPE           WS2812B
#define COLOR_TYPE         GRB
#define NUM_STRIPS         1
#define NUM_LEDS_PER_STRIP 75
#define NUM_LEDS           NUM_STRIPS * NUM_LEDS_PER_STRIP

/* Animation Settings */
#define ANIMATION_STEPS 16
#define ANIMATION_DELAY 25 // milliseconds

/* Color Settings */
#define BASE_BRIGHTNESS 10  // out of 256
#define PEAK_BRIGHTNESS 64 // out of 256
#define COLOR_START     CHSV(25, 221, 255)
#define COLOR_BASS      CHSV(0, 247, 255)
#define COLOR_MID       CHSV(208, 214, 255)
#define COLOR_HIGH      CHSV(145, 217, 255)
#define COLOR_END       CHSV(0, 0, 255)

/* Utilities */
#define ONE_SECOND  1000000
#define WAVE_AMP    256

#define DEBUG_MAG          true
#define DEBUG_POWER        false
#define DEBUG_DB           false
#define DEBUG_PROCESS_TIME false
#define DEBUG_CYCLE_TIME   false

// --- Global Variables ---

/* General */
unsigned long refreshTime;    // microseconds

/* Audio */
unsigned long samplingPeriod; // microseconds
unsigned long samplingTime;   // microseconds
double vReal[SAMPLES];
double vImag[SAMPLES];
arduinoFFT fft = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_RATE);
double powers[NUM_LEDS];
double maxPower;

/* Processing */
double multipliers[ANIMATION_STEPS];
double factors[NUM_LEDS];

/* LED */
CRGB colors[NUM_LEDS];
CRGB leds[NUM_LEDS];
int luma;

void setup() {
  Serial.begin(115200);

  // FFT Setup
  samplingPeriod = round(ONE_SECOND * (1.0 / SAMPLING_RATE));

  // LED Setup
  FastLED.addLeds<NUM_STRIPS, LED_TYPE, DATA_OUT, COLOR_TYPE>(leds, NUM_LEDS_PER_STRIP);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 9000);
  for (int i = 0; i < NUM_LEDS; i++) {
    colors[NUM_LEDS - i - 1] = getColorByBin(idx2bin(i));
  }
  FastLED.show();

  // Animation Setup
  for (int i = 0; i < ANIMATION_STEPS; i++) {
    multipliers[i] = cubicwave8(round(i * WAVE_AMP * (1.0 / ANIMATION_STEPS))) * (1.0 / WAVE_AMP);
  }
}

void loop() {
  refreshTime = micros();
  
  /* Input */
  for (int i = 0; i < SAMPLES; i++) {
    samplingTime = micros();

    vReal[i] = analogRead(AUDIO_IN);
    vImag[i] = 0;

    while (micros() - samplingTime < samplingPeriod) {}
  }

  /* FFT */
  dcRemoval();
  fft.Windowing(FFT_WIN_TYP_HANN, FFT_FORWARD);
  fft.Compute(FFT_FORWARD);
  fft.ComplexToMagnitude();

  if (DEBUG_MAG) {
    int whitespaceWidth = (500 - (BIN_END - BIN_START + 1)) / 2;
    for (int i = 0; i < 500; i++) {
      Serial.print(LOWER_BOUND_MAG);
      Serial.print(" ");
      if (i < whitespaceWidth || i >= whitespaceWidth + (BIN_END - BIN_START + 1)) {
        Serial.println(0);
      } else {
        Serial.println(vReal[i - whitespaceWidth + BIN_START]);
      }
    }
  }

  filterMagnitude();
  magnitudeToPower();
  
  if (DEBUG_POWER) {
    int whitespaceWidth = (500 - (BIN_END - BIN_START + 1)) / 2;
    for (int i = 0; i < 500; i++) {
      if (i < whitespaceWidth || i >= whitespaceWidth + (BIN_END - BIN_START + 1)) {
        Serial.println(0);
      } else {
        Serial.println(vReal[i - whitespaceWidth + BIN_START]);
      }
    }
  }

  /* Processing */
  memset(powers, 0, sizeof(powers));
  memset(factors, 0, sizeof(factors));

  // Combine powers
  for (int i = BIN_START; i <= BIN_END; i++) {
    for (int j = bin2idx(i); j < min(bin2idx(i + 1), NUM_LEDS); j++) {
      powers[j] += vReal[i];
    }
  }

  // Find max power
  maxPower = 0;
  for (int i = 0; i < NUM_LEDS; i++) {
    if (powers[i] > maxPower) {
      maxPower = powers[i];
    }
  }

  if (maxPower > 0) {
    powerToDecibel();
    if (DEBUG_DB) {
      int whitespaceWidth = (500 - NUM_LEDS) / 2;
      for (int i = 0; i < 500; i++) {
        Serial.print(LOWER_BOUND_DB);
        Serial.print(" ");
        if (i < whitespaceWidth || i >= whitespaceWidth + NUM_LEDS) {
          Serial.println(-50);
        } else {
          Serial.println(powers[i - whitespaceWidth]);
        }
      }
    }
    filterDecibel();
    computeFactors();
  }

  if (DEBUG_PROCESS_TIME) {
    Serial.print("Process time: ");
    Serial.print(micros() - refreshTime);
    Serial.println("us");
  }
  
  /* LED Animation */
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

  if (DEBUG_CYCLE_TIME) {
    Serial.print("Cycle time: ");
    Serial.print(micros() - refreshTime);
    Serial.println("us");
  }

  while (micros() - refreshTime < REFRESH_INTERVAL) {}
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

void filterMagnitude() {
  for (int i = 0; i < SAMPLES; i++) {
    if (vReal[i] < LOWER_BOUND_MAG) {
      vReal[i] = 0;
    }
  }
}

void magnitudeToPower() {
  for (int i = 0; i < SAMPLES; i++) {
    vReal[i] = sq(vReal[i]);
  }
}

void powerToDecibel() {
  for (int i = 0; i < NUM_LEDS; i++) {
    if (powers[i] > 0) {
      powers[i] = 10 * log10(powers[i] / maxPower);
    } else {
      powers[i] = LOWER_BOUND_DB;
    }
  }
}

void filterDecibel() {
  for (int i = 0; i < NUM_LEDS; i++) {
    if (powers[i] < LOWER_BOUND_DB) {
      powers[i] = LOWER_BOUND_DB;
    }
  }
}

void computeFactors() {
  for (int i = 0; i < NUM_LEDS; i++) {
    factors[i] = (powers[i] - LOWER_BOUND_DB) / (-LOWER_BOUND_DB);
  }
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
}

int bin2idx(int bin) {
  return round((NUM_LEDS - 1) * (log(bin - BIN_START + 1) / log(BIN_END - BIN_START + 1)));
}

int idx2bin(int idx) {
  return floor(pow(BIN_END - BIN_START + 1, idx * (1.0 / (NUM_LEDS - 1))) + BIN_START - 1);
}
