
#include <Arduino.h>

#include <SPI.h>
#include "epd5in79g.h"
// #include "imagedata.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include "user_interface.h"

#include <ESP8266HTTPClient.h>

ESP8266WiFiMulti WiFiMulti;
Epd epd;
IPAddress myIP;

uint8_t buff[26928] = { 0 };

void setup() {

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  Serial.print("Wi-fi init  \r\n");

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(WIFI_SSID, WIFI_PASS);
  // Wait for connection
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("\r\nIP address: ");
  Serial.println(myIP = WiFi.localIP()); //192.168.1.197
}

void fpm_wakup_cb_func(void) {
  Serial.println("Light sleep is over, either because timeout or external interrupt");
  Serial.flush();
}

void loop() {
  if ((WiFiMulti.run() != WL_CONNECTED)) {
    delay(500);
    return;
  }


  Serial.print("e-Paper init  \r\n");
  if (epd.Init() != 0) {
    Serial.print("e-Paper init failed");
    return;
  }

  //  epd.Display(Image4color);
  //  delay(60000);
  //  return;
  // wait for WiFi connection
  WiFiClient client;
  client.setTimeout(30000);
  HTTPClient http;  // must be declared after WiFiClient for correct destruction order, because used by http.begin(client,...)
  HTTPClient http2;
  Serial.print("[HTTP1] begin...\n");

  // configure server and url
  // http.begin(client, "http://jigsaw.w3.org/HTTP/connection.html");
  http.begin(client, HTTP_HOST, 5000, "/raw_image_bytes/1");
  Serial.print("[HTTP1] GET...\n");
  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("[HTTP1] GET... code: %d\n", httpCode);
    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      // get length of document (is -1 when Server sends no Content-Length header)
      int len = http.getSize();
      Serial.printf("[HTTP1] GET... len: %d\n", len);

      WiFiClient stream = http.getStream();
      int c = stream.readBytes(buff, std::min((size_t)len, sizeof(buff)));
      if (!c) {
        Serial.println("read timeout");
      }
      Serial.printf("Read %d bytes\n", c);
      epd.DisplayH(0x01, buff);

      Serial.println();
      Serial.print("[HTTP1] connection closed or file end.\n");
    }
  } else {
    Serial.printf("[HTTP21] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }



  //    delay(2000);
  http.end();
  delay(10000);

  Serial.print("[HTTP] begin...\n");

  // configure server and url
  // http.begin(client, "http://jigsaw.w3.org/HTTP/connection.html");
  http2.begin(client, HTTP_HOST, 5000, "/raw_image_bytes/0");
  Serial.print("[HTTP] GET...\n");

  const char * headerKeys[] = {"time_until_next_hour_ms"};
  http2.collectHeaders(headerKeys, 1);

  httpCode = http2.GET();
  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      // get length of document (is -1 when Server sends no Content-Length header)
      int len = http2.getSize();
      Serial.printf("[HTTP] GET... len: %d\n", len);

      WiFiClient stream = http2.getStream();
      int c = stream.readBytes(buff, std::min((size_t)len, sizeof(buff)));
      if (!c) {
        Serial.println("read timeout");
      }
      Serial.printf("Read %d bytes\n", c);
      epd.DisplayH(0x02, buff);

      Serial.println();
      Serial.print("[HTTP] connection closed or file end.\n");
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http2.errorToString(httpCode).c_str());
  }
  String time_until_next_hour_ms_s = http2.header("time_until_next_hour_ms");
  long time_until_next_hour_ms = strtol(time_until_next_hour_ms_s.c_str(), NULL, 10);
  Serial.println(time_until_next_hour_ms_s);
  http2.end();
  delay(10000);

  epd.TurnOnDisplay();
  delay(10000);
  epd.Sleep();
  delay(1000);
//  ESP.deepSleep(50e6);
  
//  delay(600000);

  WiFi.disconnect(); 
  WiFi.mode(WIFI_OFF);
  delay(3000);



  long sleepTimeMilliSeconds = 240e3; // 240 seconds = 4 minutes
  //0xFFFFFFE = 2^28-1 = 268435454 microseconds (~4 1/2 minutes)
  long time_to_go = time_until_next_hour_ms;

  while(time_to_go > sleepTimeMilliSeconds) {
    extern os_timer_t *timer_list;
    timer_list = nullptr;
    wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
    wifi_fpm_open();
    // light sleep function requires microseconds
    wifi_fpm_do_sleep(sleepTimeMilliSeconds * 1000);

    // timed light sleep is only entered when the sleep command is
    // followed by a delay() that is at least 1ms longer than the sleep
    delay(sleepTimeMilliSeconds + 1);
    time_to_go -= sleepTimeMilliSeconds;
  }

  delay(time_to_go); // remaining time
  delay(60000); // for good measure
  ESP.restart(); // todo: this might break because the initial boot requires reset to be pressed. Let's see..
  delay(3000);
  // delay(time_until_next_hour_ms);
}
