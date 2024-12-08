#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include <RTC_SAMD51.h>
#include <Seeed_Arduino_FS.h>
#include "DateTime.h"
#include <WiFi.h>
#include <TFT_eSPI.h> 

#ifdef _SAMD21_
#define SDCARD_SS_PIN 1
#define SDCARD_SPI SPI
#endif

// Generally Useful
const unsigned int FIVE_SECONDS = 5000;

/*****************************************/
/* Replace with your network credentials */
/*****************************************/
const char* ssid = "";
const char* password = "";


/****************************************/
/*        NTP and RTC sync setup        */
/****************************************/

unsigned int localPort = 2390;

// #define USELOCALNTP
#ifdef USELOCALNTP
    char timeServer[] = "n.n.n.n";
#else
    char timeServer[] = "time.nist.gov";
#endif
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];
WiFiUDP udp;
unsigned long deviceTime;


RTC_SAMD51 rtc;

/****************************************/ 
/*          Other Device Setup          */
/****************************************/

// SHT40 instance
Adafruit_SHT4x sht4;

// SD card chip select pin
#define SD_CS_PIN 10

// TFT Screen instance
TFT_eSPI tft = TFT_eSPI();


/****************************************/ 
/*       Application Shared State       */
/****************************************/

// track the time of the latest sample
DateTime eventStamp;

// timestamp for for log file names
DateTime lastLog;

// convenience for tracking the next line of text on the tft
int line_y = 0;

// Most text start at x = 10
const int DEFAULT_LEFT_ALIGN = 10;

/****************************************/
/*           Utility Functions          */
/****************************************/

// stop on failures
void fail(const char* message) {
  displayMessageLeftAlign(message);
  while (1) delay(10);
}

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

void displayData(float temperature, float humidity) {
    // TODO: see if this can be removed to eliminate flicker
  tft.fillScreen(TFT_BLACK); // Clear the screen
  tft.setTextSize(2);
  tft.drawString("Temperature: " + String(temperature, 2) + " C", 10, 20);
  tft.drawString("Humidity:    " + String(humidity, 2) + " %", 10, 50);
}

String getFileName() {
    // Initialize the log file name
  DateTime now = rtc.now();
  uint32_t diffSeconds = abs(now.unixtime() - lastLog.unixtime());
  // Convert seconds to hours and compare
  if (diffSeconds > 3600) {
    now = lastLog;
  } else {
    lastLog = now;
  }
  // Format timestamp as YYYY-MM-DD_HH-MM-SS
  char timestamp[20];
  snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d_%02d-%02d-%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second()); 
  return String("temps_") + timestamp + ".csv";
}

String toLogDate(DateTime dt) {
  return String(dt.year()) + "-" + 
         String(dt.month()) + "-" + 
         String(dt.day()) + " " + 
         String(dt.hour()) + ":" + 
         String(dt.minute()) + ":" + 
         String(dt.second());
}

String toPageDate(DateTime dt) {
  return String(dt.year()) + "-" + String(dt.month()) + "-" + String(dt.day()) +
  " " +
  String(dt.hour()) + ":" + String(dt.minute()) + ":" + String(dt.second());

}

/****************************************/
/* Application Initialization Functions */
/****************************************/

void initializeScreen() {
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
}

unsigned int initializeSHT40() {
    if (!sht4.begin()) {
    return 1;
  }
  return 0;
}

unsigned int initializeSDCard() {
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  while (!SD.begin(SDCARD_SS_PIN,SDCARD_SPI,4000000UL)) {
    return 1;
  }
  return 0;
}

unsigned int initializeRTC() {
  deviceTime = getNTPtime();  
  if (deviceTime == 0) {
    return 1; // failed to get time from NTP
  }
  
  if (!rtc.begin()) {
    return 2; // Could not find the RTC
  }
    rtc.adjust(DateTime(deviceTime));
  return 0;
}

unsigned long getNTPtime() {

    // module returns a unsigned long time valus as secs since Jan 1, 1970 
    // unix time or 0 if a problem encounted

    //only send data when connected
    if (WiFi.status() == WL_CONNECTED) {
        //initializes the UDP state
        //This initializes the transfer buffer
        udp.begin(WiFi.localIP(), localPort);

        sendNTPpacket(timeServer); // send an NTP packet to a time server
        // wait to see if a reply is available
        delay(1000);
        if (udp.parsePacket()) {
            Serial.println("udp packet received");
            Serial.println("");
            // We've received a packet, read the data from it
            udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

            //the timestamp starts at byte 40 of the received packet and is four bytes,
            // or two words, long. First, extract the two words:

            unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
            unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
            // combine the four bytes (two words) into a long integer
            // this is NTP time (seconds since Jan 1 1900):
            unsigned long secsSince1900 = highWord << 16 | lowWord;
            // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
            const unsigned long seventyYears = 2208988800UL;
            // subtract seventy years:
            unsigned long epoch = secsSince1900 - seventyYears;

            // adjust time for timezone offset in secs +/- from UTC
            // WA time offset from UTC is +8 hours (28,800 secs)
            // + East of GMT
            // - West of GMT
            long tzOffset = 28800UL;

            // WA local time 
            unsigned long adjustedTime;
            return adjustedTime = epoch + tzOffset;
        }
        else {
            // were not able to parse the udp packet successfully
            // clear down the udp connection
            udp.stop();
            return 0; // zero indicates a failure
        }
        // not calling ntp time frequently, stop releases resources
        udp.stop();
    }
    else {
        // network not connected
        return 0;
    }

}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(const char* address) {
    // set all bytes in the buffer to 0
    for (int i = 0; i < NTP_PACKET_SIZE; ++i) {
        packetBuffer[i] = 0;
    }
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udp.beginPacket(address, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
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

  // Initialize file DateTime
  lastLog = rtc.now();  

  displayMessageLeftAlign("Initialization Complete");
  delay(FIVE_SECONDS);
}

/****************************************/
/*        Application Functions         */
/****************************************/

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
  displayData(temperature, humidity);

  delay(FIVE_SECONDS);
}




