#include "Finger.h"

// --- CONSTRUCTOR ---
// This initializes the specific tuning variables for this exact finger when it is created
Finger::Finger(uint8_t channel, int pin, float strikeThresh, float releaseThresh, float filterAlpha) {
    muxChannel = channel;
    potPin = pin;
    
    strikeThreshold = strikeThresh;
    releaseThreshold = releaseThresh;
    alpha = filterAlpha;

    // Initialize state variables to 0 or false
    previous_az = 0.0;
    isFingerDown = false;
    lastStrikeTime = 0;
    filteredPitch = 0.0;
    filteredRoll = 0.0;
    lastTime = 0;
    justClicked = false;

    currentBend = 0;

    isStretched = false;
}

// --- INITIALIZATION ---
// Runs once in setup() to prep the finger
void Finger::init() {
    pinMode(potPin, INPUT); // Prep the potentiometer pin
    lastTime = millis();    // Start the time tracker
}

// --- MAIN LOOP UPDATE ---
// Switches the mux, reads the shared MPU, and does all the math
void Finger::update(Adafruit_MPU6050 &shared_mpu, float handGyroZ) {
    // 1. Switch the Multiplexer to this finger's channel
    Wire.beginTransmission(0x70); 
    Wire.write(1 << muxChannel); 
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        return; // Mux failed to switch, skip this loop
    }

    // 2. Read the sensor data
    sensors_event_t a, g, temp;
    if (!shared_mpu.getEvent(&a, &g, &temp)) {
       return; // Sensor failed to read, skip this loop
    }

    // 3. Time calculation for the filter
    unsigned long currentTime = millis();
    float dt = (currentTime - lastTime) / 1000.0; 
    lastTime = currentTime;

    currentBend = analogRead(potPin);

    // 4. Complementary Filter Math
    float accelPitch = atan2(a.acceleration.y, sqrt(a.acceleration.x * a.acceleration.x + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;
    float accelRoll = atan2(-a.acceleration.x, a.acceleration.z) * 180.0 / PI;

    float gyroRateX = g.gyro.x * 180.0 / PI; 
    float gyroRateY = g.gyro.y * 180.0 / PI;

    filteredPitch = alpha * (filteredPitch + gyroRateY * dt) + (1.0 - alpha) * accelPitch;
    filteredRoll = alpha * (filteredRoll + gyroRateX * dt) + (1.0 - alpha) * accelRoll;

    // 5. Clicking/Jerk Math
    float current_az = a.acceleration.z;
    float delta_az = current_az - previous_az;
    float abs_delta_az = abs(delta_az);
    previous_az = current_az;

    const int DEBOUNCE_TIME = 100; // From your prototype

    // Check for a downward strike
    if(delta_az < strikeThreshold && !isFingerDown && (millis() - lastStrikeTime > DEBOUNCE_TIME)){
        isFingerDown = true;
        justClicked = true;
        lastStrikeTime = millis();
    }
    // Check for an upward release
    else if(delta_az > releaseThreshold && isFingerDown && (millis() - lastStrikeTime > DEBOUNCE_TIME)){
        isFingerDown = false;
        lastStrikeTime = millis();
    }

    // --- SPREAD MATH WITH GATE ---
    float indexGyroZ = g.gyro.z * 180.0 / PI;
    float relativeVelocityZ = indexGyroZ - handGyroZ;

    const float STRIKE_NOISE_THRESHOLD = 6.5; 
    const float STRETCH_LEFT_SPEED = 170.0;   
    const float RETURN_CENTER_SPEED = -200.0; 

    // Only update the spread state if we are NOT clicking
    if (abs_delta_az < STRIKE_NOISE_THRESHOLD) {
        if (relativeVelocityZ > STRETCH_LEFT_SPEED) {
            isStretched = true;  
        } 
        else if (relativeVelocityZ < RETURN_CENTER_SPEED) {
            isStretched = false; 
        }
    }
}

// --- DATA GETTERS ---

float Finger::getPitch() {
    return filteredPitch;
}

bool Finger::isClicked() {
    return isFingerDown; 
}

bool Finger::hasJustClicked() {
    if (justClicked) {
        justClicked = false; // We read it, now reset it immediately!
        return true;
    }
    return false;
}

int Finger::getBend(){
    return currentBend;
}

bool Finger::getStretchedState() {
    return isStretched;
}

// NEW GETTER
float Finger::getRoll() {
    return filteredRoll;
}