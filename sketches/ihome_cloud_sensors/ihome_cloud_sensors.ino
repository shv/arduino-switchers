/*

 */

#include <SPI.h>
#include <Ethernet.h>
#include "DHT.h"
#include <string.h>

#define DHTPIN 2 // номер пина, к которому подсоединен датчик
#define token ""

// Инициируем датчик

DHT dht(DHTPIN, DHT22);

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

unsigned long lastConnectionTime = 0;             // last time you connected to the server, in milliseconds
const unsigned long postingInterval = 1L * 1000L; // delay between updates, in milliseconds
// the "L" is needed to use long type numbers

int i;

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

String readString; // Прочитанная из body строка
String currentString; // Часть строки, отвечающая только за один девайс
int newLines = 0; // Счетчик для поиска разделения заголовка от body
int pinsid; // Pin и Sid без разделения
int on; // Флаг включения лампы
String level; // Уровень яркости для лампы
String value; // Значение сенаора
int index; // позиция вхождения подтекста в текст

void apply_pin_values() {
  for (i = 0; i < PowerPinCount; i++) {
    // Применяем изменения состояния пинов
    // false - выключен
    // true - включен
    digitalWrite(PowerPins[i], PowerPinStatuses[i]);
  }
}

void setup() {
  // start serial port:
//  Serial.begin(9600);

  dht.begin();

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

//  while (!Serial) {
//    ; // wait for serial port to connect. Needed for native USB port only
//  }

  // give the ethernet module time to boot up:
  delay(1000);
  // start the Ethernet connection using a fixed IP address and DNS server:
  Ethernet.begin(mac, ip);
  // print the Ethernet board/shield's IP address:
//  Serial.print("My IP address: ");
//  Serial.println(Ethernet.localIP());
}

void apply_string() {
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

//    Serial.print("pinsid: ");
//    Serial.print(pinsid);
//    Serial.print(", on: ");
//    Serial.print(on);
//    Serial.print(", level: ");
//    Serial.print(level);
//    Serial.print(", value: ");
//    Serial.println(value);

  }

  apply_pin_values();

  newLines = 0;
  readString = "";
}

void loop() {
  if (client.available()) {
    char c = client.read();
//    Serial.write(c);
    if (newLines > 3) {
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
            httpRequest();
          }
        }
      }
    }

    // if ten seconds have passed since your last connection,
    // then connect again and send data:
    if (millis() - lastConnectionTime > postingInterval) {
      httpRequest();
    }
  }
}

// this method makes a HTTP connection to the server:
void httpRequest() {
  // close any connection before send a new request.
  // This will free the socket on the WiFi shield
  client.stop();

  //Считываем влажность
  float h = dht.readHumidity();

  // Считываем температуру
  float t = dht.readTemperature();

  // Проверка удачно прошло ли считывание.
  if (isnan(h) || isnan(t)) {
//    Serial.println("Не удается считать показания");
    return;
  }

//  Serial.print("Влажность: ");
//  Serial.print(h);
//  Serial.print(" %\t");
//  Serial.print("Температура: ");
//  Serial.print(t);
//  Serial.println(" *C ");

  // if there's a successful connection:
  if (client.connect(server, 80)) {
//    Serial.println("connecting...");
    client.print("GET /api/v0.1/communicate/");
    client.print(token);
    client.print("?data=");
    client.print(DHTPIN);
    client.print("!1:::");
    client.print(t);
    client.print(",");
    client.print(DHTPIN);
    client.print("!2:::");
    client.print(h);
    // Лампы нужно синкать
//    for (i = 0; i < PowerPinCount; i++) {
//      client.print(",");
//      client.print(PowerPins[i]);
//      client.print(":");
//      client.print(PowerPinStatuses[i]?1:0);
//      client.print("::");
//    }

    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(server);

    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println();

    // note the time that the connection was made:
    lastConnectionTime = millis();
  } else {
    // if you couldn't make a connection:
//    Serial.println("connection failed");
  }
}
