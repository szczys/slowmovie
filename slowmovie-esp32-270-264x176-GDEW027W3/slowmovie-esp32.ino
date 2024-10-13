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
 * change: #define MQTT_MAX_PACKET_SIZE 12000 
 */

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[12000];
uint8_t displayFrame[5808];
int value = 0;

// include library, include base class, make path known
#include <GxEPD.h>
#include <GxGDEW027W3/GxGDEW027W3.h>      // 2.7" b/w
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

//ESP32 pin definitions
GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16); // arbitrary selection of 17, 16
GxEPD_Class display(io, /*RST=*/ 16, /*BUSY=*/ 4); // arbitrary selection of (16), 4

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup");

  display.init(115200); // enable diagnostic output on Serial

  for (uint16_t i=0; i<5808; i++) { displayFrame[i] = 0; }

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

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

    for (uint16_t i=0; i<5808; i++) {
      uint8_t hNibble = (char)message[i*2];
      uint8_t lNibble = (char)message[(i*2)+1];
      uint8_t storageByte = 0;
      if (hNibble < 65) { storageByte += (hNibble-48)<<4; }
      else { storageByte += (hNibble-55)<<4; }
      if (lNibble < 65) { storageByte += lNibble-48; }
      else { storageByte += lNibble-55; }
      if (i%100 == 0) { Serial.println(i); }
      displayFrame[i] = storageByte;
    }
    display.drawExampleBitmap(displayFrame, sizeof(displayFrame));
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
