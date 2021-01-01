#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
//#include <EEPROM.h>

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

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println();

  // Set up leds
  Serial.println("Setting up pixel strips on pin 5 and pin 4");
  FastLED.addLeds<NEOPIXEL, 5>(clockleds, NR_CLOCK_LEDS);
  FastLED.addLeds<NEOPIXEL, 4>(shelfleds, NR_SHELF_LEDS);

  // Set up filesystem
  LittleFS.begin();

  // Set up WiFi
  Serial.println("Getting WiFi configuration from file system");
  File conf = LittleFS.open("/wifi.cfg", "r");
  if (!conf) {
    Serial.println("WiFi configuration file does not exist, writing default values...");
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
    Serial.println("WiFi configuration file found, reading values...");
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

  // Set up webserver
  File templ = LittleFS.open("/template.html", "r");
  if (!templ) {
    Serial.println("ERROR: HTML template /template.html not found, looping!");
    while (1) ;
  }
  prehtml = templ.readStringUntil('@');
  posthtml = templ.readString();
  templ.close();

  Serial.println("Starting webserver...");
  server.on("/", handle_OnConnect);
  server.on("/strip1on", HTTP_POST, handle_strip1on);
  server.on("/strip1blink", HTTP_POST, handle_strip1blink);
  server.on("/strip1off", HTTP_POST, handle_strip1off);
  server.on("/strip2on", HTTP_POST, handle_strip2on);
  server.on("/strip2blink", HTTP_POST, handle_strip2blink);
  server.on("/strip2off", HTTP_POST, handle_strip2off);
  server.on("/testing", handle_test);
  server.onNotFound(handle_NotFound);
  server.begin();

  // Ready
  Serial.println("Accepting connections");
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

  FastLED.show();
  
  server.handleClient();

  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML()); 
}

void handle_strip1on() {
  clockstatus = On;
  Serial.println("Strip 1: ON");
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handle_strip1off() {
  clockstatus = Off;
  Serial.println("Strip 1: OFF");
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handle_strip1blink() {
  clockstatus = Blink;
  Serial.println("Strip 1: Blink");
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handle_strip2on() {
  shelfstatus = On;
  Serial.println("Strip 2: ON");
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handle_strip2off() {
  shelfstatus = Off;
  Serial.println("Strip 2: OFF");
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handle_strip2blink() {
  shelfstatus = Blink;
  Serial.println("Strip 2: Blink");
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handle_test() {
  Serial.println("Serve test file");
  File file = LittleFS.open("/test.txt", "r");
  server.streamFile(file, "text/plain");
  file.close();
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

void append_post_button(String *html, String buttonClass, String URL, String text)
{
  *html += "<form method=\"post\" action=\"" + URL + "\"><button type=\"submit\" name=\"selection\" value=\"" + buttonClass + "\" class=\"button button-" + buttonClass + "\">"
    + text + "</button></form>\n";
}

String SendHTML(){
  String ptr = "";
  ptr += prehtml;
  
  if (clockstatus == On) {
    ptr +="<p>Clock Status: ON</p>";
    append_post_button(&ptr, "off", "/strip1off", "OFF");
    append_post_button(&ptr, "blink", "/strip1blink", "BLINK");
  } else if (clockstatus == Off) {
    ptr +="<p>Clock Status: OFF</p>";
    append_post_button(&ptr, "on", "/strip1on", "ON");
    append_post_button(&ptr, "blink", "/strip1blink", "BLINK");
  } else {
    ptr +="<p>Clock Status: BLINK</p>";
    append_post_button(&ptr, "on", "/strip1on", "ON");
    append_post_button(&ptr, "off", "/strip1off", "OFF");
  }

  if (shelfstatus == On) {
    ptr +="<p>Shelf Status: ON</p>";
    append_post_button(&ptr, "off", "/strip2off", "OFF");
    append_post_button(&ptr, "blink", "/strip2blink", "BLINK");
  } else if (shelfstatus == Off) {
    ptr +="<p>Shelf Status: OFF</p>";
    append_post_button(&ptr, "on", "/strip2on", "ON");
    append_post_button(&ptr, "blink", "/strip2blink", "BLINK");
  } else {
    ptr +="<p>Shelf Status: BLINK</p>";
    append_post_button(&ptr, "on", "/strip2on", "ON");
    append_post_button(&ptr, "off", "/strip2off", "OFF");
  }

  ptr += posthtml;
  return ptr;
}
