#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

// ─────────────────────────────────────────────────────────────────────────────
//  CalibStep enum — MUST be in the header so both DisplayUI.cpp and main.cpp
//  see the same definition.  Never define this only in main.cpp.
// ─────────────────────────────────────────────────────────────────────────────
enum CalibStep {
    CALIB_CONFIRM,          // "Auto Calibrate? Cancel / Yes"
    CALIB_SELECT_FINGER,    // pick which finger
    CALIB_STEP_1,           // sample position 1 (top row / straight)
    CALIB_STEP_2,           // sample position 2 (home row / mid)
    CALIB_STEP_3,           // sample position 3 (bottom row / curled)
    CALIB_DONE              // "Saved! Press button to exit"
};

class DisplayUI {
    private:
        Adafruit_SSD1306 display;
        uint8_t muxChannel;

        const char* grid[3][5] = {
            {"Y", "U", "I", "O", "P"},
            {"H", "J", "K", "L", ";"},
            {"N", "M", ",", ".", "Ent"}
        };

        void selectMux();

    public:
        DisplayUI(uint8_t channel);
        void init();

        // Normal keyboard / mouse mode display
        void updateUI(int thumbRow, int indexRow, bool indexSpread,
                      int middleRow, int ringRow, int pinkyRow,
                      bool isMouseMode);

        // Calibration screens
        // selectedOption : highlighted row index (0 or 1 for confirm; 0-4 for finger select)
        // fingerIndex    : 0=Thumb 1=Index 2=Middle 3=Ring 4=Pinky
        // totalSteps     : how many positions this finger needs (2 for thumb/pinky, 3 for others)
        // rawBend        : live ADC reading shown during sampling steps
        void updateCalibUI(CalibStep step, int selectedOption,
                           int fingerIndex, int totalSteps, int rawBend);
};

#endif