#include "DisplayUI.h"

DisplayUI::DisplayUI(uint8_t channel) : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {
    muxChannel = channel;
}

void DisplayUI::selectMux() {
    Wire.beginTransmission(0x70);
    Wire.write(1 << muxChannel);
    Wire.endTransmission();
}

void DisplayUI::init() {
    selectMux();
    // SSD1306_SWITCHCAPVCC generates 3.3V from the 3.3v input
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.display();
}

void DisplayUI::updateUI(int thumbRow, int indexRow, bool indexSpread, int middleRow, int ringRow, int pinkyRow, bool isMouseMode) {
    selectMux();
    display.clearDisplay();

    if (isMouseMode) {
        // --- MOUSE MODE UI ---
        display.setTextSize(2);
        display.setCursor(10, 5);
        display.print("MouseMode");

        display.setTextSize(1);
        int thumbY = 35;
        
        // Draw the L/R Click Text
        display.setCursor(12, thumbY + 6);
        display.print("L-CLICK");
        display.setCursor(72, thumbY + 6);
        display.print("R-CLICK");

        // Thumb Box Selection (Row 1 = L-Click/Backspace, Row 0 = R-Click/Space)
        if (thumbRow == 1) {
            display.drawRect(5, thumbY, 55, 20, SSD1306_WHITE); // Box L-Click
        } else {
            display.drawRect(65, thumbY, 55, 20, SSD1306_WHITE); // Box R-Click
        }

    } else {
        // --- KEYBOARD MODE UI ---
        int colWidth = 25;
        int rowHeight = 14;
        
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 5; col++) {
                display.setCursor(col * colWidth + 4, row * rowHeight + 2);
                display.print(grid[row][col]);
            }
        }

        int indexCol = indexSpread ? 0 : 1; 
        display.drawRect(indexCol * colWidth, indexRow * rowHeight, colWidth - 2, rowHeight, SSD1306_WHITE);
        display.drawRect(2 * colWidth, middleRow * rowHeight, colWidth - 2, rowHeight, SSD1306_WHITE);
        display.drawRect(3 * colWidth, ringRow * rowHeight, colWidth - 2, rowHeight, SSD1306_WHITE);
        display.drawRect(4 * colWidth, pinkyRow * rowHeight, colWidth - 2, rowHeight, SSD1306_WHITE);

        int thumbY = 48; 
        display.setCursor(5, thumbY + 4); display.print("SPACE");
        display.setCursor(65, thumbY + 4); display.print("BACKSPACE");

        if (thumbRow == 0) display.drawRect(2, thumbY, 40, 14, SSD1306_WHITE);
        else display.drawRect(62, thumbY, 60, 14, SSD1306_WHITE); 
    }

    display.display();
}