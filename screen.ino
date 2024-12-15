// Most text start at x = 10
const int DEFAULT_LEFT_ALIGN = 10;

const unsigned int SCREEN_TIMEOUT_MS = 5000;

// Display text
void displayMessage(const char* message, int x, int y) {
  Serial.println(message);
  tft.drawString(message, x, y);
}

// Display text left aligned
void displayMessageLeftAlign(const  char* message) {
  displayMessage(message, DEFAULT_LEFT_ALIGN, nextLine());
}

unsigned int nextLine() {
  line_y += 15;
  return line_y;
}

void initializeScreen() {
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
}

unsigned int logSample(float temperature, float humidity) {
  String fileName = getFileName();
  bool isNewFile = SD.exists(fileName);
  File dataFile = SD.open(getFileName(), FILE_WRITE);
  if (dataFile) {
    if (isNewFile) {
      // add a header row
      dataFile.println("timestamp,temperature,humidity");
    }
    dataFile.println(toLogDate(eventStamp) + "," + String(temperature) + "," + String(humidity));
    dataFile.close();
  } else {
    return 1;
  }
  return 0;
}

void displayData() {
    // TODO: see if this can be removed to eliminate flicker
  tft.fillScreen(TFT_BLACK); // Clear the screen
  tft.setTextSize(2);
  tft.drawString("Temperature: " + String(lastTemperature, 2) + " C", 10, 20);
  tft.drawString("Humidity:    " + String(lastHumidity, 2) + " %", 10, 50);
  screenOn = true;
  lastButtonPress = millis();
}

void manageScreen() {
  handleButtonPresses();

  // Turn off the screen if timeout has elapsed
  if (screenOn && millis() - lastButtonPress > SCREEN_TIMEOUT_MS) {
    tft.fillScreen(TFT_BLACK); // Clear the screen
    screenOn = false;
  }
}