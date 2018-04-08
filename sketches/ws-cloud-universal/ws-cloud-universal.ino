// https://github.com/brandenhall/Arduino-Websocket

#include <Ethernet.h>
#include <EthernetClient.h>
#include <SPI.h>
#include "DHT.h"
#include <string.h>
#include <Wire.h>
#include <BMP085.h>

//#define DEBUGGING 1

// Here we define a maximum framelength to 64 bytes. Default is 256.
#define MAX_FRAME_LENGTH 64

// Define how many callback functions you have. Default is 1.
#define CALLBACK_FUNCTIONS 1

#include <WebSocketClient.h>

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
// fill in an available IP address on your network here,
// for manual configuration:
IPAddress ip(192, 168, 1, 177);

#define DHTPIN 2 // номер пина, к которому подсоединен датчик

void(* resetFunc) (void) = 0; // Reset MC function

// Инициируем датчик

DHT dht(DHTPIN, DHT22);

BMP085 dps = BMP085();

// initialize the library instance:
EthernetClient client;

WebSocketClient webSocketClient;

unsigned long lastPostSensorTime = 0; // Время последней отправки данных с датчиков
const unsigned long postSensorsInterval = 15L * 1000L; // delay between updates, in milliseconds

int i; // iterator
String data;
int index; // позиция вхождения подтекста в текст
String readString; // Прочитанная из body строка
String currentString; // Часть строки, отвечающая только за один девайс
String type; // Pin и Sid без разделения
int pinsid; // Pin и Sid без разделения
int on; // Флаг включения лампы
int automat; // Флаг включения лампы
String level; // Уровень яркости для лампы
String value; // Значение сенаора

// Список сущностей, которые можно включать и выключать
const int PowerPins[] = {3, 4, 8, 9};

// Количество рабочих пинов
const int PowerPinCount = sizeof(PowerPins) / sizeof(int);

// Массив для фиксации изменений.
boolean PowerPinStatuses[PowerPinCount];

const int ControlPins[] = {5};

// Количество пинов кнопок
const int ControlPinCount = sizeof(ControlPins) / sizeof(int);

// ID Power пинов, на которые действуют пины контроля
const int ControlPowerPinIds[] = {0};

// Зафиксированные значения контролов
int PermanetnControlValues[] = {0};

// Полученное значение сенсора
int ControlValue = 0;

const int PIRPin = 6;
int PIRPinValue = 0;

// Фотодатчики
const int photoSensors[] = {A3};
const int photoCount = sizeof(photoSensors) / sizeof(int);;

float t; // Температура с DHT-22
float h; // Влажность с DHT-22
long Temperature = 0, Pressure = 0; // BMP-85

boolean connectClient() {
  client.stop();
  if (client.connect("wall.electro-control-center.ru", 80)) {
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
  webSocketClient.path = "/ws/<token>";
  webSocketClient.host = "wall.electro-control-center.ru";
  
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

  // Инициализация пинов ламп
  for(i = 0; i < PowerPinCount; i++) {
      pinMode(PowerPins[i],OUTPUT);
      // false - выключен
      // true - включен
      PowerPinStatuses[i] = false;
  }
  // Инициализация пинов кнопок
  for(i = 0; i < ControlPinCount; i++) {
    // Цифровые входы
    pinMode(ControlPins[i],INPUT);
  }
  pinMode(PIRPin,INPUT);
  apply_pin_values();

  dht.begin();
  Wire.begin();

  // give the ethernet module time to boot up:
  delay(1000);
  // Для инициализации dsp и Ethernet нужна задержка

  dps.init();


  Ethernet.begin(mac, ip);
  #ifdef DEBUGGING
    Serial.print("My IP address: ");
    Serial.println(Ethernet.localIP());
  #endif

  // Connect to the websocket server
  for (i=0; i<10; i++) {
    if (connectClient()) break;
    delay(1000);
  }
}

void loop() {
  if (client.connected()) {
    uint8_t opcode[1];
//    int opcode1;
    data = "";
    webSocketClient.getData(data, opcode);


    if (data.length() > 0) {
//      Serial.print(opcode1, HEX);
//      Serial.print(": ");
//      Serial.print((char) opcode1);
//      Serial.print(": ");
//      Serial.write(*opcode);
//      Serial.print(": ");
//      Serial.println(data.length());
      #ifdef DEBUGGING
          Serial.print("Received data: ");
          Serial.println(data);
      #endif

      #ifdef DEBUGGING
          Serial.print("Opcode: ");
          Serial.println(opcode[0]);
          // Serial.println(opcode[1]);
      #endif

      // if (opcode[0] == 0x89) {
      if (opcode[0] == 0x9) {
        Serial.println("PING PONG");
        webSocketClient.sendData(data, WS_OPCODE_PONG | WS_FIN);
          data = "";
      // } else if (data == "a") {
      //   Serial.println("PING PONG a");
      //   webSocketClient.sendData(data, WS_OPCODE_PONG | WS_FIN);
      // } else if (opcode[0] == 0x88) {
      } else if (opcode[0] == 0x8) {
        Serial.println("Disconnected");
        client.stop();
      } else {
        Serial.print("New string");
        apply_string(data);
      }
    }
    // Отправляем изменение датчика движения сразу
    if (PIRPinValue != digitalRead(PIRPin)) {
      PIRPinValue = digitalRead(PIRPin);
      PowerPinStatuses[0] = PIRPinValue;
      apply_pin_values();
      data = "s:" + String(PIRPin) + ":";
      data += String(PIRPinValue?1:0);
      data += ";";
      data += "l:" + String(PowerPins[0]) + ":";
      data += String(PowerPinStatuses[0]) + ":1:";
      webSocketClient.sendData(data);
    }
    // Два запроса с разными интервалами
    if (millis() - lastPostSensorTime > postSensorsInterval) {
      #ifdef DEBUGGING
        Serial.println("Creating data");
      #endif
      collectSensorsData(data);
      #ifdef DEBUGGING
          Serial.println(data);
      #endif
      webSocketClient.sendData(data);
//      data = "";
      lastPostSensorTime = millis();
    }

  } else {
    
    #ifdef DEBUGGING
      Serial.println("Client disconnected.");
    #endif
    if (!connectClient()) {
      delay(1000);
    };
  }  
  delay(10);
}

void collectSensorsData(String& data) {
  #ifdef DEBUGGING
    Serial.print("read DHT Humidity = ");
  #endif
  //Считываем влажность
  h = dht.readHumidity();
  #ifdef DEBUGGING
    Serial.println(h);
  #endif
  #ifdef DEBUGGING
    Serial.print("read DHT Temperature = ");
  #endif
  // Считываем температуру
  t = dht.readTemperature();
  #ifdef DEBUGGING
    Serial.println(t);
  #endif
  #ifdef DEBUGGING
    Serial.print("read BMP Pressure = ");
  #endif
  // Считываем давление
  dps.getPressure(&Pressure);
  #ifdef DEBUGGING
    Serial.println(Pressure);
  #endif
  #ifdef DEBUGGING
    Serial.print("read BMP Temperature = ");
  #endif
  // Считываем температуру
  dps.getTemperature(&Temperature);
  #ifdef DEBUGGING
    Serial.println(Temperature);
  #endif
  data = "";
  #ifdef DEBUGGING
    Serial.println("Creating data: ");
  #endif
  // Датчик влажности и температуры
  data = "s:" + String(DHTPIN) + "_1:";
  if (!isnan(t)) {
    data += String(t);
  }
  #ifdef DEBUGGING
    Serial.println(data);
  #endif
  data += ";";
  data += "s:" + String(DHTPIN) + "_2:";
  if (!isnan(h)) {
    data += String(h);
  }
  #ifdef DEBUGGING
    Serial.println(data);
  #endif
  // Вывод информации с аналоговых датчиков (освещенность)
  for(i = 0; i < photoCount; i++) {
    data += ";";
    data += "s:" + String(photoSensors[i]) + ":";
    data += String(analogRead(photoSensors[i]));
  }
  #ifdef DEBUGGING
    Serial.println(data);
  #endif
  // Датчик давления и температуры
  data += ";";
  data += "s:" + String(A4) + ":";
  data += String(Pressure/133.3);
  #ifdef DEBUGGING
    Serial.println(data);
  #endif
  data += ";";
  data += "s:" + String(A5) + ":";
  data += String(Temperature*0.1);
  // Пока тут тоже отправляем состояние датчика движения
  #ifdef DEBUGGING
    Serial.println(data);
  #endif
  data += ";";
  data += "s:" + String(PIRPin) + ":";
  data += String(digitalRead(PIRPin)?1:0);
  #ifdef DEBUGGING
    Serial.println(data);
  #endif
  return;
}

void apply_pin_values() {
  for (i = 0; i < PowerPinCount; i++) {
    // Применяем изменения состояния пинов
    // false - выключен
    // true - включен
    digitalWrite(PowerPins[i], 1-PowerPinStatuses[i]);
  }
}

void apply_string(String& readString) {
  // Парсим строку с информацией
  #ifdef DEBUGGING
    Serial.println(readString);
  #endif
  //Find lamps and sensors  
  index = readString.indexOf(";");
  if (index < 0) index = readString.length();

  #ifdef DEBUGGING
    Serial.println(index);
  #endif
  while (index > 0) {
    currentString = readString.substring(0, index);
    readString = readString.substring(index+1);
    #ifdef DEBUGGING
      Serial.println(readString);
    #endif
    index = currentString.indexOf(":");
    if (index > 0) {
      type = currentString.substring(0, index);
      currentString = currentString.substring(index+1);
      index = currentString.indexOf(":");
      pinsid = currentString.substring(0, index).toInt();// только для ламп, где pin число
      currentString = currentString.substring(index+1);
      index = currentString.indexOf(":");
      if (index > 0) {
        on = currentString.substring(0, index).toInt();
        currentString = currentString.substring(index+1);
        index = currentString.indexOf(":");
        if (index > 0) {
          automat = currentString.substring(0, index).toInt();
          level = currentString.substring(index+1);      
        }
      }
    }
    for (i = 0; i < PowerPinCount; i++) {
      if (PowerPins[i] == pinsid) {
        PowerPinStatuses[i] = on;
      }
    }
    index = readString.indexOf(";");
    if (index < 0) index = readString.length();
  }
  apply_pin_values();
}
