// Send sensors data to electro control center
// Use your own token from admin interface
// In admin interface add 2 sensors
//   external_id = 2_1 (temperature)
//   external_id = 2_2 (humidity)

#include <Ethernet.h>
#include <EthernetClient.h>
#include "DHT.h"
#include <string.h>

//#define DEBUGGING 1

// Here we define a maximum framelength to 64 bytes. Default is 256.
#define MAX_FRAME_LENGTH 64

// Define how many callback functions you have. Default is 1.
#define CALLBACK_FUNCTIONS 1

#define HOST "wall.electro-control-center.ru"
#define PATH "/ws/<token>"

#include <WebSocketClient.h>

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
// fill in an available IP address on your network here,
// for manual configuration:
IPAddress ip(192, 168, 1, 177);

#define DHTPIN 2 // pin

DHT dht(DHTPIN, DHT22);

// initialize the library instance:
EthernetClient client;

WebSocketClient webSocketClient;

unsigned long lastPostSensorTime = 0; // Время последней отправки данных с датчиков
const unsigned long postSensorsInterval = 15L * 1000L; // delay between updates, in milliseconds

String data;

float t; // Температура с DHT-22
float h; // Влажность с DHT-22

boolean connectClient() {
  client.stop();
  if (client.connect(HOST, 80)) {
    #ifdef DEBUGGING
      Serial.println("Connected");
    #endif
  } else {
    #ifdef DEBUGGING
      Serial.println("Connection failed.");
    #endif
    return false;
  }

  // Handshake with the server
  webSocketClient.path = PATH;
  webSocketClient.host = HOST;
  
  if (webSocketClient.handshake(client)) {
    #ifdef DEBUGGING
      Serial.println("Handshake successful");
    #endif
  } else {
    #ifdef DEBUGGING
      Serial.println("Handshake failed.");
    #endif
    return false;
  }
  return true;
}

void setup() {
  #ifdef DEBUGGING
    Serial.begin(9600);
    Serial.println("Start");
  #endif

  dht.begin();

  // give the ethernet module time to boot up:
  delay(1000);

  Ethernet.begin(mac, ip);
  #ifdef DEBUGGING
    Serial.print("My IP address: ");
    Serial.println(Ethernet.localIP());
  #endif

  // Try reconnect. Lock all another actions
  while (!connectClient()) {
    delay(2000);
  };
}

void loop() {
  if (client.connected()) {
    data = "";
    // Try get data from websocket
    webSocketClient.getData(data);

    if (data.length() > 0) {
      #ifdef DEBUGGING
          Serial.print("Received data: ");
          Serial.println(data);
      #endif
    }
    if (millis() - lastPostSensorTime > postSensorsInterval) {
      #ifdef DEBUGGING
        Serial.println("Creating data");
      #endif
      // read data from sensor
      collectSensorsData(data);
      #ifdef DEBUGGING
          Serial.println(data);
      #endif

      // send data to websocket
      webSocketClient.sendData(data);
      lastPostSensorTime = millis();
    }
  } else {
    #ifdef DEBUGGING
      Serial.println("Client disconnected.");
    #endif
    // Try reconnect. Lock all another actions
    while (!connectClient()) {
      delay(2000);
    };
  }  
}

void collectSensorsData(String& data) {
  data = "";

  // Humidity
  h = dht.readHumidity();

  // Temperature
  t = dht.readTemperature();

  #ifdef DEBUGGING
    Serial.print("read DHT Humidity = ");
    Serial.println(h);
    Serial.print("read DHT Temperature = ");
    Serial.println(t);
    Serial.print("Creating data: ");
  #endif

  if (!isnan(t)) {
    data += "s:" + String(DHTPIN) + "_1:" + String(t);
  }
  if (!isnan(h)) {
    data += ";s:" + String(DHTPIN) + "_2:" + String(h);
  }
  #ifdef DEBUGGING
    Serial.println(data);
  #endif
}


