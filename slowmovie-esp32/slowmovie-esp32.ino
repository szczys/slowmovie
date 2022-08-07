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
 *   GxEPD
 *   PubSubClient
 */

#include <WiFi.h>
/* Enter your WiFi access point ssid and password in this headerfile
 * format: 
 *      const char* ssid = "ssid";
 *      const char* password = "password";
 */
#include "wifi_credentials.h"


#include <PubSubClient.h>
const char* mqtt_server = "192.168.1.135";
/****
 * IMPORTANT: Packet size must be changed inside the PubSubClient library files
 * edit: ~/Arduino/libraries/PubSubClient/src/PubSubClient.h
 * change: #define MQTT_MAX_PACKET_SIZE 40000
 */

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[12000];
uint8_t displayFrame[5808];
int value = 0;

#include "DEV_Config.h"
#include "EPD.h"
#include "ImageData.h"

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup");

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  printf("EPD_5IN83B_V2_test Demo\r\n");
  DEV_Module_Init();

  printf("e-Paper Init and Clear...\r\n");
  EPD_5IN83B_V2_Init();
//  Serial.println("Fill red");
//  EPD_5IN83B_V2_Clear();
//  EPD_5IN83B_V2_Display(BLANK,FILLED);
//  delay(2000);
//  Serial.println("Fill black");
//  EPD_5IN83B_V2_Clear();
//  EPD_5IN83B_V2_Display(FILLED, BLANK);
//  delay(2000);
//  Serial.println("Clear screen");
  EPD_5IN83B_V2_Clear();
  //EPD_5IN83B_V2_Sleep();

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

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "slowmovie/frame") {
    Serial.println("Received a message");
    Serial.println(length);

    if (length >= 38880) {
      uint16_t data_idx = length-38880;
      Serial.print("Data starts at index ");
      Serial.println(data_idx);

      EPD_5IN83B_V2_Init();
      //EPD_5IN83B_V2_Clear();
      EPD_5IN83B_V2_Display(message+data_idx, BLANK);
      EPD_5IN83B_V2_Sleep();
      
    }
    else {
      Serial.println("ERROR: Expected message length to be >= 38880");
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("slowmovie/frame");
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
