#include <M5StickCPlus2.h>

// =======================================================
// === 1. GLOBAL VARIABLES AND CONSTANTS ===
// =======================================================

// --- Hardware & Motion Configuration ---
const float SHAKE_THRESHOLD = 2.0;       
const long cooldownPeriod = 3000;       
unsigned long lastAnswerTime = 0;

// --- Sleep Configuration ---
const long SLEEP_TIMEOUT = 60000;        
const long LONG_PRESS_DURATION = 2000;   // Used for the new Time Adjustment trigger
unsigned long lastActivityTime = 0;      
unsigned long buttonAPressStartTime = 0; 

// --- Clock & Display State (For Conditional Redraw/Anti-Flicker) ---
m5::rtc_time_t RTC_TimeStruct; 
int timeAdjustMode = 0; // 0: Normal, 1: Adjust Hour, 2: Adjust Minute
int lastDisplayedMinute = -1; 
int lastDisplayedPercentage = -1; 

// --- Animation Parameters ---
const int FADE_STEPS = 25;   
const int FADE_DELAY_MS = 15; 

// --- Battery Voltage Mapping (Li-ion) ---
const int VOLTAGE_MAX_MV = 4200; 
const int VOLTAGE_MIN_MV = 3300; 

// --- Brightness Control ---
const int BRIGHTNESS_LEVELS[] = {1, 25, 50, 75, 100}; // 1% is for true "night mode"
const int NUM_BRIGHTNESS_LEVELS = sizeof(BRIGHTNESS_LEVELS) / sizeof(BRIGHTNESS_LEVELS[0]);
int currentBrightnessIndex = NUM_BRIGHTNESS_LEVELS - 1; // Start at 100% (index 4)

// --- Magic 8-Ball Answers ---
const String answers[] = {
    "It is certain", "It is decidedly so", "Without a doubt", "Yes, definitely",
    "You may rely on it", "As I see it, yes", "Most likely", "Outlook good",
    "Yes", "Signs point to yes", "Reply hazy, try again", "Ask again later",
    "Better not tell you now", "Cannot predict now", "Concentrate and ask again",
    "Don't count on it", "My reply is no", "My sources say no", 
    "Outlook not so good", "Very doubtful"
};
const int numAnswers = sizeof(answers) / sizeof(answers[0]);


// =======================================================
// === 2. HELPER FUNCTIONS ===
// =======================================================

// Function prototypes 
void drawClock(uint16_t color = DARKGREY); 
void drawBatteryPercentage(int percentage, uint16_t color);

// Utility function to blend two RGB565 colors based on an alpha value (0-255)
uint16_t alphaBlend(uint8_t alpha, uint16_t fgc, uint16_t bgc) {
    if (alpha == 0) return bgc;
    if (alpha == 255) return fgc;
    uint8_t fR = (fgc >> 11) & 0x1F; uint8_t fG = (fgc >> 5) & 0x3F; uint8_t fB = fgc & 0x1F;
    uint8_t bR = (bgc >> 11) & 0x1F; uint8_t bG = (bgc >> 5) & 0x3F; uint8_t bB = bgc & 0x1F;
    uint8_t r = ((fR * alpha) + (bR * (255 - alpha))) / 255;
    uint8_t g = ((fG * alpha) + (bG * (255 - alpha))) / 255;
    uint8_t b = ((fB * alpha) + (bB * (255 - alpha))) / 255;
    return (r << 11) | (g << 5) | b;
}

// Custom word-wrapping function 
String wrapTextByWord(String text, int textSize) {
    const int MAX_WIDTH = M5.Lcd.width() - 10; 
    M5.Lcd.setTextSize(textSize);
    int charWidth = M5.Lcd.fontWidth();
    if (charWidth == 0) charWidth = 6 * textSize; 
    int maxCharsPerLine = MAX_WIDTH / charWidth;
    
    String wrappedText = "";
    String currentLine = "";
    int currentLineLength = 0;
    int lastSpace = 0;
    for (int i = 0; i <= text.length(); i++) {
        if (i == text.length() || text.charAt(i) == ' ') {
            String word = text.substring(lastSpace, i);
            lastSpace = i + 1;
            if (currentLineLength + word.length() + (currentLine.length() > 0 ? 1 : 0) <= maxCharsPerLine) {
                if (currentLine.length() > 0) {
                    currentLine += " ";
                    currentLineLength += 1;
                }
                currentLine += word;
                currentLineLength += word.length();
            } else {
                if (wrappedText.length() > 0) {
                    wrappedText += "\n";
                }
                wrappedText += currentLine;
                currentLine = word;
                currentLineLength = word.length();
            }
        }
    }
    if (wrappedText.length() > 0) {
        wrappedText += "\n";
    }
    wrappedText += currentLine;

    return wrappedText;
}

// Helper to draw text centered line-by-line (Vertically and Horizontally)
void drawCenteredWrappedText(String message, int textSize, uint16_t color, uint16_t bgColor) {
    String wrappedMessage = wrapTextByWord(message, textSize);
    
    M5.Lcd.setTextSize(textSize); 
    M5.Lcd.setTextColor(color, bgColor);
    M5.Lcd.setTextDatum(MC_DATUM); 
    M5.Lcd.setTextWrap(false, false); 
    
    int lineHeight = M5.Lcd.fontHeight() + 2; 
    int xCenter = M5.Lcd.width() / 2;
    int lineCount = 0;
    
    int start = 0;
    int end = 0;
    while (end >= 0) {
        end = wrappedMessage.indexOf('\n', start);
        String line = (end < 0) ? wrappedMessage.substring(start) : wrappedMessage.substring(start, end);
        line.trim();
        if (line.length() > 0) {
            lineCount++;
        }
        start = end + 1;
        if (end < 0) break;
    }
    
    int totalTextHeight = lineCount * lineHeight;
    int currentY = (M5.Lcd.height() / 2) - (totalTextHeight / 2) + (lineHeight / 2) + 5; 
    
    start = 0;
    end = 0;
    while (end >= 0) {
        end = wrappedMessage.indexOf('\n', start);
        String line = (end < 0) ? wrappedMessage.substring(start) : wrappedMessage.substring(start, end);
        line.trim();

        if (line.length() > 0) {
            M5.Lcd.drawString(line, xCenter, currentY);
            currentY += lineHeight;
        }

        start = end + 1;
        if (end < 0) break;
    }
}


// Function to display a message instantly
void displayMessage(String message, int textSize, uint16_t color = WHITE, uint16_t bgColor = BLACK) {
    M5.Lcd.fillScreen(bgColor);
    drawCenteredWrappedText(message, textSize, color, bgColor);
}

// Function to display a message with a fade-in animation
void fadeInMessage(String message, int textSize, uint16_t targetColor, uint16_t bgColor = BLACK) {
    
    for (int i = 0; i <= FADE_STEPS; i++) {
        M5.Lcd.fillScreen(bgColor); 
        uint8_t alpha = map(i, 0, FADE_STEPS, 0, 255);
        uint16_t currentColor = alphaBlend(alpha, targetColor, bgColor);
        
        drawCenteredWrappedText(message, textSize, currentColor, bgColor);
        
        // Draw auxiliary info during the fade to prevent them from disappearing
        drawClock(WHITE); 
        
        // Redraw battery percentage during fade 
        int batVoltage_mV = M5.Power.getBatteryVoltage(); 
        int percentage = constrain(map(batVoltage_mV, VOLTAGE_MIN_MV, VOLTAGE_MAX_MV, 0, 100), 0, 100);
        uint16_t bat_color = WHITE;
        if (percentage <= 20) bat_color = RED; else if (percentage <= 50) bat_color = YELLOW; else bat_color = GREEN;
        drawBatteryPercentage(percentage, bat_color); 
        
        delay(FADE_DELAY_MS);
    }
}

// Function to check if the device is being shaken
bool isShaking() {
    float accX, accY, accZ;
    M5.Imu.getAccelData(&accX, &accY, &accZ);
    float totalAcceleration = sqrt(pow(accX, 2) + pow(accY, 2) + pow(accZ, 2));
    return totalAcceleration > SHAKE_THRESHOLD;
}

// Draws the current time on the bottom right of the screen (Utility function)
void drawClock(uint16_t color /* = DARKGREY */) {
    M5.Rtc.getTime(&RTC_TimeStruct);
    
    String timeStr = String(RTC_TimeStruct.hours < 10 ? "0" : "") + String(RTC_TimeStruct.hours) + 
                     ":" + 
                     String(RTC_TimeStruct.minutes < 10 ? "0" : "") + String(RTC_TimeStruct.minutes);

    M5.Lcd.setTextSize(2); 
    M5.Lcd.setTextColor(color, BLACK);
    M5.Lcd.setTextDatum(BR_DATUM); 

    int xPos = M5.Lcd.width() - 2; 
    int yPos = M5.Lcd.height() - 2; 
    
    // --- FLICKER FIX: Clear the exact area of the time string and draw ---
    int textWidth = M5.Lcd.textWidth(timeStr);
    int textHeight = M5.Lcd.fontHeight();
    
    int clearX = xPos - textWidth;
    int clearY = yPos - textHeight;
    int padding = 2; 
    
    M5.Lcd.fillRect(clearX - padding, clearY - padding, textWidth + 2 * padding, textHeight + 2 * padding, BLACK); 

    M5.Lcd.drawString(timeStr, xPos, yPos);
    
    lastDisplayedMinute = RTC_TimeStruct.minutes;
}

// Conditional call to drawClock to prevent flickering
void updateClockDisplay() {
    M5.Rtc.getTime(&RTC_TimeStruct);
    
    if (RTC_TimeStruct.minutes != lastDisplayedMinute || lastDisplayedMinute == -1) {
        drawClock(DARKGREY);
    }
}

// Draws the battery percentage (Utility function, does not check logic)
void drawBatteryPercentage(int percentage, uint16_t color) {
    
    String batStr = String(percentage) + "%";

    M5.Lcd.setTextSize(2); 
    M5.Lcd.setTextColor(color, BLACK);
    M5.Lcd.setTextDatum(TR_DATUM); 

    int xPos = M5.Lcd.width() - 2; 
    int yPos = 2; 
    
    // --- Anti-Flicker Clear: Area is calculated based on the new text size ---
    int textWidth = M5.Lcd.textWidth(batStr);
    int textHeight = M5.Lcd.fontHeight();
    
    int clearX = xPos - textWidth;
    
    M5.Lcd.fillRect(clearX - 2, yPos - 1, textWidth + 4, textHeight + 2, BLACK); 

    M5.Lcd.drawString(batStr, xPos, yPos);
    
    lastDisplayedPercentage = percentage; 
}

// Conditional call to drawBatteryPercentage (Saves power/Prevents flicker)
void updateBatteryDisplay() {
    // 1. Get and calculate the current percentage
    int batVoltage_mV = M5.Power.getBatteryVoltage(); 
    
    int currentPercentage = map(batVoltage_mV, VOLTAGE_MIN_MV, VOLTAGE_MAX_MV, 0, 100);
    currentPercentage = constrain(currentPercentage, 0, 100);

    // 2. Apply Hysteresis for Stabilization and Extreme Power Saving (5% threshold)
    if (lastDisplayedPercentage == -1 || abs(currentPercentage - lastDisplayedPercentage) >= 5) {
        
        // 3. Determine color
        uint16_t color = WHITE;
        if (currentPercentage <= 20) {
            color = RED;
        } else if (currentPercentage <= 50) {
            color = YELLOW;
        } else {
            color = GREEN;
        }

        // 4. Draw the new value (which updates lastDisplayedPercentage inside drawBatteryPercentage)
        drawBatteryPercentage(currentPercentage, color);
    }
}


// Handles the logic for adjusting the time using Button A and B
void handleTimeAdjustment() {
    
    if (M5.BtnB.wasReleased()) {
        timeAdjustMode++; // Move to next field
        if (timeAdjustMode > 2) {
            timeAdjustMode = 0; // Exit adjustment mode
            M5.Lcd.fillScreen(BLACK);
            displayMessage("Adjustment Complete!", 2, GREEN);
            delay(1000);
            return;
        }
        M5.Rtc.getTime(&RTC_TimeStruct);
    }

    if (timeAdjustMode == 0) return;

    M5.Lcd.fillScreen(BLACK);

    // Button A now handles incrementing the value in adjustment mode
    if (M5.BtnA.wasReleased()) { 
        lastActivityTime = millis(); 
        switch (timeAdjustMode) {
            case 1: RTC_TimeStruct.hours = (RTC_TimeStruct.hours + 1) % 24; break;
            case 2: RTC_TimeStruct.minutes = (RTC_TimeStruct.minutes + 1) % 60; break;
        }
        M5.Rtc.setTime(&RTC_TimeStruct);
        lastDisplayedMinute = -1; 
    }
    
    // --- Display the current adjustment step ---
    uint16_t hourColor = (timeAdjustMode == 1) ? YELLOW : WHITE;
    uint16_t minuteColor = (timeAdjustMode == 2) ? YELLOW : WHITE;
    
    M5.Lcd.setTextDatum(MC_DATUM);
    M5.Lcd.setTextSize(4); 
    int center_x = M5.Lcd.width() / 2;
    int center_y = M5.Lcd.height() / 2;

    // Time Display
    M5.Lcd.setCursor(center_x - 40, center_y);
    M5.Lcd.setTextColor(hourColor, BLACK); M5.Lcd.printf("%02d", RTC_TimeStruct.hours);
    M5.Lcd.setTextColor(WHITE, BLACK); M5.Lcd.printf(":");
    M5.Lcd.setTextColor(minuteColor, BLACK); M5.Lcd.printf("%02d", RTC_TimeStruct.minutes);
    
    // Bottom instructions
    M5.Lcd.setTextDatum(BC_DATUM);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.drawString("A: Increment | B: Next/Save", M5.Lcd.width() / 2, M5.Lcd.height() - 5);
    
    updateBatteryDisplay(); 
}


// Function to go to sleep
void prepareToSleep() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextDatum(MC_DATUM);
    M5.Lcd.setTextColor(DARKGREY, BLACK);
    M5.Lcd.setTextSize(2); 
    M5.Lcd.drawString("Zzz...", M5.Lcd.width() / 2, M5.Lcd.height() / 2);

    esp_sleep_enable_ext0_wakeup(GPIO_NUM_37, 0); 

    M5.Lcd.setBrightness(0);
    
    esp_deep_sleep_start();
}


// =======================================================
// === 3. CORE ARDUINO FUNCTIONS ===
// =======================================================

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    
    M5.Lcd.setRotation(1); 
    randomSeed(analogRead(0)); 

    // Set initial brightness
    M5.Lcd.setBrightness(BRIGHTNESS_LEVELS[currentBrightnessIndex]); 

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
        lastActivityTime = millis();
    } else {
        displayMessage("Ask a YES/NO question and SHAKE ME!", 2, GREEN);
        lastActivityTime = millis(); 
    }
    
    updateClockDisplay();
    updateBatteryDisplay();
}

void loop() {
    M5.update(); // Read button and IMU states
    
    // 1. --- Check for Button A Long Press to ENTER Time Adjustment Mode ---
    if (M5.BtnA.isPressed()) {
        if (buttonAPressStartTime == 0) {
            buttonAPressStartTime = millis();
            // Optional visual feedback for long press
            M5.Lcd.fillRect(0, 0, 40, 15, DARKGREY); 
            M5.Lcd.setTextColor(WHITE, DARKGREY);
            M5.Lcd.setTextSize(1);
            M5.Lcd.setCursor(2, 2);
            M5.Lcd.print("Hold");
        }
        
        // If the button has been held long enough AND we are not already in adjustment mode
        if (millis() - buttonAPressStartTime >= LONG_PRESS_DURATION && timeAdjustMode == 0) {
            timeAdjustMode = 1; // Start adjustment with the Hour
            buttonAPressStartTime = 0; // Reset timer
            M5.Rtc.getTime(&RTC_TimeStruct);
            // Clear the visual feedback from the corner
            M5.Lcd.fillRect(0, 0, 40, 15, BLACK); 
            // Enter the handler to display the adjustment screen
            handleTimeAdjustment(); 
            return;
        }
    } else {
        // Button was released before the long-press duration
        if (buttonAPressStartTime != 0) {
            M5.Lcd.fillRect(0, 0, 40, 15, BLACK); // Clear the 'Hold' indicator
        }
        buttonAPressStartTime = 0;
    }
    
    // 2. --- Handle Button B Brightness Cycle (Anytime, unless in Time Adjustment Mode) ---
    if (M5.BtnB.wasReleased()) {
        // **KEY CHANGE:** Only check if we are NOT in time adjustment mode
        if (timeAdjustMode == 0) {
            // Cycle the brightness index
            currentBrightnessIndex = (currentBrightnessIndex + 1) % NUM_BRIGHTNESS_LEVELS;
            
            // Set the new brightness level
            M5.Lcd.setBrightness(BRIGHTNESS_LEVELS[currentBrightnessIndex]);
            
            // Register this as activity to prevent immediate sleep
            lastActivityTime = millis();
        }
    }


    // 3. --- Handle Time Adjustment Mode (Highest Priority) ---
    if (timeAdjustMode > 0) {
        handleTimeAdjustment();
        delay(100);
        return; 
    }
    
    // If we reach this point, we are in normal operation (timeAdjustMode == 0)

    // 4. --- Check for Shake/Answer Condition ---
    if (millis() - lastAnswerTime > cooldownPeriod) {
        if (isShaking()) {
            lastActivityTime = millis(); 
            displayMessage("Thinking...", 3, YELLOW);
            delay(1500); 
            
            int randomIndex = random(0, numAnswers);
            String selectedAnswer = answers[randomIndex];

            fadeInMessage(selectedAnswer, 2, WHITE);
            lastAnswerTime = millis();
        }
    } 

    // 5. --- Update Clock and Battery Display (Conditional, Power-Saving) ---
    updateClockDisplay();
    updateBatteryDisplay();

    // 6. --- Check for Auto-Sleep Condition ---
    if (millis() - lastActivityTime > SLEEP_TIMEOUT) {
        prepareToSleep();
    }
    
    delay(10);
}