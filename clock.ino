#include <stdint.h>
#include <Wire.h>
#include <Adafruit_SSD1327.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_GFX.h>    // Core graphics library
//#include <Adafruit_TFTLCD.h> // Hardware-specific library

// Use built-in font from font.h
#include "font.h"
#define FONT_96 DSEG14_Modern_Mini_Regular_80
#define FONT_48 DSEG14_Modern_Mini_Regular_48
#define FONT_24 DSEG14_Modern_Mini_Regular_24
#define FONT_22 DSEG14_Modern_Mini_Regular_22
#define FONT_16 DSEG14_Modern_Mini_Regular_16
#define FONT_12 DSEG14_Modern_Mini_Regular_12
#define FONT_10 DSEG14_Modern_Mini_Regular_10

#define GRAY_LIGHT 12
#define GRAY_MEDIUM 6
#define GRAY_DARK 3

// Used for I2C or SPI
#define OLED_RESET 16

// Select I2C BUS
void TCA9548A(uint8_t bus){
  if (bus > 7) return; // invalid bus
  Wire.beginTransmission(0x70);  // TCA9548A address
  Wire.write(1 << bus);          // send byte to select bus
  Wire.endTransmission();
  delay(2); // small settle time after switching bus
  //Serial.print("TCA bus: ");
  //Serial.println(bus);
}

// Replace with your network credentials
const char* ssid = "YOURSSID";
const char* password = "YOURPASSWORD";

// Define NTP Client to get time
WiFiUDP ntpUDP;
// use a public NTP pool by default; change to your local NTP server if needed
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// I2C
Adafruit_SSD1327 display(128, 128, &Wire, OLED_RESET);

//Week Days
String weekDays[7] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};

//Month names
String months[12] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"};

// Multiplexer bus mapping for the 4 displays
const uint8_t buses[4] = {2,3,4,5};
const uint8_t displayRotations[4] = {3,1,3,3};

uint8_t getDisplayRotation(uint8_t bus) {
  for (uint8_t i = 0; i < 4; i++) {
    if (buses[i] == bus) return displayRotations[i];
  }
  return 3;
}

// NTP sync variables
unsigned long lastNTPSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 12 * 60 * 60 * 1000; // 12 hours
int lastScrubDay = -1;
int lastRenderedHour = -1;
int lastRenderedMinute = -1;
int lastRenderedSecond = -1;
int lastRenderedDay = -1;
uint8_t lastBrightness = 255;

// Set contrast on all displays
void setBrightnessAll(uint8_t brightness) {
  for (uint8_t i = 0; i < 4; i++) {
    TCA9548A(buses[i]);
    display.setContrast(brightness);
  }
}

// Perform a short nightly scrub on all displays to reduce burn-in risk
void scrubDisplays(const uint8_t *buses, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    uint8_t b = buses[i];
    TCA9548A(b);
    display.clearDisplay();
    display.setRotation(getDisplayRotation(b));
    for (uint8_t frame = 0; frame < 5; frame++) {
      display.clearDisplay();
      for (int x = 0; x < display.width(); x += 16) {
        int offset = (x + frame * 4) % 16;
        display.fillRect(offset, 0, 8, display.height(), SSD1327_WHITE);
      }
      display.display();
      delay(240);
    }
    display.clearDisplay();
    display.display();
  }
}

// Calculate approximate sunset time based on month (returns hour)
uint8_t getApproximateSunset(int month) {
  // Approximate sunset times (middle of month) for Germany
  const uint8_t sunsetHours[] = {16, 17, 18, 20, 21, 22, 21, 21, 20, 18, 17, 16};
  if (month >= 1 && month <= 12) {
    return sunsetHours[month - 1];
  }
  return 20; // fallback
}

void setup() {
  // Initialize Serial Monitor
  //Serial.begin(74880);
  // Start I2C communication with the Multiplexer
  // Start I2C communication with the Multiplexer
  Wire.begin();
  // Use a faster I2C clock for OLED transfers; may require strong pull-ups
  Wire.setClock(1000000);
  Wire.setClockStretchLimit(2000);

    // Init all OLED displays via multiplexer
    for (uint8_t i=0; i<4; i++) {
      uint8_t b = buses[i];
      TCA9548A(b);
      //Serial.print("SSD1327 OLED initialize "); Serial.println(b);
      if (!display.begin(0x3D)) {
        //Serial.print("Unable to initialize OLED "); Serial.println(b);
        for(;;);
      }
      display.setRotation(displayRotations[i]);
      display.clearDisplay();
      display.display();
    }

  // Run start animation and display test across all OLEDs
  startAnimation(buses, 4);
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  connectWiFiWithStatus(buses, 4);
  
  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(7200);
  // show syncing status on last display
  displayStatusMessage("Syncing time", NULL, 5);
  unsigned long syncStart = millis();
  while (!timeClient.update() && (millis() - syncStart) < 10000) {
    delay(500);
  }
  if (timeClient.getEpochTime() > 1000000000) displayStatusMessage("Time synced", NULL, 5);
  else displayStatusMessage("Time sync failed", NULL, 5);
  delay(800);
  display.clearDisplay();

  //clear all buffers
  for (uint8_t i=0; i<4; i++) {
    TCA9548A(buses[i]);
    display.clearDisplay();
    display.display();
  }
}

void testfillrect(void) {
  uint8_t color = 1;
  for (uint8_t i=0; i<display.height()/2; i+=3) {
    // alternate colors
    display.fillRect(i, i, display.width()-i*2, display.height()-i*2,  i % 15 + 1);
    display.display();
    color++;
  }
}

// Show a short status message on a given bus (last display used for status)
void displayStatusMessage(const char* line1, const char* line2, uint8_t bus) {
  TCA9548A(bus);
  display.clearDisplay();
  display.setRotation(getDisplayRotation(bus));
  display.setFont(&FONT_16);
  display.setCursor(0,20);
  display.println(line1);
  if (line2) {
    display.setCursor(0,40);
    display.println(line2);
  }
  display.display();
}

// Connect WiFi and update status on the last display until connected or timeout
void connectWiFiWithStatus(const uint8_t *buses, uint8_t count) {
  uint8_t bus = buses[count-1];
  displayStatusMessage("Connect to", ssid, bus);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    TCA9548A(bus);
    display.print('.');
    display.display();
    //Serial.print('.');
    if (millis() - start > 20000) break; // 20s timeout
  }
  TCA9548A(bus);
  display.clearDisplay();
  display.setRotation(getDisplayRotation(bus));
  if (WiFi.status() == WL_CONNECTED) {
    display.setCursor(0,20);
    display.setFont(&FONT_16);
    display.println("WiFi connected");
    display.println(WiFi.localIP().toString());
  } else {
    display.setCursor(0,50);
    display.setFont(&FONT_24);
    display.println("WiFi timeout");
  }
  display.display();
}

// Start animation across all displays (simple sweep + testfill)
void startAnimation(const uint8_t *buses, uint8_t count) {
  // Multi-frame start animation: logo sweep, display test, status icons
  // 1) Sweep a simple logo across displays
  for (uint8_t frame=0; frame<8; frame++) {
    for (uint8_t i=0; i<count; i++) {
      uint8_t b = buses[i];
      TCA9548A(b);
      display.clearDisplay();
      display.setRotation(displayRotations[i]);
      display.setFont(&FONT_24);
      display.setCursor((frame%3)*8,40);
      display.println("CLOCK");
      display.display();
    }
    delay(80);
  }

  // 2) Display-test (color fills) per display
  for (uint8_t i=0; i<count; i++) {
    uint8_t b = buses[i];
    TCA9548A(b);
    display.clearDisplay();
    display.setRotation(displayRotations[i]);
    testfillrect();
    delay(180);
  }

  // 3) Status summary across displays with icons and progress bar
  // icons: WiFi, NTP, DISP, READY
  for (uint8_t i=0; i<count; i++) {
    uint8_t b = buses[i];
    TCA9548A(b);
    display.clearDisplay();
    display.setRotation(displayRotations[i]);
    display.setFont(&FONT_16);
    display.setCursor(0,20);
    if (i==0) display.println("WiFi");
    else if (i==1) display.println("NTP");
    else if (i==2) display.println("DISP");
    else display.println("READY");
    // draw a small progress bar
    for (uint8_t p=0; p<display.width(); p+=8) {
      display.drawRect(0, display.height()-12, display.width()-1, 10, SSD1327_WHITE);
      display.fillRect(2, display.height()-10, p, 6, SSD1327_WHITE);
      display.display();
      delay(40);
    }
    delay(120);
  }
}

// Format an int as a zero-padded two-digit string.
String twoDigits(int v) {
  return (v < 10 ? "0" : "") + String(v);
}

// Repaint a value in place: clear only its bounding box, then print it.
// This keeps the SSD1327 dirty-window tiny, so the following display() call
// transfers just those few rows instead of the whole 128x128 frame.
void drawValue(const GFXfont *font, int16_t x, int16_t y, const String &s) {
  display.setFont(font);
  // Size the clear box against a full-cell template ('0' lights every outer
  // segment, so it is the widest/tallest digit). Measuring the real value
  // instead would give a tight box -- and a narrow digit like '1' would fail
  // to erase the wider previous digit (e.g. when 0 changes to 1).
  String tmpl;
  for (uint16_t i = 0; i < s.length(); i++) tmpl += '0';
  int16_t bx, by;
  uint16_t bw, bh;
  display.getTextBounds(tmpl, x, y, &bx, &by, &bw, &bh);
  display.fillRect(bx - 4, by - 4, bw + 8, bh + 8, SSD1327_BLACK);
  display.setTextColor(SSD1327_WHITE, SSD1327_BLACK);
  display.setCursor(x, y);
  display.print(s);
}

void loop() {

  timeClient.update();
  time_t epochTime = timeClient.getEpochTime();
  
  // NTP Sync 2x täglich (01:00 und 13:00 Uhr)
  int currentHour = timeClient.getHours();
  if ((currentHour == 1 || currentHour == 13) && (millis() - lastNTPSync) > NTP_SYNC_INTERVAL) {
    //Serial.println("Performing scheduled NTP sync...");
    unsigned long syncStart = millis();
    while (!timeClient.update() && (millis() - syncStart) < 5000) {
      delay(100);
    }
    lastNTPSync = millis();
  }
  
  //Serial.print("Epoch Time: ");
  //Serial.println(epochTime);

  String formattedTime = timeClient.getFormattedTime();
  //Serial.print("Formatted Time: ");
  //Serial.println(formattedTime);

  String shortTime = timeClient.getShortTime();
  
  currentHour = timeClient.getHours();
  //Serial.print("Hour: ");
  //Serial.println(currentHour);

  int currentMinute = timeClient.getMinutes();
  //Serial.print("Minutes: ");
  //Serial.println(currentMinute);

  int currentSecond = timeClient.getSeconds();
  //Serial.print("Seconds: ");
  //Serial.println(currentSecond);

  String weekDay = weekDays[timeClient.getDay()];
  //Serial.print("Week Day: ");
  //Serial.println(weekDay);

  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime);

  int monthDay = ptm->tm_mday;
  //Serial.print("Month day: ");
  //Serial.println(monthDay);

  int currentMonth = ptm->tm_mon + 1;
  //Serial.print("Month: ");
  //Serial.println(currentMonth);

  int currentDayOfYear = ptm->tm_yday;
  if (currentHour == 4 && currentDayOfYear != lastScrubDay) {
    //Serial.println("Performing overnight display scrub...");
    scrubDisplays(buses, 4);
    lastScrubDay = currentDayOfYear;
    // Scrub wiped every panel, so force a full chrome redraw next time.
    lastRenderedHour = lastRenderedMinute = lastRenderedSecond = -1;
    lastRenderedDay = -1;
  }

  String currentMonthName = months[currentMonth - 1];
  //Serial.print("Month name: ");
  //Serial.println(currentMonthName);

  int currentYear = ptm->tm_year + 1900;
  //Serial.print("Year: ");
  //Serial.println(currentYear);

  String dateShort = (monthDay < 10 ? "0" : "") + String(monthDay) + "." +
                     (currentMonth < 10 ? "0" : "") + String(currentMonth) + "." +
                     (currentYear % 100 < 10 ? "0" : "") + String(currentYear % 100);

  bool hourChanged = currentHour != lastRenderedHour;
  bool minuteChanged = currentMinute != lastRenderedMinute;
  bool secondChanged = currentSecond != lastRenderedSecond;
  bool dateChanged = currentDayOfYear != lastRenderedDay;

  // Brightness control based on time of day
  uint8_t brightness = 150; // Normal brightness
  if (currentHour >= 2 && currentHour < 7) {
    // Night: minimal brightness (02:00-06:59)
    brightness = 30;
  } else {
    uint8_t sunsetHour = getApproximateSunset(currentMonth);
    if (currentHour >= (sunsetHour + 1) && currentHour < 22) {
      // Evening (1 hour after sunset until 22:00): darker
      brightness = 100;
    }
  }
  if (brightness != lastBrightness) {
    setBrightnessAll(brightness);
    lastBrightness = brightness;
  }

  //Serial.println("");

  //clear display
  //display.clearDisplay();

  // draw hour panel
  if (hourChanged) {
    TCA9548A(2);
    display.setRotation(getDisplayRotation(2));
    if (lastRenderedHour < 0) {
      // First render after boot/scrub: draw the static chrome once.
      display.fillRect(0, 0, display.width(), display.height(), SSD1327_BLACK);
      display.fillRect(0, 0, display.width(), 24, GRAY_MEDIUM);
      display.setFont(&FONT_12);
      display.setTextColor(SSD1327_WHITE, SSD1327_BLACK);
      display.setCursor(10, 18);
      display.println("STUNDE");
    }
    drawValue(&FONT_96, 0, 120, twoDigits(currentHour));
    display.display();
    lastRenderedHour = currentHour;
  }

  // draw minute panel
  if (minuteChanged) {
    TCA9548A(3);
    display.setRotation(getDisplayRotation(3));
    if (lastRenderedMinute < 0) {
      // First render after boot/scrub: draw the static chrome once.
      display.fillRect(0, 0, display.width(), display.height(), SSD1327_BLACK);
      display.fillRect(0, 0, display.width(), 24, GRAY_MEDIUM);
      display.setFont(&FONT_12);
      display.setTextColor(SSD1327_WHITE, SSD1327_BLACK);
      display.setCursor(10, 18);
      display.println("MINUTE");
    }
    drawValue(&FONT_96, 0, 120, twoDigits(currentMinute));
    display.display();
    lastRenderedMinute = currentMinute;
  }

  // draw second panel
  if (secondChanged) {
    TCA9548A(4);
    display.setRotation(getDisplayRotation(4));
    if (lastRenderedSecond < 0) {
      // First render after boot/scrub: draw the static chrome once.
      display.fillRect(0, 0, display.width(), display.height(), SSD1327_BLACK);
      display.fillRect(0, 0, display.width(), 24, GRAY_MEDIUM);
      display.setFont(&FONT_12);
      display.setTextColor(SSD1327_WHITE, SSD1327_BLACK);
      display.setCursor(10, 18);
      display.println("SEKUNDEN");
    }
    drawValue(&FONT_24, 4, 60, twoDigits(currentSecond));
    display.display();
    lastRenderedSecond = currentSecond;
  }

  // draw date panel
  if (dateChanged) {
    TCA9548A(5);
    display.setRotation(getDisplayRotation(5));
    display.fillRect(0, 0, display.width(), display.height(), SSD1327_BLACK);
    display.fillRect(0, 0, display.width(), 24, GRAY_MEDIUM);
    display.setFont(&FONT_12);
    display.setTextColor(SSD1327_WHITE, SSD1327_BLACK);
    display.setCursor(10, 18);
    display.println(weekDay);
    display.setFont(&FONT_48);
    display.setCursor(12, 76);
    display.println(monthDay);
    display.setFont(&FONT_16);
    display.drawLine(8, 84, display.width() - 9, 84, SSD1327_WHITE);
    display.setCursor(12, 106);
    display.println(currentMonthName);
    display.setCursor(12, 128);
    display.println(currentYear);
    display.display();
    lastRenderedDay = currentDayOfYear;
  }

  // Poll the time frequently instead of sleeping a fixed second.
  // Panels only redraw when their value actually changes (see *Changed flags
  // above), so this keeps the display locked to real time: the second ticks
  // over exactly at the boundary, no skips, no accumulating drift.
  delay(50);

}
