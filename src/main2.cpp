// ─────────────────────────────────────────────────────────────────────────────
//  VirtuKey main.cpp
//  Modes: 0=Keyboard  1=Mouse  2=Calibration
//
//  SERIAL PLOTTER (PlatformIO):
//    In MOUSE mode, every line printed is pure Label:value,Label:value format
//    so the PlatformIO Serial Plotter can graph it directly.
//    Switch to Mouse mode and open the Serial Plotter — you will see
//    RawPitch, RawRoll, CorrPitch, CorrRoll, DeadZone, MiddleBend as live traces.
// ─────────────────────────────────────────────────────────────────────────────
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include "Finger.h"
#include "DisplayUI.h"
#include <BleCombo.h>

// ═══════════════ HARDWARE ════════════════════════════════════════════════════
Adafruit_MPU6050 mpu;

Finger thumb(7,        32, -3.5, 2.0, 0.90);
Finger indexFinger(6,  35, -5.0, 2.0, 0.90);
Finger middleFinger(5, 34, -5.0, 2.0, 0.90);
Finger ringFinger(3,   39, -5.3, 3.0, 0.90);
Finger pinky(2,        36, -3.4, 4.0, 0.90);

const uint8_t HAND_MUX = 4;
const uint8_t OLED_MUX = 1;
DisplayUI oled(OLED_MUX);
const int MODE_BTN_PIN = 26;

// ═══════════════ MODE ════════════════════════════════════════════════════════
int  currentMode  = 0;   // 0=keyboard  1=mouse  2=calibration
bool lastBtnState = HIGH;
unsigned long lastBtnTime    = 0;
const unsigned long BTN_DEBOUNCE_MS = 200;

// ═══════════════ MOUSE — GYRO-BASED (stable, no drift) ══════════════════════
//
//  WHY GYRO INSTEAD OF PITCH/ROLL ANGLE:
//    Pitch/roll from the complementary filter accumulates error over time and
//    drifts, especially at a fixed tilt. Raw gyro (deg/s) only outputs a signal
//    when the hand is MOVING — when still it reads ~0 (after bias removal),
//    so the cursor stays perfectly stationary with no drift at all.
//    This is the same approach used in the reference air-mouse project.
//
//  AXIS MAPPING for HORIZONTAL glove (hand lying flat, palm down):
//    Wrist roll left/right  → gyroZ (yaw)  → cursor X
//    Wrist tilt up/down     → gyroY (roll)  → cursor Y
//
//  AUTO-CALIBRATION:
//    Every time you switch INTO mouse mode the system waits 3 seconds,
//    collects 100 gyro samples, and computes the bias automatically.
//    Keep your hand still during the countdown shown on the OLED.
//    No manual tuning needed.

// Gyro bias (auto-computed on mouse mode entry — do not edit manually)
float mouse_gyroY_bias = 0.0;   // deg/s bias for X axis (cursor Y)
float mouse_gyroZ_bias = 0.0;   // deg/s bias for Z axis (cursor X)

// Deadzone in deg/s — gyro values below this are treated as zero (hand still)
// 15 deg/s is a good starting point. Raise if cursor still creeps; lower for sensitivity.
const float MOUSE_GYRO_DEAD  = 12.0;

// Speed scale: how many pixels to move per deg/s beyond the deadzone
// 0.05 is smooth. Raise for faster cursor. Lower for precision work.
const float MOUSE_GYRO_SCALE = 0.35f;

// EMA smoothing factor (0=max smooth/laggy, 1=raw/snappy) — same as reference project
const float MOUSE_EMA = 0.2f;

// Smoothed gyro values (persistent between loop calls)
float mouse_filtY = 0.0;
float mouse_filtZ = 0.0;

// Calibration state
bool  mouseCalibrated   = false;   // true after first calibration completes
bool  mouseCalibRunning = false;   // true while collecting samples
unsigned long calibStartTime = 0;

// ═══════════════ MIDDLE FINGER DRAG ══════════════════════════════════════════
//  Curl middle finger below DRAG_BEND_THRESHOLD → hold left button (drag mode).
//  Straighten above threshold → release.
int  DRAG_BEND_THRESHOLD = 2514;
bool isDragging = false;

// ═══════════════ CALIBRATION THRESHOLDS ══════════════════════════════════════
//  posVal[finger][position] — ADC reading at each position
//  Finger 0=Thumb  1=Index  2=Middle  3=Ring  4=Pinky
//  Thumb & Pinky: 2 positions (straight=top, curled=bottom)
//  Index, Middle, Ring: 3 positions (straight=top, home=mid, curled=bottom)
const int MAX_POS = 3;
int posVal[5][MAX_POS] = {
    {3900, 3300,    0},   // Thumb  [straight, curled,   —  ]
    {3658, 3120, 2593},   // Index  [straight, home,  curled]
    {2986, 2264, 1542},   // Middle [straight, home,  curled]
    {2500, 2050, 1600},   // Ring   [straight, home,  curled]
    {2656, 1970,    0},   // Pinky  [straight, curled,   —  ]
};
const int stepsForFinger[5] = {2, 3, 3, 3, 2};

// Midpoint threshold between position p and p+1 for finger fi
int getThreshold(int fi, int p) {
    return (posVal[fi][p] + posVal[fi][p + 1]) / 2;
}

// Return row index: 0=top, 1=home(or bottom for 2-step), 2=bottom
int classifyRow(int fi, int rawBend) {
    if (stepsForFinger[fi] == 2) {
        return (rawBend >= getThreshold(fi, 0)) ? 0 : 1;
    }
    if      (rawBend >= getThreshold(fi, 0)) return 0;
    else if (rawBend >= getThreshold(fi, 1)) return 1;
    else                                     return 2;
}

// ═══════════════ CALIBRATION STATE ═══════════════════════════════════════════
CalibStep calibStep        = CALIB_CONFIRM;
int       calibOption      = 0;
int       calibFingerIndex = 0;
int       calibPosStep     = 0;

CalibStep posStepToEnum(int ps) {
    if (ps == 0) return CALIB_STEP_1;
    if (ps == 1) return CALIB_STEP_2;
    return CALIB_STEP_3;
}

// Forward declarations
void runKeyboardMode();
void runMouseMode();
void runCalibMode();

// ═══════════════ SETUP ═══════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    Wire.begin();
    pinMode(MODE_BTN_PIN, INPUT_PULLUP);

    uint8_t activeChannels[] = {7, 6, 5, 3, 2};
    for (int i = 0; i < 5; i++) {
        Wire.beginTransmission(0x70);
        Wire.write(1 << activeChannels[i]);
        Wire.endTransmission();
        if (!mpu.begin()) {
            Serial.print("WARNING: No MPU6050 ch ");
            Serial.println(activeChannels[i]);
            continue;
        }
        mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }

    thumb.init();
    indexFinger.init();
    middleFinger.init();
    ringFinger.init();
    pinky.init();

    oled.init();
    Keyboard.begin();
    Mouse.begin();
    Serial.println("VirtuKey ready. Press button to cycle modes: Keyboard->Mouse->Calibrate");
}

// ═══════════════ LOOP ════════════════════════════════════════════════════════
void loop() {
    // ── Button: cycle modes 0→1→2→0 (calib handles its own button inside) ────
    bool curBtn  = digitalRead(MODE_BTN_PIN);
    bool btnFell = (lastBtnState == HIGH && curBtn == LOW
                    && (millis() - lastBtnTime) > BTN_DEBOUNCE_MS);

    if (btnFell && currentMode != 2) {
        if (currentMode == 1 && Keyboard.isConnected()) {
            Mouse.release(MOUSE_LEFT);
            Mouse.release(MOUSE_RIGHT);
            isDragging = false;
        }
        currentMode = (currentMode + 1) % 3;
        if (currentMode == 1) {
            // Entering mouse mode — trigger auto-calibration
            mouseCalibrated   = false;
            mouseCalibRunning = false;
            mouse_filtY = 0.0;
            mouse_filtZ = 0.0;
        }
        if (currentMode == 2) {
            calibStep = CALIB_CONFIRM; calibOption = 0;
            calibFingerIndex = 0;      calibPosStep = 0;
        }
        lastBtnTime = millis();
        delay(50);
    }
    lastBtnState = curBtn;

    // ── Back-of-hand IMU ──────────────────────────────────────────────────────
    Wire.beginTransmission(0x70);
    Wire.write(1 << HAND_MUX);
    Wire.endTransmission();

    float handGyroZ = 0.0;
    sensors_event_t ah, gh, th2;
    if (mpu.getEvent(&ah, &gh, &th2))
        handGyroZ = gh.gyro.z * 180.0 / PI;

    // ── Update finger sensors ─────────────────────────────────────────────────
    thumb.update(mpu, handGyroZ);
    indexFinger.update(mpu, handGyroZ);
    middleFinger.update(mpu, handGyroZ); // needed in all modes for drag + calib
    if (currentMode != 1) {              // ring & pinky not used in mouse mode
        ringFinger.update(mpu, handGyroZ);
        pinky.update(mpu, handGyroZ);
    }

    // ── Dispatch to current mode ──────────────────────────────────────────────
    if      (currentMode == 0) runKeyboardMode();
    else if (currentMode == 1) runMouseMode();     // prints its own serial data
    else                       runCalibMode();

    // ── MATLAB telemetry (keyboard and calib only) ────────────────────────────
    // In mouse mode this block is skipped so the Serial Plotter only receives
    // the clean Label:value lines from runMouseMode() and can graph them.
    if (currentMode != 1) {
        Serial.print("T:"); Serial.print(thumb.getPitch(),1);        Serial.print(",");
                            Serial.print(thumb.getRoll(),1);          Serial.print(",");
                            Serial.print(thumb.getBend());            Serial.print("|");
        Serial.print("I:"); Serial.print(indexFinger.getPitch(),1);  Serial.print(",");
                            Serial.print(indexFinger.getRoll(),1);    Serial.print(",");
                            Serial.print(indexFinger.getBend());      Serial.print(",");
                            Serial.print(indexFinger.getStretchedState()?1:0); Serial.print("|");
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
    }

    delay(20);
}

// ═══════════════ KEYBOARD MODE ═══════════════════════════════════════════════
void runKeyboardMode() {
    int tRow = classifyRow(0, thumb.getBend());
    int iRow = classifyRow(1, indexFinger.getBend());
    int mRow = classifyRow(2, middleFinger.getBend());
    int rRow = classifyRow(3, ringFinger.getBend());
    int pRow = classifyRow(4, pinky.getBend());

    oled.updateUI(tRow, iRow, indexFinger.getStretchedState(),
                  mRow, rRow, pRow, false);

    if (!Keyboard.isConnected()) return;

    if (thumb.hasJustClicked())
        Keyboard.write(tRow == 0 ? ' ' : KEY_BACKSPACE);

    if (indexFinger.hasJustClicked()) {
        bool sp = indexFinger.getStretchedState();
        char keys[2][3] = {{'y','h','n'},{'u','j','m'}};
        Keyboard.write(keys[sp?0:1][classifyRow(1, indexFinger.getBend())]);
    }

    if (middleFinger.hasJustClicked()) {
        char keys[3] = {'i','k',','};
        Keyboard.write(keys[classifyRow(2, middleFinger.getBend())]);
    }

    if (ringFinger.hasJustClicked()) {
        char keys[3] = {'o','l','.'};
        Keyboard.write(keys[classifyRow(3, ringFinger.getBend())]);
    }

    if (pinky.hasJustClicked())
        Keyboard.write(pRow == 0 ? 'p' : KEY_RETURN);
}

// ═══════════════ MOUSE MODE ══════════════════════════════════════════════════
//
//  STATE MACHINE:
//    Phase 1 — COUNTDOWN (3 s): OLED shows "Hold still… X s"
//              Index finger IMU samples 100 gyro readings → compute bias
//    Phase 2 — ACTIVE: bias-corrected gyro → EMA filter → deadzone → move
//
void runMouseMode() {

    // ── Phase 1: Auto-calibration countdown ───────────────────────────────────
    if (!mouseCalibrated) {

        if (!mouseCalibRunning) {
            // First call after entering mouse mode — start the countdown
            mouseCalibRunning = true;
            calibStartTime    = millis();
            Serial.println("Mouse calibration: hold hand STILL for 3 seconds...");
        }

        unsigned long elapsed = millis() - calibStartTime;
        int secsLeft = 3 - (int)(elapsed / 1000);

        // Show countdown on OLED using the keyboard UI as a blank canvas
        // (reuse updateUI with all rows at home, isMouseMode=false so we can
        //  draw on top — simpler than adding a new OLED method)
        // We call selectMux directly via a simple wrapper approach:
        // Just show mouse mode screen with a countdown number in thumb row
        oled.updateUI(secsLeft > 0 ? secsLeft : 0, 1, false, 1, 1, 1, true);

        // Collect samples during the 3-second window
        // Switch mux to index finger channel before reading
        Wire.beginTransmission(0x70);
        Wire.write(1 << 6);  // index finger mux channel
        Wire.endTransmission();

        sensors_event_t a, g, temp;
        static long sumGyroY = 0, sumGyroZ = 0;
        static int  sampleCount = 0;

        if (elapsed < 3000) {
            // Still collecting
            if (mpu.getEvent(&a, &g, &temp)) {
                sumGyroY   += (long)(g.gyro.y * 180.0 / PI * 100); // ×100 to keep precision
                sumGyroZ   += (long)(g.gyro.z * 180.0 / PI * 100);
                sampleCount++;
            }
            Serial.print("CalibSamples:"); Serial.print(sampleCount);
            Serial.print(",SecsLeft:");    Serial.println(secsLeft);
            return;  // stay in calibration phase
        }

        // 3 seconds elapsed — compute bias from collected samples
        if (sampleCount > 0) {
            mouse_gyroY_bias = (float)sumGyroY / (float)sampleCount / 100.0f;
            mouse_gyroZ_bias = (float)sumGyroZ / (float)sampleCount / 100.0f;
        }
        // Reset statics for next calibration
        sumGyroY    = 0; sumGyroZ    = 0; sampleCount = 0;

        mouseCalibrated   = true;
        mouseCalibRunning = false;
        mouse_filtY = 0.0;
        mouse_filtZ = 0.0;

        Serial.print("Mouse bias set — gyroY_bias="); Serial.print(mouse_gyroY_bias, 3);
        Serial.print("  gyroZ_bias=");                Serial.println(mouse_gyroZ_bias, 3);
        return;
    }

    // ── Phase 2: Active mouse movement ────────────────────────────────────────
    //
    //  AXIS MAPPING (glove horizontal, palm down):
    //    gyroY (deg/s) = wrist tips up/down     → cursor Y  (tilt up = move up)
    //    gyroZ (deg/s) = wrist rotates left/right→ cursor X  (rotate left = move left)
    //
    //  Bias correction → deadzone → EMA smooth → scale → Mouse.move()
    //  Identical algorithm to the reference air-mouse project.

    // Read fresh raw gyro from index finger IMU
    Wire.beginTransmission(0x70);
    Wire.write(1 << 6);  // index finger mux channel
    Wire.endTransmission();

    sensors_event_t a, g, temp;
    float gy = 0.0, gz = 0.0;
    if (mpu.getEvent(&a, &g, &temp)) {
        gy = g.gyro.y * 180.0 / PI;   // deg/s
        gz = g.gyro.z * 180.0 / PI;   // deg/s
    }

    // Remove per-session bias
    gy -= mouse_gyroY_bias;
    gz -= mouse_gyroZ_bias;

    // Deadzone — zero out small values so a still hand = no movement
    if (abs(gy) < MOUSE_GYRO_DEAD) gy = 0.0;
    if (abs(gz) < MOUSE_GYRO_DEAD) gz = 0.0;

    // EMA low-pass filter — same as reference project (alpha=0.2)
    mouse_filtY = MOUSE_EMA * gy + (1.0f - MOUSE_EMA) * mouse_filtY;
    mouse_filtZ = MOUSE_EMA * gz + (1.0f - MOUSE_EMA) * mouse_filtZ;

    // Scale to pixel movement
    // Negate Z so rotating wrist left moves cursor left (adjust sign if inverted)
    int moveX = -(int)(mouse_filtZ * MOUSE_GYRO_SCALE);
    int moveY = (int)(mouse_filtY * MOUSE_GYRO_SCALE);

    if ((moveX || moveY) && Keyboard.isConnected())
        Mouse.move(moveX, moveY);

    // ── Thumb click ───────────────────────────────────────────────────────────
    int tRow = classifyRow(0, thumb.getBend());
    if (thumb.hasJustClicked() && Keyboard.isConnected())
        Mouse.click(tRow == 0 ? MOUSE_LEFT : MOUSE_RIGHT);

    indexFinger.hasJustClicked(); // consume

    // ── Middle finger drag ────────────────────────────────────────────────────
    bool shouldDrag = (middleFinger.getBend() < DRAG_BEND_THRESHOLD);
    if (Keyboard.isConnected()) {
        if (shouldDrag && !isDragging) {
            Mouse.press(MOUSE_LEFT);
            isDragging = true;
        } else if (!shouldDrag && isDragging) {
            Mouse.release(MOUSE_LEFT);
            isDragging = false;
        }
    }

    // ── Serial Plotter output (Label:value CSV — PlatformIO plotter compatible)
    Serial.print("RawGyroY:");    Serial.print(gy + mouse_gyroY_bias, 2); Serial.print(",");
    Serial.print("RawGyroZ:");    Serial.print(gz + mouse_gyroZ_bias, 2); Serial.print(",");
    Serial.print("CorrGyroY:");   Serial.print(mouse_filtY, 2);           Serial.print(",");
    Serial.print("CorrGyroZ:");   Serial.print(mouse_filtZ, 2);           Serial.print(",");
    Serial.print("DeadZone:");    Serial.print(MOUSE_GYRO_DEAD);          Serial.print(",");
    Serial.print("NegDeadZone:"); Serial.print(-MOUSE_GYRO_DEAD);         Serial.print(",");
    Serial.print("MiddleBend:");  Serial.println(middleFinger.getBend());

    oled.updateUI(tRow, 1, false, 1, 1, 1, true);
}

// ═══════════════ CALIBRATION MODE ════════════════════════════════════════════
void runCalibMode() {
    bool curBtn    = digitalRead(MODE_BTN_PIN);
    bool btnPressed = (lastBtnState == HIGH && curBtn == LOW
                       && (millis() - lastBtnTime) > BTN_DEBOUNCE_MS);
    if (btnPressed) lastBtnTime = millis();
    lastBtnState = curBtn;

    int iBend   = indexFinger.getBend();
    int totalSt = stepsForFinger[calibFingerIndex];

    switch (calibStep) {

        case CALIB_CONFIRM: {
            calibOption = (iBend < getThreshold(1, 0)) ? 1 : 0;
            oled.updateCalibUI(CALIB_CONFIRM, calibOption, 0, 0, 0);
            if (btnPressed) {
                if (calibOption == 0) { currentMode = 0; }
                else {
                    calibStep = CALIB_SELECT_FINGER;
                    calibOption = 0; calibFingerIndex = 0;
                }
            }
            break;
        }

        case CALIB_SELECT_FINGER: {
            int straightI = posVal[1][0];
            int curledI   = posVal[1][stepsForFinger[1]-1];
            int range     = max(straightI - curledI, 1);
            int zone      = 4 - constrain((int)(5.0f*(iBend-curledI)/range), 0, 4);
            calibOption = zone; calibFingerIndex = zone;
            oled.updateCalibUI(CALIB_SELECT_FINGER, calibOption, calibFingerIndex, 0, 0);
            if (btnPressed) { calibPosStep = 0; calibStep = CALIB_STEP_1; }
            break;
        }

        case CALIB_STEP_1:
        case CALIB_STEP_2:
        case CALIB_STEP_3: {
            int liveVal = 0;
            switch (calibFingerIndex) {
                case 0: liveVal = thumb.getBend();        break;
                case 1: liveVal = indexFinger.getBend();  break;
                case 2: liveVal = middleFinger.getBend(); break;
                case 3: liveVal = ringFinger.getBend();   break;
                case 4: liveVal = pinky.getBend();        break;
            }
            oled.updateCalibUI(posStepToEnum(calibPosStep),
                               0, calibFingerIndex, totalSt, liveVal);
            if (btnPressed) {
                posVal[calibFingerIndex][calibPosStep] = liveVal;
                Serial.print("Finger "); Serial.print(calibFingerIndex);
                Serial.print(" pos ");   Serial.print(calibPosStep);
                Serial.print(" = ");     Serial.println(liveVal);
                calibPosStep++;
                calibStep = (calibPosStep >= totalSt) ? CALIB_DONE
                                                      : posStepToEnum(calibPosStep);
            }
            break;
        }

        case CALIB_DONE: {
            oled.updateCalibUI(CALIB_DONE, 0, calibFingerIndex, totalSt, 0);
            if (btnPressed) {
                currentMode = 0;
                Serial.println("Calibration done. Back to keyboard mode.");
            }
            break;
        }
    }
}