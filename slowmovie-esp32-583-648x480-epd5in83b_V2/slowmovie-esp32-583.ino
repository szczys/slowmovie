/*
 * slowmovie 
 * 
 * 
 * MIT License
 * 
 * Copyright (c) 2020 Mike Szczys
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

#include <SPI.h>
#include "epd5in83b_V2.h"
Epd epd;

#include <WiFi.h>
/* Enter your WiFi access point ssid and password in this headerfile
 * format: 
 *      const char* ssid = "ssid";
 *      const char* password = "password";
 */
#include "wifi_credentials.h"

const uint8_t header[] = {0x36,0x34,0x38,0x20,0x34,0x38,0x30,0x0a}; //Header for 648x480: "648 480[Linefeed]"

#include <PubSubClient.h>
const char* mqtt_server = "192.168.1.135";
const char* mqtt_topic = "slowmovie/frame583";
/****
 * IMPORTANT: Packet size must be changed inside the PubSubClient library files
 * edit: ~/Arduino/libraries/PubSubClient/src/PubSubClient.h
 * change: #define MQTT_MAX_PACKET_SIZE 12000 
 */

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[39200];
int value = 0;


//Prototypes
bool headers_match(uint8_t *slice);
void shift_header_buf_left(uint8_t *slice);

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup");

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  if (epd.Init() != 0) {
    Serial.print("e-Paper init failed");
    return;
  }

//  Serial.print("e-Paper Clear\r\n ");
//  epd.Clear();

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

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  /*
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  */
  Serial.println();

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == mqtt_topic) {
    Serial.print("Received a message of length: ");
    Serial.println(length);

    uint8_t header_buf[] = {0,0,0,0,0,0,0,0};

    uint8_t data_offset = 0;
    for (uint8_t i=0; i<200; i++) {
      //If we don't find header in the first 200 chars, assume something went wrong
      header_buf[7] = message[i];
      if (message[i] == 0x0A) {
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
      epd.DisplayFrame(message+data_offset, 1);
      Serial.println("Done writing new frame");
    }
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
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
