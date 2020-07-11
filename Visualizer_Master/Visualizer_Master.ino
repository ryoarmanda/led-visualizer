// Originally written for Arduino Nano

#include <SoftwareSerial.h>
#include "Visualizer_Commons.h"

// Debug mode connects the board to PC, instead
// of to the HC-05 as usual
//#define DEBUG

#define AUDIO_IN A1
#define LED_PIN  13
#define BUF_SIZE 100

#ifdef DEBUG
  #define Master Serial
#else
  SoftwareSerial Master(2, 3);
#endif

char bufInput[BUF_SIZE];
char bufSample[BUF_SIZE];

int samples;
int samplingRate;
unsigned long samplingPeriod;
unsigned long samplingTime;

void setup() {
  Master.begin(BAUD_RATE);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  establishConnection();
}

void loop() {
  char cmd = readChar();

  switch (cmd) {
    case CONFIG_CMD:
      setupConfig();
      return;
    case AUDIO_CMD:
      sendSamples();
      return;
  }
}

/* Functions */

// Serial Input

bool readToBuf(bool *isComplete, int *bufIdx) {
  if (*bufIdx >= BUF_SIZE) return false;  // Buffer full
  if (*isComplete) return false;          // Buffer already contains a complete input

  digitalWrite(LED_PIN, HIGH);
  while (!Master.available()); // Block until data is available
  digitalWrite(LED_PIN, LOW);
  char c = Master.read();
  if (c == DELIM_CHAR) {
    c = '\0';
    *isComplete = true;
  }

  bufInput[*bufIdx] = c;
  *bufIdx = *bufIdx + 1;
  return true;
}

bool readToBufComplete() {
  bool isComplete = false;
  int bufIdx = 0;
  while (!isComplete) {
    if (!readToBuf(&isComplete, &bufIdx)) return false;
  }

  return isComplete;
}

char readChar() {
  if (!readToBufComplete()) return '\0';
  return bufInput[0];
}

int readUInt() {
  if (!readToBufComplete()) return -1;
  return atoi(bufInput);
}

// Analog Input

void sendSamples() {  
  for (int i = 0; i < samples; i++) {
    samplingTime = micros();

    sprintf(bufSample, "%d%c", analogRead(AUDIO_IN), DELIM_CHAR);
    Master.write(bufSample);

    while (micros() - samplingTime < samplingPeriod) {}
  }
  Master.flush();

#ifdef DEBUG
  Serial.println();
#endif
}

// Setup

void establishConnection() {
  while (!Master.available()) {
    Master.write(CONFIG_CMD);
    Master.write(DELIM_CHAR);
    Master.flush();
    delay(1000);
  }

  setupConfig();
}

void setupConfig() {
  if ((samples = readUInt()) == -1) raiseBlockingError("Samples");
  if ((samplingRate = readUInt()) == -1) raiseBlockingError("Sampling Rate");

  samplingPeriod = round(SECOND_TO_MICRO * (1.0 / samplingRate));

  Master.write('1');
  Master.write(DELIM_CHAR);
  Master.flush();

#ifdef DEBUG
  Serial.println();
#endif
}

void raiseBlockingError(char *msg) {
#ifdef DEBUG
  Serial.print("ERROR: ");
  Serial.println(msg);
#endif

  while (true) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
}
