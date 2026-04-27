#ifndef FINGER_H
#define FINGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>

class Finger {
    private:
        uint8_t muxChannel;
        int potPin;

        float strikeThreshold;
        float releaseThreshold;
        float alpha;

        float previous_az;
        bool isFingerDown;
        unsigned long lastStrikeTime;
        float gz_bias;
        float gy_bias;
        float filteredPitch;
        float filteredRoll;
        unsigned long lastTime;
        bool justClicked;

        int currentBend;
        bool isStretched;

        // Raw gyroscope rates in deg/s — stored each update() call
        // Used by mouse mode instead of complementary-filter angles
        // so cursor movement is drift-free (gyro only outputs when moving)
        float rawGyroX;   // deg/s around X axis (hand tilts forward/back)
        float rawGyroY;   // deg/s around Y axis (hand tilts left/right)
        float rawGyroZ;   // deg/s around Z axis (hand yaws / rotates flat)

    public:
        Finger(uint8_t channel, int pin,
               float strikeThresh, float releaseThresh, float filterAlpha);

        void init();
        void update(Adafruit_MPU6050 &shared_mpu, float handGyroZ);

        // Complementary-filter angles (used by keyboard mode / spread detection)
        float getPitch();
        float getRoll();

        // Raw gyro rates in deg/s (used by mouse mode — no drift)
        float getGyroX();
        float getGyroY();
        float getGyroZ();

        bool isClicked();
        bool hasJustClicked();
        int  getBend();
        bool getStretchedState();
};

#endif