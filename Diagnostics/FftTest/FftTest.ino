#include <arduinoFFT.h>

/* FFT Essentials */
#define AUDIO_IN         14
#define SAMPLES          512    // power of 2
#define SAMPLING_RATE    10000  // Hertz
#define LOWER_BOUND_MAG  80.0
#define ONE_SECOND       1000000
#define REFRESH_INTERVAL 0 // microseconds

/* Debug */
#define PLOT_WIDTH  500
#define BIN_START   2                // zero-based
#define BIN_END     SAMPLES / 2 - 1  // zero-based

#define DEBUG_MAG          false
#define DEBUG_PEAK         false

/* General */
unsigned long refreshTime;
int i;

/* Audio */
unsigned long samplingPeriod; // microseconds
unsigned long samplingTime;   // microseconds
double vReal[SAMPLES];
double vImag[SAMPLES];
arduinoFFT fft;
double peak;
double peakMag;

void setup() {
  Serial.begin(115200);

  fft = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_RATE);
  samplingPeriod = round(ONE_SECOND * (1.0 / SAMPLING_RATE));
}

void loop() {
  refreshTime = micros();

  getSamples();

  computeFFT();
  if (DEBUG_MAG) {
    plotvRealCentered();
  }

  recordPeakFreq();
  if (DEBUG_PEAK) {
    plotPeakFreq();
  }

  while (micros() - refreshTime < REFRESH_INTERVAL) {}
}

void getSamples() {
  for (i = 0; i < SAMPLES; i++) {
    samplingTime = micros();

    vReal[i] = analogRead(AUDIO_IN);
    vImag[i] = 0;

    while (micros() - samplingTime < samplingPeriod) {}
  }
}

void dcRemoval() {
  double vMean = 0;
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
  Serial.println(peak);
}
