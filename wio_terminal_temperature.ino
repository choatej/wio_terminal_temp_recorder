#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include <RTC_SAMD51.h>
#include <Seeed_Arduino_FS.h>
#include "DateTime.h"
#include <WiFi.h>
#include <WebServer.h>
#include <TFT_eSPI.h> 

#ifdef _SAMD21_
#define SDCARD_SS_PIN 1
#define SDCARD_SPI SPI
#endif

unsigned int localPort = 2390;

// used for clock sync
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

// RTC (Real Time Clock)
RTC_SAMD51 rtc;

// Replace with your network credentials
const char* ssid = "";
const char* password = "";

const int FIVE_SECONDS = 5000;

// Web server on port 80
WebServer server(80);

// SHT40 instance
Adafruit_SHT4x sht4;

// SD card chip select pin
#define SD_CS_PIN 10

// TFT Screen instance
TFT_eSPI tft = TFT_eSPI();

// timestamp for for log file name
DateTime lastLog;

// convenience for tracking the next line of text on the tft
int line_y = 0;

int nextLine() {
  line_y += 15;
  return line_y;
}

const int DEFAULT_LEFT_ALIGN = 10;

void displayMessageLeftAlign(const  char* message) {
  displayMessage(message, DEFAULT_LEFT_ALIGN, nextLine());
}

void displayMessage(const char* message, int x, int y) {
  Serial.println(message);
  tft.drawString(message, x, y);
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

void setup() {
  Serial.begin(115200);

  // Initialize the screen
  tft.begin();
  tft.setRotation(3); // Rotate for horizontal orientation
  tft.fillScreen(TFT_BLACK); // Clear the screen
  tft.setTextColor(TFT_WHITE, TFT_BLACK); // White text on black background
  tft.setTextSize(1);
  displayMessageLeftAlign("Initializing...");
  
  // Initialize I2C and SHT40
  if (!sht4.begin()) {
    displayMessageLeftAlign("Error: SHT40 not found");
    while (1);
  }
  displayMessageLeftAlign("SHT40 initialized");

  // Initialize SD card
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  while (!SD.begin(SDCARD_SS_PIN,SDCARD_SPI,4000000UL)) {
    displayMessageLeftAlign("Card Mount Failed");
    return;
  }
  displayMessageLeftAlign("SD card initialized");

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    displayMessageLeftAlign("Connecting to Wi-Fi...");
  }
  displayMessageLeftAlign("Connected to Wi-Fi");
  IPAddress ip = WiFi.localIP();
  String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
  displayMessageLeftAlign(("IP Address: " + ipStr).c_str());

  // Initialize the RTC
  deviceTime = getNTPtime();
  
  if (deviceTime == 0) {
    displayMessageLeftAlign("Failed to get the time from NTP server");
  }
  
  if (!rtc.begin()) {
    displayMessageLeftAlign("Error: Couldn't find RTC");
    while (1) delay(10);
  }

  rtc.adjust(DateTime(deviceTime));

  // if (rtc.lostPower()) {
  //   rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  //   rtc.start();
  // } 
  displayMessageLeftAlign("RTC initialized");

  // Initialize file DateTime
  lastLog = rtc.now();  

  // Set up web server
  server.on("/", []() {
    float temperature, humidity;
    sensors_event_t temp, hum;
    sht4.getEvent(&temp, &hum);
    temperature = temp.temperature;
    humidity = hum.relative_humidity;

    String page = "<h1>Temperature & Humidity</h1>";
    page += "<p>At " + toPageDate(rtc.now()) + "</p>"; 
    page += "<p>Temperature: " + String(temperature, 2) + " C</p>";
    page += "<p>Humidity: " + String(humidity, 2) + "%</p>";
    server.send(200, "text/html", page);
  });

  server.begin();
  displayMessageLeftAlign("Web server started");
  displayMessageLeftAlign(("View page at http://" + ipStr + "/").c_str());
  delay(FIVE_SECONDS);
}

void loop() {
  // Read data from sensor
  sensors_event_t temp, hum;
  sht4.getEvent(&temp, &hum);
  DateTime eventStamp = rtc.now();
  float temperature = temp.temperature;
  float humidity = hum.relative_humidity;

  // Log data to SD card
  String fileName = getFileName();
  bool isNewFile = SD.exists(fileName);
  File dataFile = SD.open(getFileName(), FILE_WRITE);
  if (dataFile) {
    if (isNewFile) {
      dataFile.println("timestamp,temperature,humidity");
    }
    dataFile.println(toLogDate(eventStamp) + "," + String(temperature) + "," + String(humidity));
    dataFile.close();
  } else {
    Serial.println("Error: Failed to open file for writing");
  }

  // TODO: see if this can be removed to eliminate flicker
  tft.fillScreen(TFT_BLACK); // Clear the screen
  tft.setTextSize(2);
  tft.drawString("Temperature: " + String(temperature, 2) + " C", 10, 20);
  tft.drawString("Humidity:    " + String(humidity, 2) + " %", 10, 50);


  // Handle client requests
  server.handleClient();

  delay(FIVE_SECONDS);
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