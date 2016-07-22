/*

 */

#include <SPI.h>
#include <Ethernet.h>
#include "DHT.h"
#include <string.h>
#include <Wire.h>
#include <BMP085.h>


#define DHTPIN 2 // номер пина, к которому подсоединен датчик
#define token ""

// Инициируем датчик

DHT dht(DHTPIN, DHT22);

BMP085 dps = BMP085();

// assign a MAC address for the ethernet controller.
// fill in your address here:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
// fill in an available IP address on your network here,
// for manual configuration:
IPAddress ip(192, 168, 1, 177);

// initialize the library instance:
EthernetClient client;

char server[] = "wall.electro-control-center.ru";

unsigned long lastConnectionGetLampStatusIntervalTime = 0; // Время последнего сбора данных о лампах
const unsigned long getLampStatusInterval = 1L * 1000L; // delay between gets, in milliseconds
unsigned long lastConnectionPostSensorsIntervalTime = 0; // Время последней отправки данных с датчиков
const unsigned long postSensorsInterval = 10L * 1000L; // delay between updates, in milliseconds
// the "L" is needed to use long type numbers

// Список сущностей, которые можно включать и выключать
const int PowerPins[] = {3, 4};

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

// Фотодатчики
const int photoSensors[] = {A3};
const int photoCount = sizeof(photoSensors) / sizeof(int);;

int i; // Итератор
String readString; // Прочитанная из body строка
String currentString; // Часть строки, отвечающая только за один девайс
int newLines = 0; // Счетчик для поиска разделения заголовка от body
int pinsid; // Pin и Sid без разделения
int on; // Флаг включения лампы
String level; // Уровень яркости для лампы
String value; // Значение сенаора
int index; // позиция вхождения подтекста в текст
float t; // Температура с DHT-22
float h; // Влажность с DHT-22
long Temperature = 0, Pressure = 0; // BMP-85

void apply_pin_values() {
  for (i = 0; i < PowerPinCount; i++) {
    // Применяем изменения состояния пинов
    // false - выключен
    // true - включен
    digitalWrite(PowerPins[i], PowerPinStatuses[i]);
  }
}

void setup() {
//  Serial.begin(9600);

  dht.begin();
  Wire.begin();

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

  // give the ethernet module time to boot up:
  delay(1000);
  // Для инициализации dsp и Ethernet нужна задержка

  dps.init();

  // start the Ethernet connection using a fixed IP address and DNS server:
  Ethernet.begin(mac, ip);
  // print the Ethernet board/shield's IP address:
//  Serial.print("My IP address: ");
//  Serial.println(Ethernet.localIP());
}

void apply_string() {
  // Парсим строку с информацией
//  Serial.println(readString);
  //Find lamps and sensors  
  index = readString.indexOf(",");
  if (index < 0) index = readString.length();

//  Serial.println(index);
  while (index > 0) {
    currentString = readString.substring(0, index);
    readString = readString.substring(index+1);
//    Serial.println(readString);
    index = currentString.indexOf(":");
    if (index > 0) {
      pinsid = currentString.substring(0, index).toInt();
      currentString = currentString.substring(index+1);
      index = currentString.indexOf(":");
      if (index > 0) {
        on = currentString.substring(0, index).toInt();
        currentString = currentString.substring(index+1);
        index = currentString.indexOf(":");
        if (index > 0) {
          level = currentString.substring(0, index);
          value = currentString.substring(index+1);      
        }
      }
    }
    for (i = 0; i < PowerPinCount; i++) {
      if (PowerPins[i] == pinsid) {
        PowerPinStatuses[i] = on;
      }
    }
    index = readString.indexOf(",");
    if (index < 0) index = readString.length();
  }

  apply_pin_values();
  newLines = 0;
  readString = "";
}

void loop() {
  if (client.available()) {
    char c = client.read();
//    Serial.write(c);
    if (newLines > 3 && newLines < 5) {
      if (c == '\n' || c == '\r') {
          // End of body
//        Serial.println("End body");
        apply_string();
      } else {
        readString += c;
      }
    } else {
      if (c == '\n' || c == '\r') {
        newLines++;
      } else {
        newLines = 0;
      }
    }
  } else {
    if (readString != "") {
      // Считаем, что запрос окончен
      // Работает только с непустым ответом (post отработает некорректно)
//        Serial.println("End request");
      apply_string();
    }

    // Тут нужно проверять местные выключатели и только после этого отправлять данные
    for(i = 0; i < ControlPinCount; i++) {
      ControlValue = digitalRead(ControlPins[i]);
      if (ControlValue != PermanetnControlValues[i]) {
        delay(10);
        ControlValue = digitalRead(ControlPins[i]);
        if (ControlValue != PermanetnControlValues[i]) {
          PermanetnControlValues[i] = ControlValue;
          if (ControlValue) {
            // Меняем состояние
            PowerPinStatuses[ControlPowerPinIds[i]] = !PowerPinStatuses[ControlPowerPinIds[i]];
            // Записываем новый статус
            apply_pin_values();
            // Информируем сервер
            httpPostLampRequest();
          }
        }
      }
    }

    // Два запроса с разными интервалами
    if (millis() - lastConnectionPostSensorsIntervalTime > postSensorsInterval) {
      httpPostSensorsRequest();
    } else if (millis() - lastConnectionGetLampStatusIntervalTime > getLampStatusInterval) {
      httpGetLampRequest();
    }
  }
}

void httpPostSensorsRequest() {
  // отправляем информацию о состоянии датчиков
  client.stop();

  //Считываем влажность
  h = dht.readHumidity();
  // Считываем температуру
  t = dht.readTemperature();
  // Считываем давление
  dps.getPressure(&Pressure);
  // Считываем температуру
  dps.getTemperature(&Temperature);

  if (client.connect(server, 80)) {
    client.print("GET /api/v0.1/communicate/");
    client.print(token);
    client.print("?data=");
    // Датчик влажности и температуры
    client.print(DHTPIN);
    client.print("!1:::");
    if (!isnan(t)) {
      client.print(t);
    }
    client.print(",");
    client.print(DHTPIN);
    client.print("!2:::");
    if (!isnan(h)) {
      client.print(h);
    }
    // Вывод информации с аналоговых датчиков (освещенность)
    for(i = 0; i < photoCount; i++) {
      client.print(",");
      client.print(photoSensors[i]);
      client.print(":::");
      client.print(analogRead(photoSensors[i]));
    }
    // Датчик давления и температуры
    client.print(",");
    client.print(A4);
    client.print(":::");
    client.print(Pressure/133.3);
    client.print(",");
    client.print(A5);
    client.print(":::");
    client.print(Temperature*0.1);

    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(server);

    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println();

    // Отмечаем время отправки данных датчиков и время получения инфы о лампах
    lastConnectionPostSensorsIntervalTime = millis();
    lastConnectionGetLampStatusIntervalTime = millis();
  } else {
//    Serial.println("connection failed");
  }
}

// Отправляем новое состояние ламп, если их переключили
void httpPostLampRequest() {
  // Отправка инфы о статусе всех ламп
  // Если произошел какой касяк достаточно дернуть одну лампу и все синхронизируется
  client.stop();

  if (client.connect(server, 80)) {
    client.print("GET /api/v0.1/communicate/");
    client.print(token);
    client.print("?data=");
    // Отправляем инфу о статусе ламп
    for (i = 0; i < PowerPinCount; i++) {
      if (i > 0) client.print(",");
      client.print(PowerPins[i]);
      client.print(":");
      client.print(PowerPinStatuses[i]?1:0);
      client.print("::");
    }
    // Вывод информации с аналоговых датчиков (освещенность)
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(server);

    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println();

    // Отмечаем время получения статуса ламп
    lastConnectionGetLampStatusIntervalTime = millis();
  } else {
//    Serial.println("connection failed");
  }
}

// Забираем состояние ламп с сервера
void httpGetLampRequest() {
  // Простые запросы на получениестатуса ламп
  client.stop();

  if (client.connect(server, 80)) {
//    Serial.println("connecting...");
    client.print("GET /api/v0.1/get/");
    client.print(token);

    // Вывод информации с аналоговых датчиков (освещенность)
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(server);

    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println();

    // Отмечаем время получения стутуса ламп
    lastConnectionGetLampStatusIntervalTime = millis();
  } else {
//    Serial.println("connection failed");
  }
}
