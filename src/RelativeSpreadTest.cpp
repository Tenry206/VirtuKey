#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

// Multiplexer Channels
const uint8_t HAND_MUX = 4;
const uint8_t INDEX_MUX = 6;

// Time tracking
unsigned long lastTime = 0;

// Variables for Strike Gate
float previous_az = 0.0;

// NEW: The State Machine memory
bool isStretchedLeft = false; 

void selectTCAChannel(uint8_t bus){
    Wire.beginTransmission(0x70); 
    Wire.write(1 << bus); 
    Wire.endTransmission();
}

void setup(){
    Serial.begin(115200); 
    Wire.begin();

    // 1. Initialize Hand MPU
    selectTCAChannel(HAND_MUX);
    if (!mpu.begin()){
        Serial.println("Failed to find Hand MPU6050 (Ch 4).");
        while(1){ delay(10); }
    }
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);

    // 2. Initialize Index MPU
    selectTCAChannel(INDEX_MUX);
    if (!mpu.begin()){
        Serial.println("Failed to find Index MPU6050 (Ch 6).");
        while(1){ delay(10); }
    }
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    
    Serial.println("Velocity Spread Test Initialized. Open Serial Plotter!");
    lastTime = millis();
}

void loop(){
    unsigned long currentTime = millis();
    float dt = (currentTime - lastTime) / 1000.0; 
    lastTime = currentTime;

    sensors_event_t a, g, temp;

    // --- 1. Get Hand Gyro Z ---
    selectTCAChannel(HAND_MUX);
    mpu.getEvent(&a, &g, &temp);
    float handGyroZ = g.gyro.z * 180.0 / PI; 

    // --- 2. Get Index Gyro Z & Accel Z ---
    selectTCAChannel(INDEX_MUX);
    mpu.getEvent(&a, &g, &temp);
    float indexGyroZ = g.gyro.z * 180.0 / PI;

    // Calculate the clicking motion (jerk) to use as a gate
    float current_az = a.acceleration.z;
    float delta_az = abs(current_az - previous_az);
    previous_az = current_az;

    // --- 3. Calculate Relative Horizontal Velocity ---
    float relativeVelocityZ = indexGyroZ - handGyroZ;

    // --- 4. The Velocity State Machine with Strike Gate ---
    const float STRIKE_NOISE_THRESHOLD = 6.5; 
    
    // TUNE THESE: How fast in degrees-per-second you must swipe to trigger it
    const float STRETCH_LEFT_SPEED = 170.0;   // Positive spike going left
    const float RETURN_CENTER_SPEED = -200.0; // Negative spike returning right

    // Only update the state if we are NOT currently doing a downward strike
    if (delta_az < STRIKE_NOISE_THRESHOLD) {
        if (relativeVelocityZ > STRETCH_LEFT_SPEED) {
            isStretchedLeft = true;  // Finger snapped left!
        } 
        else if (relativeVelocityZ < RETURN_CENTER_SPEED) {
            isStretchedLeft = false; // Finger snapped back to center!
        }
    }

    // --- 5. PlatformIO Serial Plotter Output ---
    Serial.print(">"); 
    Serial.print("VelocityZ:"); Serial.print(relativeVelocityZ); Serial.print(",");
    
    // Multiply the boolean by 100 so it draws a clear square wave on the graph
    Serial.print("SpreadState:"); Serial.print(isStretchedLeft ? 100.0 : 0.0); Serial.print(",");
    
    Serial.print("Delta_AZ:"); Serial.print(delta_az); 
    Serial.println();

    delay(10); 
}