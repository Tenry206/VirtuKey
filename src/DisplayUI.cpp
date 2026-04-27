#include "DisplayUI.h"

DisplayUI::DisplayUI(uint8_t channel)
    : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {
    muxChannel = channel;
}

void DisplayUI::selectMux() {
    Wire.beginTransmission(0x70);
    Wire.write(1 << muxChannel);
    Wire.endTransmission();
}

void DisplayUI::init() {
    selectMux();
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.display();
}

// ─── Normal keyboard / mouse UI ───────────────────────────────────────────────
void DisplayUI::updateUI(int thumbRow, int indexRow, bool indexSpread,
                         int middleRow, int ringRow, int pinkyRow,
                         bool isMouseMode) {
    selectMux();
    display.clearDisplay();

    if (isMouseMode) {
        display.setTextSize(2);
        display.setCursor(10, 5);
        display.print("MouseMode");

        display.setTextSize(1);
        int thumbY = 35;
        display.setCursor(12, thumbY + 6); display.print("R-CLICK");
        display.setCursor(72, thumbY + 6); display.print("L-CLICK");

        if (thumbRow == 1) display.drawRect(5,  thumbY, 55, 20, SSD1306_WHITE);
        else               display.drawRect(65, thumbY, 55, 20, SSD1306_WHITE);

    } else {
        int colW = 25, rowH = 14;
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 5; c++) {
                display.setCursor(c * colW + 4, r * rowH + 2);
                display.print(grid[r][c]);
            }

        int iCol = indexSpread ? 0 : 1;
        display.drawRect(iCol * colW,  indexRow  * rowH, colW - 2, rowH, SSD1306_WHITE);
        display.drawRect(2 * colW,     middleRow * rowH, colW - 2, rowH, SSD1306_WHITE);
        display.drawRect(3 * colW,     ringRow   * rowH, colW - 2, rowH, SSD1306_WHITE);
        display.drawRect(4 * colW,     pinkyRow  * rowH, colW - 2, rowH, SSD1306_WHITE);

        int thumbY = 48;
        display.setCursor(5,  thumbY + 4); display.print("SPACE");
        display.setCursor(65, thumbY + 4); display.print("BACKSPACE");
        if (thumbRow == 0) display.drawRect(2,  thumbY, 40, 14, SSD1306_WHITE);
        else               display.drawRect(62, thumbY, 60, 14, SSD1306_WHITE);
    }

    display.display();
}

// ─── Calibration UI ───────────────────────────────────────────────────────────
void DisplayUI::updateCalibUI(CalibStep step, int selectedOption,
                              int fingerIndex, int totalSteps, int rawBend) {
    selectMux();
    display.clearDisplay();
    display.setTextSize(1);

    const char* fingerNames[5] = {"Thumb", "Index", "Middle", "Ring", "Pinky"};

    // Position labels depend on how many steps this finger has
    // 2-step fingers (Thumb, Pinky): Top row / Bottom row
    // 3-step fingers (Index, Middle, Ring): Top / Home / Bottom
    const char* stepLabels2[2] = {"Top row (straight)", "Bottom row (curl)"};
    const char* stepLabels3[3] = {"Top row  (straight)",
                                  "Home row (mid-bend)",
                                  "Bottom   (full curl)"};

    switch (step) {

        // ── Confirm screen ────────────────────────────────────────────────────
        case CALIB_CONFIRM:
            display.setCursor(8, 0);
            display.print("=== Auto Calibrate ===");

            display.setCursor(30, 18); display.print("Cancel");
            display.setCursor(30, 38); display.print("Yes - Start");

            // selectedOption 0 = Cancel highlighted, 1 = Yes highlighted
            if (selectedOption == 0)
                display.drawRect(20, 14, 88, 16, SSD1306_WHITE);
            else
                display.drawRect(20, 34, 88, 16, SSD1306_WHITE);

            display.setCursor(2, 56);
            display.print("Bend=toggle  Btn=confirm");
            break;

        // ── Finger select screen ──────────────────────────────────────────────
        case CALIB_SELECT_FINGER:
            display.setCursor(16, 0);
            display.print("Select Finger:");

            for (int i = 0; i < 5; i++) {
                int y = 10 + i * 10;
                display.setCursor(30, y + 1);
                display.print(fingerNames[i]);
                // Show how many steps each finger needs
                display.setCursor(78, y + 1);
                display.print((i == 0 || i == 4) ? "(2)" : "(3)");
                if (i == selectedOption)
                    display.drawRect(24, y, 100, 10, SSD1306_WHITE);
            }

            display.setCursor(0, 57);
            display.print("Bend=scroll  Btn=pick");
            break;

        // ── Step 1, 2, or 3: sample position ─────────────────────────────────
        case CALIB_STEP_1:
        case CALIB_STEP_2:
        case CALIB_STEP_3: {
            // Determine which step number we are on (1-based)
            int stepNum = (step == CALIB_STEP_1) ? 1 :
                          (step == CALIB_STEP_2) ? 2 : 3;

            // Header
            display.setCursor(0, 0);
            display.print(fingerNames[fingerIndex]);
            display.print("  Step ");
            display.print(stepNum);
            display.print("/");
            display.print(totalSteps);

            // Instruction
            display.setCursor(0, 14);
            display.print("Place finger at:");
            display.setCursor(0, 24);
            if (totalSteps == 2) {
                display.print(stepLabels2[stepNum - 1]);
            } else {
                display.print(stepLabels3[stepNum - 1]);
            }

            // Live ADC readout
            display.setCursor(0, 38);
            display.print("Live ADC: ");
            display.print(rawBend);

            // Simple bar graph of ADC (0-4095 range)
            int barW = map(rawBend, 0, 4095, 0, 124);
            barW = constrain(barW, 0, 124);
            display.drawRect(2, 48, 124, 8, SSD1306_WHITE);
            display.fillRect(2, 48, barW, 8, SSD1306_WHITE);

            display.setCursor(0, 58);
            display.print("Btn=sample this pos");
            break;
        }

        // ── Done screen ───────────────────────────────────────────────────────
        case CALIB_DONE:
            display.setTextSize(2);
            display.setCursor(22, 8);
            display.print("Saved!");

            display.setTextSize(1);
            display.setCursor(4, 36);
            display.print(fingerNames[fingerIndex]);
            display.print(": ");
            display.print(totalSteps);
            display.print(" pos updated");

            display.setCursor(14, 52);
            display.print("Btn = exit calib");
            break;
    }

    display.display();
}