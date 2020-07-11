#include <FastLED.h>
#include <arduinoFFT.h>

// --- Macros ---

/* FFT Essentials */
#define AUDIO_IN        15
#define SAMPLES         128   // power of 2
#define SAMPLING_RATE   10000 // Hz

/* FFT Additionals */
#define FREQ_PER_BIN    round(SAMPLING_RATE * (1.0 / SAMPLES))
#define NOISE_THRESHOLD 75.0  // magnitude

/* Frequency Breakpoints */
#define FREQ_BASS_PEAK 300  // Hz
#define FREQ_MID_PEAK  1700 // Hz
#define FREQ_HIGH_PEAK 3500 // Hz

/* Sample Breakpoints */
#define BIN_START     1
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

/* Animation Settings */
#define ANIMATION_STEPS 16
#define ANIMATION_DELAY 25 // milliseconds
#define LED_PER_BIN     2

/* Color Settings */
#define BASE_BRIGHTNESS 30  // out of 256
#define PEAK_BRIGHTNESS 150 // out of 256
#define COLOR_START     CHSV(0, 0, 0)
#define COLOR_BASS      CHSV(0, 247, PEAK_BRIGHTNESS)
#define COLOR_MID       CHSV(208, 214, PEAK_BRIGHTNESS)
#define COLOR_HIGH      CHSV(145, 217, PEAK_BRIGHTNESS)
#define COLOR_END       CHSV(0, 0, PEAK_BRIGHTNESS)

/* Utilities */
#define ONE_SECOND   1000000
#define NUM_LEDS     NUM_STRIPS * NUM_LEDS_PER_STRIP
#define WAVE_AMP     256

// --- Global Variables ---

/* FFT globals */
unsigned long samplingPeriod; // microseconds
unsigned long samplingTime;   // microseconds
double vReal[SAMPLES];
double vImag[SAMPLES];
arduinoFFT fft = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_RATE);
double peak;
double peakAmp;

/* LED globals */
CRGB leds[NUM_LEDS];
double multiplier;
double factor;
double luma;

void setup() {
  Serial.begin(115200);

  samplingPeriod = round(ONE_SECOND * (1.0 / SAMPLING_RATE));

  FastLED.addLeds<NUM_STRIPS, LED_TYPE, DATA_OUT, COLOR_TYPE>(leds, NUM_LEDS_PER_STRIP);
  FastLED.clear();
  FastLED.show();
}

void loop() {
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
  fft.MajorPeak(&peak, &peakAmp);

  /* LED */
  for (int k = 1; k <= ANIMATION_STEPS; k++) {
    multiplier = cubicwave8(round(k * WAVE_AMP * (1.0 / ANIMATION_STEPS))) * (1.0 / WAVE_AMP);
    
    for (int i = BIN_START; i <= BIN_END; i++) {
      factor = 0;
      if (NOISE_THRESHOLD <= vReal[i] && vReal[i] <= peakAmp) {
        factor = (vReal[i] - NOISE_THRESHOLD) / (peakAmp - NOISE_THRESHOLD);
      }

      luma = BASE_BRIGHTNESS;
      if (factor > 0.1) {
        luma += round((PEAK_BRIGHTNESS - BASE_BRIGHTNESS) * factor * multiplier);
      }
      
      for (int j = 0; j < LED_PER_BIN; j++) {
        int idx = (i - BIN_START) * LED_PER_BIN + j;
        leds[idx] = getColor(i);
        leds[idx].nscale8(luma);
      }
    }

    FastLED.show();
    if (k < ANIMATION_STEPS) {
      delay(ANIMATION_DELAY);
    }
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

CRGB getColor(int bin) {
  double factor;

  if (bin <= BIN_BASS_PEAK) {
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
  }

  return CRGB::Black;
}
