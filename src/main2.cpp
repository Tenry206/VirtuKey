#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include "Finger.h"
#include "DisplayUI.h"
#include <BleCombo.h>

// ═════════════════════════════════════════════════════════════════════════════
//  FINGER INSTANCES
// ═════════════════════════════════════════════════════════════════════════════
Adafruit_MPU6050 mpu;

Finger thumb(7,       32, -3.5, 2.0, 0.90);
Finger indexFinger(6, 35, -5.0, 2.0, 0.90);
Finger middleFinger(5,34, -5.0, 2.0, 0.90);
Finger ringFinger(3,  39, -5.3, 3.0, 0.90);
Finger pinky(2,       36, -3.4, 4.0, 0.90);

const uint8_t HAND_MUX   = 4;
const uint8_t OLED_MUX   = 1;
DisplayUI oled(OLED_MUX);

const int MODE_BTN_PIN = 26;

// ═════════════════════════════════════════════════════════════════════════════
//  MODE STATE
//  0 = Keyboard   1 = Mouse   2 = Calibration
// ═════════════════════════════════════════════════════════════════════════════
int  currentMode  = 0;          // 0=keyboard  1=mouse  2=calib
bool lastBtnState = HIGH;

// ── Mouse drift correction ────────────────────────────────────────────────────
// HOW TO TUNE:
//   Open Serial Monitor while the glove is flat on a table.
//   Read the pitch/roll values printed for index finger.
//   Set MOUSE_PITCH_BIAS = that idle pitch value.
//   Set MOUSE_ROLL_BIAS  = that idle roll value.
//   The dead-zone (MOUSE_DEAD) filters out small wobble. Increase if cursor
//   still drifts; decrease if cursor feels sluggish at the start of a move.
//   MOUSE_SCALE controls how fast the cursor moves per degree beyond dead-zone.
float MOUSE_PITCH_BIAS = 0.0;   // <-- set to idle index pitch from Serial
float MOUSE_ROLL_BIAS  = 0.0;   // <-- set to idle index roll  from Serial
const float MOUSE_DEAD  = 4.0;  // degrees dead-zone (raise to stop drift)
const float MOUSE_SCALE = 2.0;  // pixels per degree beyond dead-zone

// ── Middle-finger drag (potentiometer threshold) ───────────────────────────────
// When middle finger bend ADC drops BELOW this value the finger is considered
// "curled" and the left mouse button is held (drag mode).
// Read the middle finger ADC from Serial while the finger is half-bent,
// and set this value to that reading.
int DRAG_BEND_THRESHOLD = 2514; // default = middle home-row threshold

bool isDragging = false;        // tracks current drag state

// ═════════════════════════════════════════════════════════════════════════════
//  CALIBRATION DATA
//  Stored thresholds for each finger.  Index: 0=thumb 1=index ... 4=pinky
//  straightVal = ADC when finger is straight (top row)
//  curledVal   = ADC when finger is curled   (bottom row)
//  homeVal     = midpoint, computed automatically
// ═════════════════════════════════════════════════════════════════════════════
int straightVal[5] = {3900, 3658, 2986, 2500, 2656};
int curledVal[5]   = {3300, 2593, 1542, 1600, 1970};
int homeVal[5]     = {3600, 3120, 2264, 2050, 2313};  // midpoints (auto-computed)

// Re-compute homeVal as midpoint and apply to bend-row logic
void applyCalibration() {
    for (int i = 0; i < 5; i++) {
        homeVal[i] = (straightVal[i] + curledVal[i]) / 2;
    }
}

// ── Calibration state machine ─────────────────────────────────────────────────
CalibStep calibStep        = CALIB_CONFIRM;
int       calibOption      = 0;    // highlighted option index
int       calibFingerIndex = 0;    // which finger we're calibrating (0-4)
int       calibStraightSnap = 0;   // sampled straight value
bool      lastBtnStateCalib = HIGH;// separate debounce for calib button reads
unsigned long lastBtnTime   = 0;
const unsigned long BTN_DEBOUNCE = 200; // ms

// Forward declarations
void runKeyboardMode(float handGyroZ);
void runMouseMode();
void runCalibMode();
bool buttonJustPressed();

// ═════════════════════════════════════════════════════════════════════════════
//  SETUP
// ═════════════════════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    Wire.begin();
    pinMode(MODE_BTN_PIN, INPUT_PULLUP);

    // Init all 5 finger MPUs (channel 4 = HAND_MUX is excluded)
    uint8_t activeChannels[] = {7, 6, 5, 3, 2};
    for (int i = 0; i < 5; i++) {
        Wire.beginTransmission(0x70);
        Wire.write(1 << activeChannels[i]);
        Wire.endTransmission();

        if (!mpu.begin()) {
            Serial.print("WARNING: No MPU6050 on Mux ch ");
            Serial.println(activeChannels[i]);
            continue;
        }
        mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }
    Serial.println("MPU6050s initialised.");

    thumb.init();
    indexFinger.init();
    middleFinger.init();
    ringFinger.init();
    pinky.init();

    oled.init();

    Keyboard.begin();
    Mouse.begin();
    Serial.println("BLE started. Waiting for connection.");
    Serial.println("TUNE: place glove flat on table, read idle pitch/roll for MOUSE_PITCH_BIAS / MOUSE_ROLL_BIAS");
}

// ═════════════════════════════════════════════════════════════════════════════
//  LOOP
// ═════════════════════════════════════════════════════════════════════════════
void loop() {
    // ── Button press: cycle modes 0→1→2→0 ───────────────────────────────────
    bool currentBtnState = digitalRead(MODE_BTN_PIN);
    bool btnFell = (lastBtnState == HIGH && currentBtnState == LOW);

    if (btnFell && currentMode != 2) {
        // In calib mode the button is handled inside runCalibMode()
        // Release any held mouse buttons when leaving mouse mode
        if (currentMode == 1 && Keyboard.isConnected()) {
            Mouse.release(MOUSE_LEFT);
            Mouse.release(MOUSE_RIGHT);
            isDragging = false;
        }
        currentMode = (currentMode + 1) % 3;
        if (currentMode == 2) {
            // Entering calibration — reset state machine
            calibStep        = CALIB_CONFIRM;
            calibOption      = 0;
            calibFingerIndex = 0;
        }
        Serial.print("Mode: ");
        Serial.println(currentMode == 0 ? "KEYBOARD" : currentMode == 1 ? "MOUSE" : "CALIBRATE");
        delay(50);
    }
    lastBtnState = currentBtnState;

    // ── Read hand (back-of-hand) IMU ─────────────────────────────────────────
    Wire.beginTransmission(0x70);
    Wire.write(1 << HAND_MUX);
    Wire.endTransmission();

    float handGyroZ = 0.0;
    sensors_event_t a_h, g_h, t_h;
    if (mpu.getEvent(&a_h, &g_h, &t_h))
        handGyroZ = g_h.gyro.z * 180.0 / PI;

    // ── Always update thumb and index; others only when needed ───────────────
    thumb.update(mpu, handGyroZ);
    indexFinger.update(mpu, handGyroZ);

    if (currentMode != 1) {   // keyboard or calib needs all fingers
        middleFinger.update(mpu, handGyroZ);
        ringFinger.update(mpu, handGyroZ);
        pinky.update(mpu, handGyroZ);
    } else {
        // In mouse mode still need middle for drag detection
        middleFinger.update(mpu, handGyroZ);
    }

    // ── Dispatch to mode ──────────────────────────────────────────────────────
    if (currentMode == 0)      runKeyboardMode(handGyroZ);
    else if (currentMode == 1) runMouseMode();
    else                       runCalibMode();

    // ── Serial telemetry for MATLAB (always) ─────────────────────────────────
    Serial.print("T:"); Serial.print(thumb.getPitch(),1);        Serial.print(",");
                        Serial.print(thumb.getRoll(),1);          Serial.print(",");
                        Serial.print(thumb.getBend());            Serial.print("|");
    Serial.print("I:"); Serial.print(indexFinger.getPitch(),1);  Serial.print(",");
                        Serial.print(indexFinger.getRoll(),1);    Serial.print(",");
                        Serial.print(indexFinger.getBend());      Serial.print(",");
                        Serial.print(indexFinger.getStretchedState() ? 1 : 0); Serial.print("|");
    Serial.print("M:"); Serial.print(middleFinger.getPitch(),1); Serial.print(",");
                        Serial.print(middleFinger.getRoll(),1);   Serial.print(",");
                        Serial.print(middleFinger.getBend());     Serial.print("|");
    Serial.print("R:"); Serial.print(ringFinger.getPitch(),1);   Serial.print(",");
                        Serial.print(ringFinger.getRoll(),1);     Serial.print(",");
                        Serial.print(ringFinger.getBend());       Serial.print("|");
    Serial.print("P:"); Serial.print(pinky.getPitch(),1);        Serial.print(",");
                        Serial.print(pinky.getRoll(),1);          Serial.print(",");
                        Serial.print(pinky.getBend());            Serial.print("|");
    Serial.print("MODE:"); Serial.println(currentMode);

    delay(20);
}

// ═════════════════════════════════════════════════════════════════════════════
//  KEYBOARD MODE
// ═════════════════════════════════════════════════════════════════════════════
void runKeyboardMode(float handGyroZ) {
    // ── Row classification using calibrated thresholds ───────────────────────
    // Row 0=Top  Row 1=Home  Row 2=Bottom
    // straightVal[i] > homeVal[i] > curledVal[i]  (straight = higher ADC)

    int tRow = 1, iRow = 1, mRow = 1, rRow = 1, pRow = 1;

    // Thumb (only 2 positions: 0=Space, 1=Backspace)
    if (thumb.getBend() >= straightVal[0]) tRow = 0;
    else                                   tRow = 1;

    // Index
    int iBend = indexFinger.getBend();
    if      (iBend >= straightVal[1]) iRow = 0;
    else if (iBend <  curledVal[1])   iRow = 2;

    // Middle
    int mBend = middleFinger.getBend();
    if      (mBend >= straightVal[2]) mRow = 0;
    else if (mBend <  curledVal[2])   mRow = 2;

    // Ring
    int rBend = ringFinger.getBend();
    if      (rBend >= straightVal[3]) rRow = 0;
    else if (rBend <  curledVal[3])   rRow = 2;

    // Pinky
    int pBend = pinky.getBend();
    if      (pBend >= straightVal[4]) pRow = 0;
    else if (pBend <  curledVal[4])   pRow = 2;

    oled.updateUI(tRow, iRow, indexFinger.getStretchedState(),
                  mRow, rRow, pRow, false);

    if (!Keyboard.isConnected()) return;

    // ── Thumb ─────────────────────────────────────────────────────────────────
    if (thumb.hasJustClicked()) {
        int bend = thumb.getBend();
        if (bend >= straightVal[0]) Keyboard.write(' ');
        else                        Keyboard.write(KEY_BACKSPACE);
    }

    // ── Index ─────────────────────────────────────────────────────────────────
    if (indexFinger.hasJustClicked()) {
        bool stretched = indexFinger.getStretchedState();
        int bend = indexFinger.getBend();
        if (stretched) {
            if      (bend >= straightVal[1])                   Keyboard.write('y');
            else if (bend >= curledVal[1] && bend < straightVal[1]) Keyboard.write('h');
            else                                               Keyboard.write('n');
        } else {
            if      (bend >= straightVal[1])                   Keyboard.write('u');
            else if (bend >= curledVal[1] && bend < straightVal[1]) Keyboard.write('j');
            else                                               Keyboard.write('m');
        }
    }

    // ── Middle ────────────────────────────────────────────────────────────────
    if (middleFinger.hasJustClicked()) {
        int bend = middleFinger.getBend();
        if      (bend >= straightVal[2]) Keyboard.write('i');
        else if (bend >= curledVal[2])   Keyboard.write('k');
        else                             Keyboard.write(',');
    }

    // ── Ring ──────────────────────────────────────────────────────────────────
    if (ringFinger.hasJustClicked()) {
        int bend = ringFinger.getBend();
        if      (bend >= straightVal[3]) Keyboard.write('o');
        else if (bend >= curledVal[3])   Keyboard.write('l');
        else                             Keyboard.write('.');
    }

    // ── Pinky ─────────────────────────────────────────────────────────────────
    if (pinky.hasJustClicked()) {
        int bend = pinky.getBend();
        if (bend >= straightVal[4]) Keyboard.write('p');
        else                        Keyboard.write(KEY_RETURN);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  MOUSE MODE
// ═════════════════════════════════════════════════════════════════════════════
void runMouseMode() {
    // ── Cursor movement (index finger pitch/roll) ─────────────────────────────
    // Subtract bias so the glove's natural resting angle becomes (0,0).
    float pitch = indexFinger.getPitch() - MOUSE_PITCH_BIAS;
    float roll  = indexFinger.getRoll()  - MOUSE_ROLL_BIAS;

    int moveX = 0, moveY = 0;

    // Apply dead-zone then scale
    if      (roll  >  MOUSE_DEAD) moveX = (int)((roll  - MOUSE_DEAD) * MOUSE_SCALE);
    else if (roll  < -MOUSE_DEAD) moveX = (int)((roll  + MOUSE_DEAD) * MOUSE_SCALE);
    if      (pitch >  MOUSE_DEAD) moveY = (int)((pitch - MOUSE_DEAD) * MOUSE_SCALE);
    else if (pitch < -MOUSE_DEAD) moveY = (int)((pitch + MOUSE_DEAD) * MOUSE_SCALE);

    if ((moveX != 0 || moveY != 0) && Keyboard.isConnected())
        Mouse.move(moveX, moveY);

    // ── Thumb click (L/R based on bend) ──────────────────────────────────────
    int tRow = (thumb.getBend() >= straightVal[0]) ? 0 : 1;

    if (thumb.hasJustClicked() && Keyboard.isConnected()) {
        if (tRow == 0) Mouse.click(MOUSE_RIGHT);
        else           Mouse.click(MOUSE_LEFT);
    }
    indexFinger.hasJustClicked(); // consume so it doesn't fire in keyboard mode

    // ── Middle finger drag ────────────────────────────────────────────────────
    // Curled (ADC < DRAG_BEND_THRESHOLD) → hold left button (drag)
    // Straight (ADC >= DRAG_BEND_THRESHOLD) → release
    int mBend = middleFinger.getBend();
    bool shouldDrag = (mBend < DRAG_BEND_THRESHOLD);

    if (Keyboard.isConnected()) {
        if (shouldDrag && !isDragging) {
            Mouse.press(MOUSE_LEFT);
            isDragging = true;
            Serial.println("DRAG START");
        } else if (!shouldDrag && isDragging) {
            Mouse.release(MOUSE_LEFT);
            isDragging = false;
            Serial.println("DRAG END");
        }
    }

    // ── Print live bias-correction data to Serial for tuning ─────────────────
    Serial.print("MOUSE_RAW pitch="); Serial.print(indexFinger.getPitch(),1);
    Serial.print("  roll=");          Serial.print(indexFinger.getRoll(),1);
    Serial.print("  corrected pitch="); Serial.print(pitch,1);
    Serial.print("  roll=");            Serial.println(roll,1);

    // ── OLED ──────────────────────────────────────────────────────────────────
    oled.updateUI(tRow, 1, false, 1, 1, 1, true);
}

// ═════════════════════════════════════════════════════════════════════════════
//  CALIBRATION MODE STATE MACHINE
// ═════════════════════════════════════════════════════════════════════════════
void runCalibMode() {
    // We handle the button ourselves inside this function so we can implement
    // the multi-step flow without interfering with the main mode-switch logic.

    bool currentBtnRaw = digitalRead(MODE_BTN_PIN);
    bool btnPressed    = false;
    unsigned long now  = millis();

    if (lastBtnState == HIGH && currentBtnRaw == LOW &&
        (now - lastBtnTime) > BTN_DEBOUNCE) {
        btnPressed  = true;
        lastBtnTime = now;
    }
    lastBtnState = currentBtnRaw;

    // Index finger bend used to scroll / toggle options
    int iBend = indexFinger.getBend();

    switch (calibStep) {

        // ── Step 0: Confirm entry ─────────────────────────────────────────────
        case CALIB_CONFIRM: {
            // Index curled → highlight "Yes", straight → highlight "Cancel"
            if (iBend < homeVal[1]) calibOption = 1;  // curled = Yes
            else                    calibOption = 0;  // straight = Cancel

            oled.updateCalibUI(CALIB_CONFIRM, calibOption, 0, 0);

            if (btnPressed) {
                if (calibOption == 0) {
                    // Cancel — return to keyboard mode
                    currentMode = 0;
                } else {
                    // Yes — move to finger selection
                    calibStep        = CALIB_SELECT_FINGER;
                    calibOption      = 0;   // start at Thumb
                    calibFingerIndex = 0;
                }
            }
            break;
        }

        // ── Step 1: Pick which finger ─────────────────────────────────────────
        case CALIB_SELECT_FINGER: {
            // Scroll through 5 fingers by bending index:
            //   Straight → scroll up (lower index), Curled → scroll down (higher index)
            // We use a simple threshold map: divide bend range into 5 zones
            int range = straightVal[1] - curledVal[1];
            if (range < 1) range = 1;
            int zone = (int)(5.0f * (float)(iBend - curledVal[1]) / (float)range);
            zone = 4 - constrain(zone, 0, 4);   // invert: straight=0(Thumb), curled=4(Pinky)
            calibOption      = zone;
            calibFingerIndex = zone;

            oled.updateCalibUI(CALIB_SELECT_FINGER, calibOption, calibFingerIndex, 0);

            if (btnPressed) {
                calibStep = CALIB_STEP_STRAIGHT;
            }
            break;
        }

        // ── Step 2: Sample straight position ──────────────────────────────────
        case CALIB_STEP_STRAIGHT: {
            // Read the chosen finger's bend live
            int liveVal = 0;
            switch (calibFingerIndex) {
                case 0: liveVal = thumb.getBend();        break;
                case 1: liveVal = indexFinger.getBend();  break;
                case 2: liveVal = middleFinger.getBend(); break;
                case 3: liveVal = ringFinger.getBend();   break;
                case 4: liveVal = pinky.getBend();        break;
            }

            oled.updateCalibUI(CALIB_STEP_STRAIGHT, 0, calibFingerIndex, liveVal);

            if (btnPressed) {
                calibStraightSnap         = liveVal;
                straightVal[calibFingerIndex] = liveVal;
                Serial.print("Straight sampled: "); Serial.println(liveVal);
                calibStep = CALIB_STEP_CURLED;
            }
            break;
        }

        // ── Step 3: Sample curled position ────────────────────────────────────
        case CALIB_STEP_CURLED: {
            int liveVal = 0;
            switch (calibFingerIndex) {
                case 0: liveVal = thumb.getBend();        break;
                case 1: liveVal = indexFinger.getBend();  break;
                case 2: liveVal = middleFinger.getBend(); break;
                case 3: liveVal = ringFinger.getBend();   break;
                case 4: liveVal = pinky.getBend();        break;
            }

            oled.updateCalibUI(CALIB_STEP_CURLED, 0, calibFingerIndex, liveVal);

            if (btnPressed) {
                curledVal[calibFingerIndex] = liveVal;
                Serial.print("Curled sampled:   "); Serial.println(liveVal);

                // Auto-compute home midpoint
                applyCalibration();

                Serial.print("New thresholds for finger ");
                Serial.print(calibFingerIndex);
                Serial.print(": straight="); Serial.print(straightVal[calibFingerIndex]);
                Serial.print("  home=");     Serial.print(homeVal[calibFingerIndex]);
                Serial.print("  curled=");   Serial.println(curledVal[calibFingerIndex]);

                calibStep = CALIB_DONE;
            }
            break;
        }

        // ── Step 4: Done — press button to exit ───────────────────────────────
        case CALIB_DONE: {
            oled.updateCalibUI(CALIB_DONE, 0, calibFingerIndex, 0);

            if (btnPressed) {
                // Return to keyboard mode
                currentMode = 0;
                Serial.println("Calibration complete. Returning to keyboard mode.");
            }
            break;
        }
    }
}