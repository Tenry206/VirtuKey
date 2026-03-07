#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include "Finger.h" // Import your custom Blueprint

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

void setup() {
    Serial.begin(115200);
    Wire.begin();
    
    // Initialize the shared MPU6050 here...
    uint8_t activeChannels[] = {7, 6, 5, 3, 2};
    for(int i = 0; i < 5; i++) {
        // 1. Switch multiplexer
        Wire.beginTransmission(0x70);
        Wire.write(1 << activeChannels[i]);
        Wire.endTransmission();
        
        // 2. Initialize the sensor on this specific channel
        if (!mpu.begin()) {
            Serial.print("Failed to find MPU6050 on Mux channel ");
            Serial.println(activeChannels[i]);
            while (1) { delay(10); } // Halt if a wire is loose
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
}

void loop() {
    // Tell each finger to switch the multiplexer, read the sensor, and do its math
    thumb.update(mpu);
    indexFinger.update(mpu);
    middleFinger.update(mpu);
    ringFinger.update(mpu);
    pinky.update(mpu);
    
    // Now you can easily print or use the clean data!
    
    if (thumb.hasJustClicked()) {
        Serial.print("Thumb Pitch is: ");
         Serial.println(thumb.getPitch());
        int bend = thumb.getBend();
        Serial.print("Thumb Strike | Bend: ");
        Serial.print(bend);

        // The middle finger might have completely different raw numbers!
        if (bend > 3300 && bend < 3900) {
            Serial.println(" --> Typed: Backspace (Bottom Row)");
        } else if (3900 <= bend){
            Serial.println(" --> Typed: Space (Top Row)");
        }
    }
    if (indexFinger.hasJustClicked()) {
        Serial.print("Index Finger Pitch is: ");
        Serial.println(indexFinger.getPitch());
        int bend = indexFinger.getBend();
        Serial.print("Index Strike | Bend: ");
        Serial.print(bend);
        // --- ZONE MAPPING (You will need to calibrate these numbers!) ---
        // Assuming a low number is straight, and a high number is tightly curled
        if (bend > 2593 && bend < 3483) {
            Serial.println(" --> Typed: M (Bottom Row)");
        } else if (bend >= 3483 && bend < 3658) {
            Serial.println(" --> Typed: J (Home Row)");
        } else if (bend >= 3658){
            Serial.println(" --> Typed: U (Top Row)");
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
        } else if (bend >= 2514 && bend < 2986) {
            Serial.println(" --> Typed: K (Home Row)");
        } else if (bend >=2986){
            Serial.println(" --> Typed: I (Top Row)");
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
        } else if (bend >= 2200 && bend < 2500) {
            Serial.println(" --> Typed: L (Home Row)");
        } else if (bend >= 2500) {
            Serial.println(" --> Typed: O (Top Row)");
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
        } else if (2656 <= bend){
            Serial.println(" --> Typed: P (Top Row)");
        }
    }
    
    delay(10);
}