#ifndef FINGER_H
#define FINGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>

class Finger{
    private:
        uint8_t muxChannel;
        int potPin;

        float strikeThreshold;
        float releaseThreshold;
        float alpha;

        float previous_az;
        bool isFingerDown;
        unsigned long lastStrikeTime;
        float filteredPitch;
        float filteredRoll;
        unsigned long lastTime;
        bool justClicked;
        
        int currentBend;
    
    public:
        Finger(uint8_t channel, int pin, float strikeThresh, float releaseThresh, float filterAlpha);

        void init();
        void update(Adafruit_MPU6050 & shared_mpu);

        float getPitch();
        bool isClicked();
        bool hasJustClicked();

        int getBend();
};

#endif