#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

/*
 * Enter your SSID & Password here to connect for the first time.
 * Once the sketch has been run at least once, the values will be stored in the LittleFS filesystem.
 * You can then safely remove them from here again.
 */
const char *ssid = "";
const char *password = "";

// Stores the WiFi configuration
struct WifiData {
  char ssid[20] = "";
  char password[20] = "";
} wifiData;

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
  Blink = 2,
};

enum StripState clockstatus = Off;
uint8_t clockblinkstatus = LOW;
enum StripState shelfstatus = Off;
uint8_t shelfblinkstatus = LOW;

#define FRAMES_PER_SECOND  60

// Webserver Configuration
ESP8266WebServer server(80);
String prehtml;
String posthtml;

// Real time clock and NTP
#include <Wire.h>
#define RTC_SDA_PIN 4 // GPIO4 aka D2
#define RTC_SCL_PIN 5 // GPIO5 aka D1

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
  Serial.println(F("Setting up pixel strips on pin 5 and pin 4"));
  FastLED.addLeds<NEOPIXEL, 14>(clockleds, NR_CLOCK_LEDS);
  FastLED.addLeds<NEOPIXEL, 12>(shelfleds, NR_SHELF_LEDS);

  // Set up filesystem
  LittleFS.begin();

  // Set up WiFi
  Serial.println(F("Getting WiFi configuration from file system"));
  File conf = LittleFS.open("/wifi.cfg", "r");
  if (!conf) {
    Serial.println(F("WiFi configuration file does not exist, writing default values..."));
    conf = LittleFS.open("/wifi.cfg", "w");
    conf.write(ssid, strlen(ssid));
    conf.write("\n");
    conf.write(password, strlen(password));
    conf.write("\n");
    strncpy(wifiData.ssid, ssid, 20);
    wifiData.ssid[19] = '\0';
    strncpy(wifiData.password, password, 20);
    wifiData.password[19] = '\0';
  } else {
    Serial.println(F("WiFi configuration file found, reading values..."));
    String SSID = conf.readStringUntil('\n');
    strncpy(wifiData.ssid, SSID.c_str(), 20);
    wifiData.ssid[19] = '\0';
    String PASS = conf.readStringUntil('\n');
    strncpy(wifiData.password, PASS.c_str(), 20);
    wifiData.password[19] = '\0';
    conf.close();
  }

  Serial.printf("Connecting to SSID \"%s\"", wifiData.ssid);
  WiFi.begin(wifiData.ssid, wifiData.password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.printf("Connected with IP %s\n", WiFi.localIP().toString().c_str());

  // Set up RTC and NTP
  Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
  Serial.println(F("Setting up DS3231Clock"));
  dsClock.setup();
  Serial.println(F("Setting up NTP"));
  ntpClock.setup();
  Serial.println(F("Setting up SystemClock coroutine"));
  systemClock.setupCoroutine(F("systemClock"));
  CoroutineScheduler::setup();

  // Print current RTC time for debugging
  Serial.print("RTC current date/time: ");
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
  Serial.println(F("Accepting connections"));
}

void loop()
{
  EVERY_N_MILLISECONDS(300) {
    if ((clockstatus == On) || (clockstatus == Blink && clockblinkstatus == LOW)) {
      clockleds[0] = CRGB::White;
      clockleds[1] = CRGB::White;
      clockleds[2] = CRGB::White;
      clockleds[3] = CRGB::White;
      clockleds[4] = CRGB::White;
      clockleds[5] = CRGB::White;
      clockleds[6] = CRGB::White;
      clockleds[7] = CRGB::White;
      clockleds[8] = CRGB::White;
      clockblinkstatus = HIGH;
    } else if ((clockstatus == Off) || (clockstatus == Blink && clockblinkstatus == HIGH)) {
      clockleds[0] = CRGB::Black;
      clockleds[1] = CRGB::Black;
      clockleds[2] = CRGB::Black;
      clockleds[3] = CRGB::Black;
      clockleds[4] = CRGB::Black;
      clockleds[5] = CRGB::Black;
      clockleds[6] = CRGB::Black;
      clockleds[7] = CRGB::Black;
      clockleds[8] = CRGB::Black;
      clockblinkstatus = LOW;        
    }
  }

  EVERY_N_MILLISECONDS(300) {
    if ((shelfstatus == On) || (shelfstatus == Blink && shelfblinkstatus == LOW)) {
      shelfleds[0] = CRGB::White;
      shelfleds[1] = CRGB::White;
      shelfleds[2] = CRGB::White;
      shelfleds[3] = CRGB::White;
      shelfleds[4] = CRGB::White;
      shelfleds[5] = CRGB::White;
      shelfleds[6] = CRGB::White;
      shelfleds[7] = CRGB::White;
      shelfleds[8] = CRGB::White;
      shelfblinkstatus = HIGH;
    } else if ((shelfstatus == Off) || (shelfstatus == Blink && shelfblinkstatus == HIGH)) {
      shelfleds[0] = CRGB::Black;
      shelfleds[1] = CRGB::Black;
      shelfleds[2] = CRGB::Black;
      shelfleds[3] = CRGB::Black;
      shelfleds[4] = CRGB::Black;
      shelfleds[5] = CRGB::Black;
      shelfleds[6] = CRGB::Black;
      shelfleds[7] = CRGB::Black;
      shelfleds[8] = CRGB::Black;
      shelfblinkstatus = LOW;
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
    } else if (selection == "blink") {
      clockstatus = Blink;
    }
  } else if (lights == "shelf") {
    if (selection == "on") {
      shelfstatus = On;
    } else if (selection == "off") {
      shelfstatus = Off;
    } else if (selection == "blink") {
      shelfstatus = Blink;
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
    append_post_button(&ptr, "clock", "blink", "BLINK");
  } else if (clockstatus == Off) {
    ptr +="<p>Clock Status: OFF</p>";
    append_post_button(&ptr, "clock", "on", "ON");
    append_post_button(&ptr, "clock", "blink", "BLINK");
  } else {
    ptr +="<p>Clock Status: BLINK</p>";
    append_post_button(&ptr, "clock", "on", "ON");
    append_post_button(&ptr, "clock", "off", "OFF");
  }

  if (shelfstatus == On) {
    ptr +="<p>Shelf Status: ON</p>";
    append_post_button(&ptr, "shelf", "off", "OFF");
    append_post_button(&ptr, "shelf", "blink", "BLINK");
  } else if (shelfstatus == Off) {
    ptr +="<p>Shelf Status: OFF</p>";
    append_post_button(&ptr, "shelf", "on", "ON");
    append_post_button(&ptr, "shelf", "blink", "BLINK");
  } else {
    ptr +="<p>Shelf Status: BLINK</p>";
    append_post_button(&ptr, "shelf", "on", "ON");
    append_post_button(&ptr, "shelf", "off", "OFF");
  }

  ptr += posthtml;
  return ptr;
}
