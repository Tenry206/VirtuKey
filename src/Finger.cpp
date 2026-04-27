#include "Finger.h"

Finger::Finger(uint8_t channel, int pin,
               float strikeThresh, float releaseThresh, float filterAlpha) {
    muxChannel = channel;
    potPin     = pin;

    strikeThreshold  = strikeThresh;
    releaseThreshold = releaseThresh;
    alpha            = filterAlpha;

    previous_az  = 0.0;
    isFingerDown = false;
    lastStrikeTime = 0;
    filteredPitch  = 0.0;
    filteredRoll   = 0.0;
    lastTime       = 0;
    justClicked    = false;
    currentBend    = 0;
    isStretched    = false;

    rawGyroX = 0.0;
    rawGyroY = 0.0;
    rawGyroZ = 0.0;
}

void Finger::init() {
    pinMode(potPin, INPUT);
    lastTime = millis();
}

void Finger::update(Adafruit_MPU6050 &shared_mpu, float handGyroZ) {
    Wire.beginTransmission(0x70);
    Wire.write(1 << muxChannel);
    uint8_t error = Wire.endTransmission();
    if (error != 0) return;

    sensors_event_t a, g, temp;
    if (!shared_mpu.getEvent(&a, &g, &temp)) return;

    unsigned long currentTime = millis();
    float dt = (currentTime - lastTime) / 1000.0;
    lastTime = currentTime;

    currentBend = analogRead(potPin);

    // ── Store raw gyro rates in deg/s ─────────────────────────────────────────
    // These are used directly by mouse mode — no filter accumulation = no drift
    rawGyroX = g.gyro.x * 180.0 / PI;
    rawGyroY = g.gyro.y * 180.0 / PI;
    rawGyroZ = g.gyro.z * 180.0 / PI;

    // ── Complementary filter (still used for keyboard/spread detection) ────────
    float accelPitch = atan2(a.acceleration.y,
                             sqrt(a.acceleration.x * a.acceleration.x +
                                  a.acceleration.z * a.acceleration.z)) * 180.0 / PI;
    float accelRoll  = atan2(-a.acceleration.x, a.acceleration.z) * 180.0 / PI;

    filteredPitch = alpha * (filteredPitch + rawGyroY * dt) + (1.0 - alpha) * accelPitch;
    filteredRoll  = alpha * (filteredRoll  + rawGyroX * dt) + (1.0 - alpha) * accelRoll;

    // ── Strike detection ──────────────────────────────────────────────────────
    float current_az  = a.acceleration.z;
    float delta_az    = current_az - previous_az;
    float abs_delta_az = abs(delta_az);
    previous_az = current_az;

    const int DEBOUNCE_TIME = 100;

    if (delta_az < strikeThreshold && !isFingerDown &&
        (millis() - lastStrikeTime > DEBOUNCE_TIME)) {
        isFingerDown = true;
        justClicked  = true;
        lastStrikeTime = millis();
    } else if (delta_az > releaseThreshold && isFingerDown &&
               (millis() - lastStrikeTime > DEBOUNCE_TIME)) {
        isFingerDown = false;
        lastStrikeTime = millis();
    }

    // ── Spread detection ──────────────────────────────────────────────────────
    float relativeVelocityZ = rawGyroZ - handGyroZ;

    const float STRIKE_NOISE_THRESHOLD = 6.5;
    const float STRETCH_LEFT_SPEED     = 170.0;
    const float RETURN_CENTER_SPEED    = -200.0;

    if (abs_delta_az < STRIKE_NOISE_THRESHOLD) {
        if      (relativeVelocityZ >  STRETCH_LEFT_SPEED)  isStretched = true;
        else if (relativeVelocityZ <  RETURN_CENTER_SPEED) isStretched = false;
    }
}

// ── Getters ───────────────────────────────────────────────────────────────────
float Finger::getPitch()  { return filteredPitch; }
float Finger::getRoll()   { return filteredRoll;  }
float Finger::getGyroX()  { return rawGyroX; }
float Finger::getGyroY()  { return rawGyroY; }
float Finger::getGyroZ()  { return rawGyroZ; }

bool Finger::isClicked()  { return isFingerDown; }

bool Finger::hasJustClicked() {
    if (justClicked) { justClicked = false; return true; }
    return false;
}

int  Finger::getBend()          { return currentBend; }
bool Finger::getStretchedState(){ return isStretched;  }