#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include "Finger.h" // Import your custom Blueprint
#include "DisplayUI.h"
#include <BleCombo.h>

//BleKeyboard bleKeyboard("VirtuKey Glove", "Custom", 100);
//BleMouse bleMouse("VirtuKey Glove", "Custom", 100);
//BleComboKeyboard bleCombo("VirtuKey", "Custom", 100);

Adafruit_MPU6050 mpu; // Only one MPU object needed! We share it across the multiplexer.

// --- INSTANTIATE YOUR 5 FINGERS ---
// Format: Finger(MuxChannel, PotPin, StrikeThreshold, ReleaseThreshold, Alpha)

// The thumb is heavy and slow, so it needs a lower threshold and a higher alpha
Finger thumb(7, 32, -3.5, 2.0, 0.90); 

// The index finger is fast and snappy
Finger indexFinger(6, 35, -5.0, 2.0, 0.90); 

Finger middleFinger(5, 34, -5.0, 2.0, 0.90); 

// The ring finger (your current test finger)
Finger ringFinger(3, 39, -5.3, 3.0, 0.90); 

Finger pinky(2, 36, -3.4, 4.0, 0.90);
// ... define middle and pinky here ...

const uint8_t HAND_MUX = 4;

const uint8_t OLED_MUX = 1;
DisplayUI oled(OLED_MUX);

const int MODE_BTN_PIN = 26; 
bool isMouseMode = false;
bool lastBtnState = HIGH;

void setup() {
    Serial.begin(115200);
    Wire.begin();
    
    pinMode(MODE_BTN_PIN, INPUT_PULLUP);

    // Initialize the shared MPU6050 here...
    uint8_t activeChannels[] = {7, 6, 5, 3, 2};
    for(int i = 0; i < 5; i++) {
        // 1. Switch multiplexer
        Wire.beginTransmission(0x70);
        Wire.write(1 << activeChannels[i]);
        Wire.endTransmission();
        
        // 2. Initialize the sensor on this specific channel
        if (!mpu.begin()) {
            Serial.print("WARNING: No MPU6050 on Mux channel ");
            Serial.println(activeChannels[i]);
            continue; // Skip this channel instead of halting
        }
        // 3. Configure the sensor's sensitivity
        mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }
    Serial.println("All MPU6050s Initialized Successfully!");
    
    thumb.init();
    indexFinger.init();
    middleFinger.init();
    ringFinger.init();
    pinky.init();

    oled.init();

    //bleKeyboard.begin();
    //bleMouse.begin();
    Keyboard.begin(); 
    Mouse.begin();
    Serial.println("Bluetooth Started!. Waiting for connection");
}

void loop() {
    bool currentBtnState = digitalRead(MODE_BTN_PIN);
    if (lastBtnState == HIGH && currentBtnState == LOW) {
        if (isMouseMode && Keyboard.isConnected()) {
            Mouse.release(MOUSE_LEFT);
            Mouse.release(MOUSE_RIGHT);
        }
        isMouseMode = !isMouseMode;
        Serial.println(isMouseMode ? "+++ MOUSE MODE +++" : "--- KEYBOARD MODE ---");
        delay(50);
    }
    lastBtnState = currentBtnState;

    Wire.beginTransmission(0x70);
    Wire.write(1 << HAND_MUX);
    Wire.endTransmission();
    
    float handGyroZ = 0.0;
    sensors_event_t a_hand, g_hand, temp_hand;
    if (mpu.getEvent(&a_hand, &g_hand, &temp_hand)) {
        handGyroZ = g_hand.gyro.z * 180.0 / PI; // Get the back of hand velocity
    }

    // Tell each finger to switch the multiplexer, read the sensor, and do its math
    thumb.update(mpu, handGyroZ);
    indexFinger.update(mpu, handGyroZ);
    
    // Only update other fingers if we are in keyboard mode to save processing power!
    if (!isMouseMode) {
        middleFinger.update(mpu, handGyroZ);
        ringFinger.update(mpu, handGyroZ);
        pinky.update(mpu, handGyroZ);
    }
    // Now you can easily print or use the clean data!
    
    // Default to Home row (Row 1)
    int tRow = 0, iRow = 1, mRow = 1, rRow = 1, pRow = 1; 

    // Calculate Thumb (0 = Space, 1 = Backspace)
    if (thumb.getBend() < 3900) tRow = 1; 

    // Calculate Index (0 = Top, 1 = Home, 2 = Bottom)
    int iBend = indexFinger.getBend();
    if (iBend >= 3658) iRow = 0; 
    else if (iBend < 3483) iRow = 2; 

    // Calculate Middle
    int mBend = middleFinger.getBend();
    if (mBend >= 2986) mRow = 0;
    else if (mBend < 2514) mRow = 2;

    // Calculate Ring
    int rBend = ringFinger.getBend();
    if (rBend >= 2500) rRow = 0;
    else if (rBend < 2200) rRow = 2;

    // Calculate Pinky
    int pBend = pinky.getBend();
    if (pBend >= 2656) pRow = 0;
    else if (pBend < 2656) pRow = 2; // (Using your Enter/P logic)

    // UPDATE THE SCREEN! (It will draw instantly)
    oled.updateUI(tRow, iRow, indexFinger.getStretchedState(), mRow, rRow, pRow, isMouseMode);

    if (Keyboard.isConnected()) {
        if (isMouseMode) {
            // --- MOUSE MODE ---
            float pitch = indexFinger.getPitch();
            float roll = indexFinger.getRoll();
            int moveX = 0, moveY = 0;
            
            if (roll > 15.0) moveX = (roll - 15.0) / 2;
            else if (roll < -15.0) moveX = (roll + 15.0) / 2;
            if (pitch > 15.0) moveY = (pitch - 15.0) / 2; 
            else if (pitch < -15.0) moveY = (pitch + 15.0) / 2;
            
            if (moveX != 0 || moveY != 0) Mouse.move(moveX, moveY);

            if (thumb.hasJustClicked()) {
                if (thumb.getBend() < 3900) Mouse.click(MOUSE_LEFT);
                else Mouse.click(MOUSE_RIGHT);
            }
            indexFinger.hasJustClicked(); // Consume
        } else {
            if (thumb.hasJustClicked()) {
                Serial.print("Thumb Pitch is: ");
                Serial.println(thumb.getPitch());
                int bend = thumb.getBend();
                Serial.print("Thumb Strike | Bend: ");
                Serial.print(bend);

                // The middle finger might have completely different raw numbers!
                if (bend > 3300 && bend < 3900) {
                    Serial.println(" --> Typed: Backspace (Bottom Row)");
                    if(Keyboard.isConnected()) Keyboard.write(KEY_BACKSPACE);
                } else if (3900 <= bend){
                    Serial.println(" --> Typed: Space (Top Row)");
                    if(Keyboard.isConnected()) Keyboard.write(' ');
                }
            }
            if (indexFinger.hasJustClicked()) {
                bool isStretched = indexFinger.getStretchedState();
                Serial.print("Index Finger Pitch is: ");
                Serial.println(indexFinger.getPitch());
                int bend = indexFinger.getBend();
                Serial.print("Index Strike | Bend: ");
                Serial.print(bend);
                
                // --- ZONE MAPPING (You will need to calibrate these numbers!) ---
                // Assuming a low number is straight, and a high number is tightly curled
                if (isStretched) {
                    // THE FINGER IS SPREAD TO THE LEFT (Y, H, N)
                    if (bend > 2593 && bend < 3483) {
                        Serial.println(" --> Typed: N (Bottom Row)");
                        if(Keyboard.isConnected()) Keyboard.write('n');
                    } else if (bend >= 3483 && bend < 3658) {
                        Serial.println(" --> Typed: H (Home Row)");
                        if(Keyboard.isConnected()) Keyboard.write('h');
                    } else if (bend >= 3658){
                        Serial.println(" --> Typed: Y (Top Row)");
                        if(Keyboard.isConnected()) Keyboard.write('y');
                    }
                } else {
                    // THE FINGER IS IN NEUTRAL CENTER (U, J, M)
                    if (bend > 2593 && bend < 3483) {
                        Serial.println(" --> Typed: M (Bottom Row)");
                        if(Keyboard.isConnected()) Keyboard.write('m');
                    } else if (bend >= 3483 && bend < 3658) {
                        Serial.println(" --> Typed: J (Home Row)");
                        if(Keyboard.isConnected()) Keyboard.write('j');
                    } else if (bend >= 3658){
                        Serial.println(" --> Typed: U (Top Row)");
                        if(Keyboard.isConnected()) Keyboard.write('u');
                    }
                }
            }
            if (middleFinger.hasJustClicked()){
                Serial.print("Middle Finger Pitch is: ");
                Serial.println(middleFinger.getPitch());
                int bend = middleFinger.getBend();
                Serial.print("Middle Strike | Bend: ");
                Serial.print(bend);

                // The middle finger might have completely different raw numbers!
                if (bend > 1542 && bend < 2514) {
                    Serial.println(" --> Typed: , (Bottom Row)");
                    if(Keyboard.isConnected()) Keyboard.write(',');
                } else if (bend >= 2514 && bend < 2986) {
                    Serial.println(" --> Typed: K (Home Row)");
                    if(Keyboard.isConnected()) Keyboard.write('k');
                } else if (bend >=2986){
                    Serial.println(" --> Typed: I (Top Row)");
                    if(Keyboard.isConnected()) Keyboard.write('i');
                }
            }


            if (ringFinger.hasJustClicked()) {
                Serial.print("Ring Finger Pitch is: ");
                Serial.println(ringFinger.getPitch());
                int bend = ringFinger.getBend();
                Serial.print("Ring Strike | Bend: ");
                Serial.print(bend);

                // The middle finger might have completely different raw numbers!
                if (bend > 1600 && bend < 2200) {
                    Serial.println(" --> Typed: . (Bottom Row)");
                    if(Keyboard.isConnected()) Keyboard.write('.');
                } else if (bend >= 2200 && bend < 2500) {
                    Serial.println(" --> Typed: L (Home Row)");
                    if(Keyboard.isConnected()) Keyboard.write('l');
                } else if (bend >= 2500) {
                    Serial.println(" --> Typed: O (Top Row)");
                    if(Keyboard.isConnected()) Keyboard.write('o');
                }
            }

            if (pinky.hasJustClicked()){
                Serial.print("Pinky Finger Pitch is: ");
                Serial.println(pinky.getPitch());
                int bend = pinky.getBend();
                Serial.print("Pinky Strike | Bend:");
                Serial.print(bend);

                if (bend > 1970 && bend < 2656) {
                    Serial.println(" --> Typed: Enter (Bottom Row)");
                    if(Keyboard.isConnected()) Keyboard.write(KEY_RETURN);
                } else if (2656 <= bend){
                    Serial.println(" --> Typed: P (Top Row)");
                    if(Keyboard.isConnected()) Keyboard.write('p');
                }
            }
        }
    } 
    // ── SERIAL TELEMETRY FOR MATLAB ──────────────────────────────
    // Format: T:pitch,roll,bend|I:pitch,roll,bend,spread|M:pitch,roll,bend|R:pitch,roll,bend|P:pitch,roll,bend|MODE:0
    Serial.print("T:"); Serial.print(thumb.getPitch(),1);       Serial.print(",");
                        Serial.print(thumb.getRoll(),1);         Serial.print(",");
                        Serial.print(thumb.getBend());           Serial.print("|");

    Serial.print("I:"); Serial.print(indexFinger.getPitch(),1); Serial.print(",");
                        Serial.print(indexFinger.getRoll(),1);   Serial.print(",");
                        Serial.print(indexFinger.getBend());     Serial.print(",");
                        Serial.print(indexFinger.getStretchedState() ? 1 : 0); Serial.print("|");

    Serial.print("M:"); Serial.print(middleFinger.getPitch(),1);Serial.print(",");
                        Serial.print(middleFinger.getRoll(),1);  Serial.print(",");
                        Serial.print(middleFinger.getBend());    Serial.print("|");

    Serial.print("R:"); Serial.print(ringFinger.getPitch(),1);  Serial.print(",");
                        Serial.print(ringFinger.getRoll(),1);    Serial.print(",");
                        Serial.print(ringFinger.getBend());      Serial.print("|");

    Serial.print("P:"); Serial.print(pinky.getPitch(),1);       Serial.print(",");
                        Serial.print(pinky.getRoll(),1);         Serial.print(",");
                        Serial.print(pinky.getBend());           Serial.print("|");

    Serial.print("MODE:"); Serial.println(isMouseMode ? 1 : 0);

    delay(20); // 50 Hz
}