/****************************************/
/*        NTP and RTC sync setup        */
/****************************************/

unsigned int initializeSHT40() {
    if (!sht4.begin()) {
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


