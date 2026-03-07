#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

class DisplayUI {
    private:
        Adafruit_SSD1306 display;
        uint8_t muxChannel;
        
        // The layout of your right-hand QWERTY grid
        const char* grid[3][5] = {
            {"Y", "U", "I", "O", "P"},
            {"H", "J", "K", "L", ";"},
            {"N", "M", ",", ".", "Ent"} 
        };

        void selectMux();

    public:
        DisplayUI(uint8_t channel);
        void init();
        
        // This takes the "hover" state of every finger so it can draw the boxes in real-time
        // Row 0 = Top, Row 1 = Home, Row 2 = Bottom
        void updateUI(int thumbRow, int indexRow, bool indexSpread, int middleRow, int ringRow, int pinkyRow, bool isMouseMode);
};

#endif