/*
 * slowmovie
 *
 * variant for 7.5" Epaper (640x384  GDEW075Z09)
 *
 * MIT License
 *
 * Copyright (c) 2022 Mike Szczys
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
 *   PubSubClient
 */

 /* Enter your WiFi access point ssid and password in this headerfile
 * format: 
 *      const char* ssid = "ssid";
 *      const char* password = "password";
 */
#include "wifi_credentials.h"

#define DEBUG        1
#define MSG_BUF_SIZE 40000

/* MQTT Server for triggering a frame update */
const char* mqtt_server = "192.168.1.135";
const char* mqtt_topic = "slowmovie/frame";

/* PBM image file download for acquiring a new frame */
int    HTTP_PORT   = 80;
String HTTP_METHOD = "GET"; // or "POST"
char   HOST_NAME[] = "192.168.1.105"; // hostname of web server:
String PATH_NAME   = "/download/frame-640x384.pbm";

/* The PBM-formated header string that indicates the start of binary image data */
const uint8_t header[] = {0x36,0x34,0x30,0x20,0x33,0x38,0x34,0x0a}; //Header for 640x384: "640 384[Linefeed]"

/* Image to display at startup */
#include "mahler_black.h"
#include "mahler_red.h"

#include <SPI.h>

#define ENABLE_GxEPD2_GFX 0
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#define GxEPD2_DISPLAY_CLASS GxEPD2_3C
#define GxEPD2_DRIVER_CLASS GxEPD2_750c

// somehow there should be an easier way to do this
#define GxEPD2_BW_IS_GxEPD2_BW true
#define GxEPD2_3C_IS_GxEPD2_3C true
#define GxEPD2_7C_IS_GxEPD2_7C true
#define GxEPD2_1248_IS_GxEPD2_1248 true
#define IS_GxEPD(c, x) (c##x)
#define IS_GxEPD2_BW(x) IS_GxEPD(GxEPD2_BW_IS_, x)
#define IS_GxEPD2_3C(x) IS_GxEPD(GxEPD2_3C_IS_, x)
#define IS_GxEPD2_7C(x) IS_GxEPD(GxEPD2_7C_IS_, x)
#define IS_GxEPD2_1248(x) IS_GxEPD(GxEPD2_1248_IS_, x)

#if defined(ESP32)
#define MAX_DISPLAY_BUFFER_SIZE 65536ul // e.g.
#if IS_GxEPD2_BW(GxEPD2_DISPLAY_CLASS)
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))
#elif IS_GxEPD2_3C(GxEPD2_DISPLAY_CLASS)
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))
#elif IS_GxEPD2_7C(GxEPD2_DISPLAY_CLASS)
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2))
#endif
GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=*/ 15, /*DC=*/ 27, /*RST=*/ 26, /*BUSY=*/ 25));
#endif

#include <WiFi.h>
#include <MQTT.h>

WiFiClient net;
MQTTClient client;
uint8_t *framebuffer;
bool pending_download = false;

//Prototypes
bool headers_match(uint8_t *slice);
void shift_header_buf_left(uint8_t *slice);

void update_display(uint8_t *buffer_black, uint8_t *buffer_red) {
  display.setFullWindow();
  display.firstPage();
  do
  {
    //display.fillScreen(GxEPD_WHITE);
    if (buffer_black != NULL) display.drawInvertedBitmap(0, 0, buffer_black, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
    if (buffer_red != NULL) display.drawInvertedBitmap(0, 0, buffer_red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
  }
  while (display.nextPage());
  display.hibernate();
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
      update_display(framebuffer+data_offset, NULL);
      Serial.println("Done writing new frame");
    }
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

  /* Allocate framebuffer memory */
  framebuffer = (uint8_t *)malloc(MSG_BUF_SIZE);
  
  setup_wifi();
  client.begin(mqtt_server, net);
  client.onMessageAdvanced(messageReceived);

  display.init(115200); // uses standard SPI pins, e.g. SCK(18), MISO(19), MOSI(23), SS(5)
  // *** special handling for Waveshare ESP32 Driver board *** //
  // ********************************************************* //
  SPI.end(); // release standard SPI pins, e.g. SCK(18), MISO(19), MOSI(23), SS(5)
  //SPI: void begin(int8_t sck=-1, int8_t miso=-1, int8_t mosi=-1, int8_t ss=-1);
  SPI.begin(13, 12, 14, 15); // map and init SPI pins SCK(13), MISO(12), MOSI(14), SS(15)
  // *** end of special handling for Waveshare ESP32 Driver board *** //
  // **************************************************************** //

  update_display((uint8_t *)mahler_black_pbm, (uint8_t *)mahler_red_pbm);

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
  while (!client.connect("arduino", "public", "public")) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");

  client.subscribe(mqtt_topic);
  // client.unsubscribe("/hello");
}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (pending_download) {
    if (download_image()) {
      Serial.println("Successfully downloaded an image!");
      validate_and_display();
    }
    pending_download = false;
  }
}
