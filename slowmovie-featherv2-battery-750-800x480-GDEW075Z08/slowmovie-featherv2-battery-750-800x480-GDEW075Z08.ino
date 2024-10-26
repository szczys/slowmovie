/*
 * slowmovie
 *
 * variant for 7.5" Epaper (800x480  GDEW075Z08)
 *
 * MIT License
 *
 * Copyright (c) 2024 Mike Szczys
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * Library dependancies:
 *   GxEPD2
 */

 /* Enter your WiFi access point ssid and password in this headerfile
 * format: 
 *      const char* ssid = "ssid";
 *      const char* password = "password";
 */
#include "wifi_credentials.h"
#include "elf.h"
#include "esp_mac.h"

#define DEBUG        1
#define MSG_BUF_SIZE 50000

#define NEOPIXEL_I2C_POWER 2
#define VBATPIN A13

/* MQTT Server for triggering a frame update */
const char* mqtt_server = "192.168.1.135";
const char* mqtt_topic = "slowmovie/frame";
const char* vbat_topic = "vbat";
static bool vbat_sent = false;

#define DEVICE_NAME_MAX_LEN 32
char device_name[DEVICE_NAME_MAX_LEN];

/* PBM image file download for acquiring a new frame */
int    HTTP_PORT   = 80;
String HTTP_METHOD = "GET"; // or "POST"
char   HOST_NAME[] = "192.168.1.105"; // hostname of web server:
String PATH_NAME   = "/download/frame-800x480.pbm";

/* The PBM-formated header string that indicates the start of binary image data */
const uint8_t header[] = {0x38,0x30,0x30,0x20,0x34,0x38,0x30,0x0a}; //Header for 800x480: "800 480[Linefeed]"

#include <SPI.h>

#define ENABLE_GxEPD2_GFX 0
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>

/* Pinout
 * Epaper | ESP32
 * -------|-----------------
 * 3v3    | 3v3
 * Gnd    | Gnd
 * DC     | 0     15
 * Busy   | 15    32
 * MOSI   | 23    19
 * Clk    | 18    5
 * CS     | 5     33
 * Rst    | 2     14
 */
// GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 4> epd(GxEPD2_750c_Z08(5, 0, 2, 15)); // GDEW075Z08 800x480, GD7965
GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 4> epd(GxEPD2_750c_Z08(33, 15, 14, 32)); // GDEW075Z08 800x480, GD7965


#include <WiFi.h>
#include <MQTT.h>

WiFiClient net;
MQTTClient client;
uint8_t *framebuffer;
bool pending_download = false;

//Prototypes
bool headers_match(uint8_t *slice);
void shift_header_buf_left(uint8_t *slice);

struct coordinates
{
  uint16_t x;
  uint16_t y;
};

void update_display(uint8_t *buffer_black, bool inverted) {
  epd.firstPage();
  do
  {
    if (inverted) {
      epd.drawInvertedBitmap(0, 0, buffer_black, epd.epd2.WIDTH, epd.epd2.HEIGHT, GxEPD_BLACK);
    } else {
      epd.drawBitmap(0, 0, buffer_black, epd.epd2.WIDTH, epd.epd2.HEIGHT, GxEPD_BLACK);
    }
  }
  while (epd.nextPage());

  epd.hibernate();
}

bool download_image() {
  Serial.println("\nDownloading PBM file...");
  if (net.connect(HOST_NAME, 80)) {
    Serial.println("connected");
    net.print(String("GET ") + PATH_NAME + " HTTP/1.1\r\n" +
              "Host: " + HOST_NAME + "\r\n" + 
              "Content-Type: application/octet-stream\r\n" +
              "Connection: close\r\n\r\n");
  }

  // wait for data to be available
  unsigned long timeout = millis();
  while (net.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      net.stop();
      delay(60000);
      return false;
    }
  }

  /* TODO: Check return code to make sure file exists */

  uint16_t idx = 0;
  while(net.available())
  {
    if (idx < MSG_BUF_SIZE) {
      framebuffer[idx] = net.read();
    }
    else {
      Serial.println("Error: framebuffer full, aborting.");
      return false;
    }
    idx++;
    
  }

  if (DEBUG) {
    Serial.println(idx);
    Serial.println();

    uint16_t print_idx = 0;
    for (uint8_t linect = 0; linect < 20; linect++) {
      for (uint8_t colct = 0; colct < 16; colct++) {
        Serial.print(framebuffer[print_idx++],HEX);
        Serial.print(" ");
      }
      Serial.println(framebuffer[print_idx++],HEX);
    }
    Serial.println();
    Serial.print("Heap available: ");
    Serial.println(ESP.getFreeHeap());
  }

  return true;
}

void validate_and_display() {
    uint8_t header_buf[] = {0,0,0,0,0,0,0,0};

    uint16_t data_offset = 0;
    for (uint16_t i=0; i<600; i++) {
      //If we don't find header in the first 600 chars, assume something went wrong
      header_buf[7] = framebuffer[i];
      if (framebuffer[i] == 0x0A) {
        //Line Feed; this is what ends a valid header
        if (headers_match(header_buf) == 1) {
          data_offset = i+1;
          Serial.print("Header found! Data begins at: ");
          Serial.println(data_offset);
          break;
        }
      }
      //Shift header_checker left
      shift_header_buf_left(header_buf);
    }

    if (data_offset == 0) {
      Serial.println("Correct header not found. This message is not a properly formatted image");
    }

    else {
      Serial.println("Writing new frame to display");
      if (DEBUG) {
        Serial.print("Offset: "); Serial.println(data_offset);
        Serial.print("Pointer: "); Serial.println(*framebuffer, HEX);
        Serial.print("Pointer Math: "); Serial.println(*framebuffer+data_offset, HEX);
      }
      update_display(framebuffer+data_offset, true);
      Serial.println("Done writing new frame");
    }
}

void set_device_name(void)
{
  unsigned char mac_base[6] = {0};
  esp_efuse_mac_get_default(mac_base);

  snprintf(device_name, DEVICE_NAME_MAX_LEN,
      "slowmovie-750-%02X:%02X:%02X:%02X:%02X:%02X", mac_base[0], mac_base[1],
      mac_base[2], mac_base[3], mac_base[4], mac_base[5]);

  Serial.println(device_name);
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup");

  if (DEBUG) {
    Serial.print("Heap available: ");
    Serial.println(ESP.getFreeHeap());
  }

  set_device_name();

  // Cut power to ws2812
  pinMode(NEOPIXEL_I2C_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_I2C_POWER, LOW);

  /* Allocate framebuffer memory */
  framebuffer = (uint8_t *)ps_malloc(MSG_BUF_SIZE);
  if (!framebuffer)
  {
    Serial.println("Error: Unable to allocate frame buffer");
  }
 
  setup_wifi();
  client.begin(mqtt_server, net);
  client.onMessageAdvanced(messageReceived);

  epd.init(115200, true, 50, false);

  //update_display((uint8_t *) elf, false);

  Serial.println("setup done");
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void messageReceived(MQTTClient *client, char topic[], char message[], int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.println();

  if (String(topic) == mqtt_topic) {
    Serial.println("MQTT indicated a new frame is available");
    pending_download = true;
  }
}

bool headers_match(uint8_t *slice) {
  for (uint8_t i=0; i<8; i++) {
    if (header[i] != slice[i]) {
      return 0; //members don't match
    }
  }
  return 1;
}

void shift_header_buf_left(uint8_t *slice) {
  for (uint8_t i=1; i<8; i++) {
    slice[i-1] = slice[i];
  }
  slice[7] == 0;
}

void reconnect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\nconnecting...");
  while (!client.connect(device_name)) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");

  //client.subscribe(mqtt_topic);
  // client.unsubscribe("/hello");
}

void onConnectionEstablished()
{
  float measuredvbat = analogReadMilliVolts(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat /= 1000; // convert to volts!
  Serial.print("VBat: " ); Serial.println(measuredvbat);

  char voltage[32];
  snprintf(voltage, sizeof(voltage), "%f", measuredvbat);
  client.publish(vbat_topic, voltage);
  delay(2000);
  vbat_sent = true;
}

void loop()
{
  client.loop();

  if (!client.connected()) {
    //Fixme: add timeout and sleep so we don't run down battery awaiting connection.
    reconnect();
  }

  if (download_image()) {
    Serial.println("Successfully downloaded an image!");
    validate_and_display();
  }

  esp_sleep_enable_timer_wakeup(300000000); // 300 sec
  esp_deep_sleep_start();

}
