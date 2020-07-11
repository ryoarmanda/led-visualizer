#ifndef Visualizer_Commons_h /* Prevent loading library twice */
#define Visualizer_Commons_h
#ifdef ARDUINO
	#if ARDUINO >= 100
		#include "Arduino.h"
	#else
		#include "WProgram.h" /* This is where the standard Arduino code lies */
	#endif

#include "Arduino.h"

#define BAUD_RATE 115200

#define CONFIG_CMD 'C'
#define AUDIO_CMD  'A'
#define DELIM_CHAR ' '

#define SECOND_TO_MICRO 1000000

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
