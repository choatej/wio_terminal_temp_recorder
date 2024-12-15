#ifdef _SAMD21_
#define SDCARD_SS_PIN 1
#define SDCARD_SPI SPI
#endif

// SD card chip select pin
#define SD_CS_PIN 10

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

unsigned int initializeSDCard() {
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  while (!SD.begin(SDCARD_SS_PIN,SDCARD_SPI,4000000UL)) {
    return 1;
  }
  return 0;
}


