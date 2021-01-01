// WiFi connection
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

/*
 * Enter your SSID & Password here to connect for the first time.
 * Once the sketch has been run at least once, the values will be stored
 * on the ESP8266 by the WiFi system. You can then safely remove them
 * from here again and leave the empty string -- this will reconnect
 * to your previous AP.
 */
const char *ssid = "";
const char *password = "";

// Filesystem
#include <LittleFS.h>

// LED configuration
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include "FastLED.h"
#define CLOCK_PIN 14 // GPIO14 aka D5
#define SHELF_PIN 12 // GPIO12 aka D6

#define NR_LINE_LEDS 9
#define NR_DIGIT_LEDS (NR_LINE_LEDS * 7)
#define NR_DIGITS 4
#define NR_CLOCK_LEDS (NR_DIGITS * NR_DIGIT_LEDS)
#define NR_SHELF_LEDS 14
CRGB clockleds[NR_CLOCK_LEDS];
CRGB shelfleds[NR_SHELF_LEDS];

enum StripState {
  Off = 0,
  On = 1,
};

// Numbered backwards for easy offsets
enum ClockDigit {
  M2 = 0,
  M1 = 1,
  H2 = 2,
  H1 = 3,
};

enum StripState clockstatus = Off;
enum StripState shelfstatus = Off;

#define FRAMES_PER_SECOND  60

// Uncomment for the real clock, otherwise 8 test leds are used
// #define REAL_CLOCK_STRIP 1

// Webserver configuration
ESP8266WebServer server(80);
String prehtml;
String posthtml;

// Real time clock and NTP
#include <Wire.h>
#define I2S_SDA_PIN 4 // GPIO4 aka D2
#define I2S_SCL_PIN 5 // GPIO5 aka D1

#include <AceRoutine.h>
#include <AceTime.h>
using namespace ace_routine;
using namespace ace_time;
using namespace ace_time::clock;

DS3231Clock dsClock;
NtpClock ntpClock("europe.pool.ntp.org");
SystemClockCoroutine systemClock(&ntpClock, &dsClock);
static BasicZoneProcessor zoneProcessor;

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println();

  // Set up leds
  Serial.printf("Setting up clock pixel strip on pin %d\n", CLOCK_PIN);
  FastLED.addLeds<NEOPIXEL, CLOCK_PIN>(clockleds, NR_CLOCK_LEDS);
  Serial.printf("Setting up shelf pixel strip on pin %d\n", SHELF_PIN);
  FastLED.addLeds<NEOPIXEL, SHELF_PIN>(shelfleds, NR_SHELF_LEDS);

  // Set up filesystem
  LittleFS.begin();

  // Set up WiFi
  if (ssid != "") {
    Serial.printf("Connecting to SSID \"%s\"", ssid);
  } else {
    Serial.print(F("Connecting to previously defined AP"));
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.printf("Connected, IP %s\n", WiFi.localIP().toString().c_str());

  // Set up RTC and NTP
  Serial.printf("Setting up I2S bus SDA pin %d SCL pin %d\n", I2S_SDA_PIN, I2S_SCL_PIN);
  Wire.begin(I2S_SDA_PIN, I2S_SCL_PIN);
  Serial.println(F("Setting up DS3231Clock"));
  dsClock.setup();
  Serial.println(F("Setting up NTP"));
  ntpClock.setup();
  Serial.println(F("Setting up SystemClock coroutine"));
  systemClock.setupCoroutine(F("systemClock"));
  CoroutineScheduler::setup();

  // Print current RTC time for debugging
  Serial.print(F("RTC current date/time at startup: "));
  auto tz = TimeZone::forZoneInfo(&zonedb::kZoneEurope_Amsterdam, &zoneProcessor);
  acetime_t rtcNow = dsClock.getNow();
  auto rtcDateTime = ZonedDateTime::forEpochSeconds(rtcNow, tz);
  rtcDateTime.printTo(Serial);
  Serial.println();

  // Set up webserver
  File templ = LittleFS.open("/template.html", "r");
  if (!templ) {
    Serial.println(F("ERROR: HTML template /template.html not found, looping!"));
    while (1) ;
  }
  prehtml = templ.readStringUntil('@');
  posthtml = templ.readString();
  templ.close();

  Serial.println(F("Starting webserver..."));
  server.on("/", handle_connect);
  server.on("/set", HTTP_POST, handle_set);
  server.onNotFound(handle_notfound);
  server.begin();

  // Ready
  Serial.printf("Accepting connections at http://%s\n", WiFi.localIP().toString().c_str());
}

void clearClock()
{
  fill_solid(clockleds, NR_CLOCK_LEDS, CRGB::Black);
}

void setClockDigitTest(int value, ClockDigit digit, CRGB color)
{
  // Use the first 8 leds I have on the test strip to "display the time"
  int offset = digit * 2;
  CRGB mapping[5] = { CRGB::Red, CRGB::Blue, CRGB::Yellow, CRGB::Green, CRGB::White };

  switch (value % 10) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
      fill_solid(&clockleds[offset], 1, mapping[value % 5]);
      break;
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
      fill_solid(&clockleds[offset], 1, mapping[value % 5]);
      fill_solid(&clockleds[offset + 1], 1, color);
      break;
  }
}

void setClockDigitReal(int value, ClockDigit digit, CRGB color)
{
  int offset = digit * NR_DIGIT_LEDS;
  switch (value % 10) {
    case 0:
      fill_solid(&clockleds[offset], 27, color);
      fill_solid(&clockleds[offset + 36], 27, color);
      break;
    case 1:
      fill_solid(&clockleds[offset], 9, color);
      fill_solid(&clockleds[offset + 36], 9, color);
      break;
    case 2:
      fill_solid(&clockleds[offset], 18, color);
      fill_solid(&clockleds[offset + 27], 9, color);
      fill_solid(&clockleds[offset + 45], 18, color);
      break;
    case 3:
      fill_solid(&clockleds[offset], 18, color);
      fill_solid(&clockleds[offset + 27], 27, color);
      break;
    case 4:
      fill_solid(&clockleds[offset], 9, color);
      fill_solid(&clockleds[offset + 18], 27, color);
      break;
    case 5:
      fill_solid(&clockleds[offset + 9], 45, color);
      break;
    case 6:
      fill_solid(&clockleds[offset + 9], 54, color);
      break;
    case 7:
      fill_solid(&clockleds[offset], 18, color);
      fill_solid(&clockleds[offset + 36], 9, color);
      break;
    case 8:
      fill_solid(&clockleds[offset], 63, color);
      break;
    case 9:
      fill_solid(&clockleds[offset], 45, color);
      break;
  }
}

void clearShelf()
{
  fill_solid(shelfleds, NR_SHELF_LEDS, CRGB::Black);
}

void turnShelfOn()
{
  fill_solid(shelfleds, NR_SHELF_LEDS, CRGB::Fuchsia);
}

void loop()
{
  EVERY_N_MILLISECONDS(200) {
    clearClock();
    if (clockstatus == On) {
      acetime_t now = systemClock.getNow();
      auto tz = TimeZone::forZoneInfo(&zonedb::kZoneEurope_Amsterdam, &zoneProcessor);
      auto currentDateTime = ZonedDateTime::forEpochSeconds(now, tz);
      uint8_t minute = currentDateTime.minute();
      uint8_t hour = currentDateTime.hour();
#if defined(REAL_CLOCK_STRIP)
      setClockDigitReal(minute % 10, M2, CRGB::Peru);
      setClockDigitReal(minute / 10, M1, CRGB::PowderBlue);
      setClockDigitReal(hour % 10, H2, CRGB::Salmon);
      setClockDigitReal(hour / 10, H1, CRGB::LawnGreen);
#else
      uint8_t seconds = currentDateTime.second();
      setClockDigitTest(seconds % 10, M2, CRGB::Peru);
      setClockDigitTest(seconds / 10, M1, CRGB::PowderBlue);
      setClockDigitTest(minute % 10, H2, CRGB::Salmon);
      setClockDigitTest(minute / 10, H1, CRGB::LawnGreen);
#endif
    }

    if (shelfstatus == On) {
      turnShelfOn();
    } else if (shelfstatus == Off) {
      clearShelf();
    }
  }

  EVERY_N_MILLISECONDS(30000) {
    acetime_t now = systemClock.getNow();
    Serial.print("Current date/time: ");
    auto tz = TimeZone::forZoneInfo(&zonedb::kZoneEurope_Amsterdam, &zoneProcessor);
    auto currentDateTime = ZonedDateTime::forEpochSeconds(now, tz);
    currentDateTime.printTo(Serial);
    Serial.println();
  }

  FastLED.show();
  
  server.handleClient();

  CoroutineScheduler::loop();

  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void handle_connect() {
  server.send(200, "text/html", generate_html()); 
}

void redirect_to_main() {
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handle_set() {
  if (!server.hasArg("lights")) {
    Serial.println(F("No lights parameter send by browser"));
    redirect_to_main();
    return;
  }
  String lights = server.arg("lights");
  if (lights != "clock" && lights != "shelf") {
    Serial.println(F("Invalid lights parameter send by browser"));
    redirect_to_main();
    return;
  }

  if (!server.hasArg("selection")) {
    Serial.println(F("No selection parameter send by browser"));
    redirect_to_main();
    return;
  }
  String selection = server.arg("selection");
  if (selection != "on" && selection != "off" && selection != "blink") {
    Serial.println(F("Invalid selection parameter send by browser"));
  }
  
  Serial.printf("%s: %s\n", lights.c_str(), selection.c_str());
  if (lights == "clock" ) {
    if (selection == "on") {
      clockstatus = On;
    } else if (selection == "off") {
      clockstatus = Off;
    }
  } else if (lights == "shelf") {
    if (selection == "on") {
      shelfstatus = On;
    } else if (selection == "off") {
      shelfstatus = Off;
    }
  }

  redirect_to_main();
}

void handle_notfound(){
  server.send(404, "text/plain", "Not found");
}

void append_post_button(String *html, String strip, String selection, String text)
{
  *html += "<form method=\"post\" action=\"/set\"><input type=\"hidden\" name=\"lights\" value=\"" + strip + "\">"
        + "<button type=\"submit\" name=\"selection\" value=\"" + selection + "\" class=\"button button-" + selection + "\">"
        + text
        + "</button>"
        + "</form>\n";
}

String generate_html(){
  String ptr = "";
  ptr += prehtml;
  
  if (clockstatus == On) {
    ptr +="<p>Clock Status: ON</p>";
    append_post_button(&ptr, "clock", "off", "OFF");
  } else if (clockstatus == Off) {
    ptr +="<p>Clock Status: OFF</p>";
    append_post_button(&ptr, "clock", "on", "ON");
  }

  if (shelfstatus == On) {
    ptr +="<p>Shelf Status: ON</p>";
    append_post_button(&ptr, "shelf", "off", "OFF");
  } else if (shelfstatus == Off) {
    ptr +="<p>Shelf Status: OFF</p>";
    append_post_button(&ptr, "shelf", "on", "ON");
  }

  ptr += posthtml;
  return ptr;
}
