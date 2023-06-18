#include <ArduinoJson.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <LiquidCrystal_I2C.h>
#include <TimeLib.h>
#include <TZ.h>
#include "wifi-creds.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);
DynamicJsonDocument json_buf(4096);

#define DEFAULT_FETCH_TIMEOUT 60
#define TEXT_STEP_MAX 255
#define TIMEZONE_OFFSET 7200

uint8_t fetch_timeout = 0;
uint8_t step = 0;

struct dep_info {
  const char* dest;
  long long rt_departure;
  long long departure;
};

struct dep_info departures[2] = {{.dest = "nowhere"}, {.dest = "oops"}};

void setup() {

  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.blink();

  WiFi.begin(SSID, WPA2_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(0, 0);
    lcd.println("WiFi init..");
  }


  Serial.println("connected to wifi");
  Serial.println(WiFi.localIP());
  configTime(TZ_Europe_Budapest, "pool.ntp.org");
}


int select_departure(time_t current, struct dep_info *dep_info, size_t offset) {
  JsonArray array = json_buf.as<JsonArray>();

  for (int i = offset; i < array.size(); i++) {
    long long rt_departure = ((long long) array[i]["realtimeDepartureTime"]) / 1000;
    if (rt_departure > (current + 4 * 60)) { // time delta, let's set to 4 mins
      dep_info->rt_departure = rt_departure;
      dep_info->departure = ((long long) array[i]["plannedDepartureTime"]) / 1000;
      dep_info->dest = array[i]["destination"];

      return i;
    }
  }

  dep_info->rt_departure = 0;
  dep_info->departure = 0;
  dep_info->dest = "Laufen...";

  return 0;
}

void choose_departures() {
  time_t now;
  time(&now);

  
  int offset = select_departure(now, departures, 0);
  select_departure(now, departures + 1, offset + 1);
}

size_t convert_chars(char dest[], const char* src, size_t len) {
  size_t new_len = 0;
  for (size_t i = 0; i < len; i++) {
    //Serial.printf("%c %d\n", src[i], src[i]);
    if (i + 1 < len && src[i] == (char) 0xc3) { // possibly some unicode
      switch ((int) src[i+1]) {
        case 0x9f: // scharfes s
          dest[new_len++] = (char) 0xe2;
          break;
        case 0x84: // Ä
        case 0xa4: // ä
          dest[new_len++] = (char) 0xe1;
          break;
        case 0x96: // Ö
        case 0xb6: // ö
          dest[new_len++] = (char) 0xef;
          break;
        case 0x9c: // Ü
        case 0xbc: // ü
          dest[new_len++] = (char) 0xf5;
          break;
        default:
          dest[new_len++] = '?';
      }
      i += 1;
      continue;
    }
    dest[new_len++] = src[i];
  }
  return new_len;
}

void print_departures(uint8_t step) {

    char buff[17];
    char dst_buf[TEXT_STEP_MAX];
    for (uint8_t i = 0; i < 2; i++) {
      size_t len = convert_chars(dst_buf, departures[i].dest, strnlen(departures[i].dest, TEXT_STEP_MAX));

      if (len <= 10) {
        memcpy(buff, dst_buf, len);
      } else {
        size_t offset = step % (len - 9);
        memcpy(buff, dst_buf + offset, 10);
      }

      snprintf(buff + 11, 6, "%02d:%02d",
        hour(departures[i].rt_departure + TIMEZONE_OFFSET),
        minute(departures[i].rt_departure));

      buff[10] = ' ';
      buff[16] = '\0';
      lcd.setCursor(0, i);
      lcd.printstr(buff);
    }

}

void fetch_departures() {
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);

  //client->setFingerprint(fingerprint);
  // Or, if you happy to ignore the SSL certificate, then use the following line instead:
  client->setInsecure();

  HTTPClient https;

  Serial.print("[HTTPS] begin...\n");
  // This uses the station 'Garching' as a globalId
  if (https.begin(*client, "https://www.mvg.de/api/fib/v1/departure?globalId=de:09184:490&limit=10&offsetInMinutes=0&transportTypes=UBAHN")) {

    Serial.print("[HTTPS] GET...\n");
    // start connection and send HTTP header
    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();


        DeserializationError error = deserializeJson(json_buf, payload);

        if (error != error.Ok) {
          Serial.println("deserializeJson() failed: ");
          Serial.println(error.f_str());
        } else {
          Serial.println(payload);
        }
        
      }
    } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (fetch_timeout > 0) {
      fetch_timeout -= 1;
      step = (step + 1) % TEXT_STEP_MAX;
      choose_departures();
      print_departures(step);
    } else {
      lcd.setCursor(10, 1);  
      lcd.blink();
      fetch_departures();
      choose_departures();
      print_departures(step);
      fetch_timeout = DEFAULT_FETCH_TIMEOUT;
    }
    delay(1000);
  }
}
