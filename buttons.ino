// Button Pins
#define BUTTON_1 2
#define BUTTON_2 3
#define BUTTON_3 4

void initializeButtons() {
  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP); 
}

void handleButtonPresses() {
  if (digitalRead(BUTTON_1) == LOW) {
    displayData();
  } else if (digitalRead(BUTTON_2) == LOW) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Temperature Graph (TBD)", 10, 10);
    lastButtonPress = millis();
  } else if (digitalRead(BUTTON_3) == LOW) {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Humidity Graph (TBD)", 10, 10);
    lastButtonPress = millis();
  }
}
