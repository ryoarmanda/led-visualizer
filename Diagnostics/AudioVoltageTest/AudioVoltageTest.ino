#define AUDIO_IN 14

double vMax;
double vMin;

void setup() {
  Serial.begin(115200);
  vMax = 0;
  vMin = 1024;
}

void loop() {
  double v = analogRead(AUDIO_IN);
  if (v > vMax) {
    vMax = v;
  }
  if (v < vMin) {
    vMin = v;
  }
  Serial.print(vMax);
  Serial.print(" ");
  Serial.print(vMin);
  Serial.print(" ");
  Serial.println(v);
}
