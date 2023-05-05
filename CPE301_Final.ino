#include <RTClib.h>

RTC_DS1307 rtc;

void setup() {
  Serial.begin(9600);

  // SETUP RTC MODULE
  rtc.begin();

  // Set the RTC to the date & time on PC this sketch was compiled
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void loop() {
  DateTime now = rtc.now();

  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);

  delay(1000);
}
