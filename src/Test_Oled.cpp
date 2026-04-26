#include <Wire.h>
#include "DisplayUI.h"

// --- SETUP OLED ---
const uint8_t OLED_MUX = 1;
DisplayUI oled(OLED_MUX);

// --- SETUP BUTTON ---
const int MODE_BTN_PIN = 26; 
bool isMouseMode = false;
bool lastBtnState = HIGH;

// --- SETUP POTENTIOMETER PINS ---
// Extracted from your original Finger object initializations
const int THUMB_PIN = 32;
const int INDEX_PIN = 35;
const int MIDDLE_PIN = 34;
const int RING_PIN = 39;
const int PINKY_PIN = 36;

void setup() {
    Serial.begin(115200);
    Wire.begin();
    
    pinMode(MODE_BTN_PIN, INPUT_PULLUP);

    // Initialize analog pins
    pinMode(THUMB_PIN, INPUT);
    pinMode(INDEX_PIN, INPUT);
    pinMode(MIDDLE_PIN, INPUT);
    pinMode(RING_PIN, INPUT);
    pinMode(PINKY_PIN, INPUT);

    // Initialize the screen
    oled.init();
    
    Serial.println("--- POTENTIOMETER & OLED DIAGNOSTIC ---");
}

void loop() {
    // 1. --- BUTTON TOGGLE LOGIC ---
    bool currentBtnState = digitalRead(MODE_BTN_PIN);
    if (lastBtnState == HIGH && currentBtnState == LOW) {
        isMouseMode = !isMouseMode;
        Serial.println(isMouseMode ? "+++ MOUSE MODE UI +++" : "--- KEYBOARD MODE UI ---");
        delay(50); // Debounce
    }
    lastBtnState = currentBtnState;

    // 2. --- READ RAW POTENTIOMETER VALUES ---
    int tBend = analogRead(THUMB_PIN);
    int iBend = analogRead(INDEX_PIN);
    int mBend = analogRead(MIDDLE_PIN);
    int rBend = analogRead(RING_PIN);
    int pBend = analogRead(PINKY_PIN);

    // Print values to the Serial Monitor so you can calibrate your thresholds
    Serial.printf("Thumb:%d | Index:%d | Middle:%d | Ring:%d | Pinky:%d\n", tBend, iBend, mBend, rBend, pBend);

    // 3. --- CALCULATE VIRTUAL ROWS ---
    // Default to Home row (Row 1)
    int tRow = 0, iRow = 1, mRow = 1, rRow = 1, pRow = 1; 

    // Calculate Thumb (0 = Space, 1 = Backspace)
    if (tBend < 3900) tRow = 1; 

    // Calculate Index (0 = Top, 1 = Home, 2 = Bottom)
    if (iBend >= 3658) iRow = 0; 
    else if (iBend < 3483) iRow = 2; 

    // Calculate Middle
    if (mBend >= 2986) mRow = 0;
    else if (mBend < 2514) mRow = 2;

    // Calculate Ring
    if (rBend >= 2500) rRow = 0;
    else if (rBend < 2200) rRow = 2;

    // Calculate Pinky
    if (pBend >= 2656) pRow = 0;
    else if (pBend < 2656) pRow = 2; // (Using your Enter/P logic)

    // 4. --- UPDATE THE OLED ---
    // Note: We pass 'false' for the indexSpread parameter since we removed the MPU logic
    oled.updateUI(tRow, iRow, false, mRow, rRow, pRow, isMouseMode);

    // Small delay to make the Serial Monitor readable
    delay(100); 
}