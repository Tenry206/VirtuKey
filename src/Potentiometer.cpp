#include <Arduino.h>


const int thumbPin   = 32;
const int pointerPin = 35;
const int middlePin  = 34;
const int ringPin    = 39; // VN
const int pinkyPin   = 36; // VP



void setup() {
  Serial.begin(9600);

  delay(2000);
  Serial.println('Potentiometer Tracking');
}

void loop() {
  // put your main code here, to run repeatedly:
  int thumbBend   = analogRead(thumbPin);
  int pointerBend = analogRead(pointerPin);
  int middleBend  = analogRead(middlePin);
  int ringBend    = analogRead(ringPin);
  int pinkyBend   = analogRead(pinkyPin);

  // 2. Print the data to the Serial Monitor in a readable format
  Serial.print("Thumb: ");   Serial.print(thumbBend);
  Serial.print(" \t| Pointer: "); Serial.print(pointerBend);
  Serial.print(" \t| Middle: ");  Serial.print(middleBend);
  Serial.print(" \t| Ring: ");    Serial.print(ringBend);
  Serial.print(" \t| Pinky: ");   Serial.println(pinkyBend);

  delay(50);
}

