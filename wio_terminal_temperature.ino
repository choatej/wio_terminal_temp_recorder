#include <Adafruit_SHT4x.h>
#include <Adafruit_SleepyDog.h>
#include <RTC_SAMD51.h>
#include <Seeed_Arduino_FS.h>
#include "DateTime.h"
#include <TFT_eSPI.h> 
#include <Wire.h>
#include <WiFi.h>

/****************************************/ 
/*              Constants               */
/****************************************/

const int NTP_PACKET_SIZE = 48;

// Replace with your network credentials
const char* ssid = "";
const char* password = "";

// default to 15 minutes
const unsigned int sleepIntervalSeconds = 900;

// Generally Useful
const unsigned int FIVE_SECONDS = 5000;

/****************************************/ 
/*       Application Shared State       */
/****************************************/
unsigned long deviceTime;

RTC_SAMD51 rtc;

// power management variables
unsigned long lastButtonPress = 0;

// track the latest sample
DateTime eventStamp;
float lastTemperature;
float lastHumidity;

// how long before going back to sleep
unsigned int screenTimeout = FIVE_SECONDS;

// TFT Screen instance
TFT_eSPI tft = TFT_eSPI();

// Should the screen be on?
bool screenOn = false;

// convenience for tracking the next line of text on the tft
int line_y = 0;

// timestamp for for log file names
DateTime lastLog;

// SHT40 instance
Adafruit_SHT4x sht4;

byte packetBuffer[NTP_PACKET_SIZE];

WiFiUDP udp;

/****************************************/
/*           Utility Functions          */
/****************************************/
// stop on failures
void fail(const char* message) {
  displayMessageLeftAlign(message);
  while (1) delay(10);
}

/****************************************/
/*                setup()               */
/****************************************/
void setup() {
  Serial.begin(115200);

  // Initialize the screen
  initializeScreen();  
  displayMessageLeftAlign("Initializing...");

  // Initialize I2C and SHT40
  if (initializeSHT40() == 0) {
    displayMessageLeftAlign("SHT40 initialized");
  } else {
    fail("Error: SHT40 not found");
  }

  if (initializeSDCard() == 0) {
    displayMessageLeftAlign("SD card initialized");
  } else {
    fail("Error: Card Mount Failed");
  }

  // Initialize the RTC
  unsigned int rtcSuccess = initializeRTC();
  if (rtcSuccess == 0) {
    displayMessageLeftAlign("RTC initialized");
  } else {
    if (rtcSuccess == 1) {
      fail("Error: Failed to get the time from NTP server");
    } else {
      fail("Error: Couldn't find RTC");
    }
  }

  // Initialize the three buttons at the top
  initializeButtons();

  // Initialize file DateTime
  lastLog = rtc.now();  

  displayMessageLeftAlign("Initialization Complete");
  delay(FIVE_SECONDS);
}

/****************************************/
/*                loop()                */
/****************************************/
void loop() {
  // Read data from sensor
  sensors_event_t temp, hum;
  sht4.getEvent(&temp, &hum);
  eventStamp = rtc.now();
  float temperature = temp.temperature;
  float humidity = hum.relative_humidity;

  // Log data to SD card
  if (logSample(temperature, humidity) != 0) {
    Serial.println("Error: Failed to write t: " + String(temperature) + " h: " + String(humidity) + " to SD Card");
  }

  manageScreen();

  if (!screenOn) {
    Serial.println("Sleeping for " + String(sleepIntervalSeconds / 60) + " minutes...");
    Watchdog.sleep(sleepIntervalSeconds * 1000);
  }
}
