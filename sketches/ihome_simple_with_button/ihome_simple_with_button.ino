/*
  Полезные ссылки:
  – http://jeelabs.org/pub/docs/ethercard/classBufferFiller.html
  – http://arduino.ru/Reference
  Допущения
  – для управления нагрузкой ипользуем пины от 2 до 9
  – для физических выключателей испльзуем аналоговые пины (0 - нет реакции, 1/2 - выключение, 1 - включение)
  – запросы на управления через сеть имеют формат: GET /switch?<int pin>=<boolean status>
  – запрос на получение состояний через сеть имеют формат: GET /switch или GET /status
  Подключаем Pins "ENC28J60 Module" к Arduino Uno.
  VCC - 3.3V
  GND - GND
  SCK - Pin 13
  SO - Pin 12
  SI - Pin 11
  CS - Pin 10 Можно выбрать любой.
*/
#include <EtherCard.h>

static byte mymac[] = { 0x5A,0x5A,0x5A,0x5A,0x5A,0x5A };

static byte myip[] = { 192,168,1,222 };

byte Ethernet::buffer[500];

BufferFiller bfill;

// Общие переменные
int i;
char *data;
word len;
word pos;

// Массив задействованных номеров Pins Arduino, для управления реле.
// На моей первой ардуинке не работает пин 4
// При правке, не забыть поправить в loop() проверку запроса
const int LedPins[] = {2,3,5,6};

// Количество рабочих пинов
const int PinCount = sizeof(LedPins) / sizeof(int);

// Массив для фиксации изменений.
boolean PinStatus[PinCount];

// Кнопки на аналоговых вводах
// Для включения используем диапазон от 768 до 1023
// Для выключения используем диапазон от 256 до 767
// На значения до 255 не реагируем
// Список сенсоров
const int sensorPins[] = {A1,A2};

// Количество сенсоров
const int sensorCount = sizeof(sensorPins) / sizeof(int);

// id пинов, на которые влияют сенсоры
const int ledSensorPinIds[sensorCount] = {2,3};

// Полученное значение сенсора
int sensorValue = 0;

// Фотодатчики
const int photoSensors[] = {A5};
const int photoCount = sizeof(photoSensors) / sizeof(int);;


//-------------

const char http_OK[] PROGMEM =
"HTTP/1.0 200 OK\r\n"
"Content-Type: application/json\r\n"
"Pragma: no-cache\r\n\r\n";

const char http_404[] PROGMEM =
"HTTP/1.0 404 Not Found\r\n"
"Content-Type: application/json\r\n"
"Pragma: no-cache\r\n\r\n";

//------------

// Делаем функцию для оформления нашей Web страницы.

void statusJson() {
  bfill.emit_p(PSTR("$F["),
    http_OK
  );
  // Вывод информации с аналоговых датчиков (освещенность)
  for(i = 0; i < photoCount; i++) {
    bfill.emit_p(PSTR("{\"value\": $D, \"pin\": $D},"),
      analogRead(photoSensors[i]),
      photoSensors[i]
    );
  }
  // Вывод информации о включенных пинах
  for(i = 0; i < PinCount; i++) {
    bfill.emit_p(PSTR("{\"on\": $F, \"pin\": $D}"),
      PinStatus[i]?PSTR("false"):PSTR("true"),
      LedPins[i]
    );
    if (i+1 < PinCount && PinCount > 1) {
      bfill.emit_p(PSTR(","));
    }
  }
  bfill.emit_p(PSTR("]"));
}

void setup() {
    Serial.begin(9600);
    // По умолчанию в Библиотеке "ethercard" (CS-pin) = № 8.
    // if (ether.begin(sizeof Ethernet::buffer, mymac) == 0).
    // and change it to: Меняем (CS-pin) на 10.
    // if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0).
    if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0);
    // DHCP слишком медленный и в случае отсутствия сети все тормозит
    // if (!ether.dhcpSetup());
    // Выводим в Serial монитор IP адрес который нам автоматический присвоил наш Router.
    // Динамический IP адрес, это не удобно, периодический наш IP адрес будет меняться.
    // Нам придётся каждый раз узнавать кой адрес у нашей страницы.
    ether.printIp("My Router IP: ", ether.myip); // Выводим в Serial монитор IP адрес который нам присвоил Router.
    // Здесь мы подменяем наш динамический IP на статический / постоянный IP Address нашей Web страницы.
    // Теперь не важно какой IP адрес присвоит нам Router, автоматический будем менять его, например на "192.168.1.222".
    ether.staticSetup(myip);
    ether.printIp("My SET IP: ", ether.myip); // Выводим в Serial монитор статический IP адрес.
    //-----
    // Инициализация пинов ламп
    for(i = 0; i < PinCount; i++) {
        pinMode(LedPins[i],OUTPUT);
        PinStatus[i] = true;
        digitalWrite(LedPins[i], PinStatus[i]);
    }
}

void loop() {
  delay(5); // Дёргаем микроконтроллер.

  for(i = 0; i < sensorCount; i++) {
    sensorValue = analogRead(sensorPins[i]);
    if (sensorValue > 255) {
      // Перепроверяем для сглаживания шума
      delay(10);
      sensorValue = analogRead(sensorPins[i]);
      if (sensorValue > 767) {
        // On
        PinStatus[ledSensorPinIds[i]] = false;
      } else if (sensorValue > 255) {
        // Off
        PinStatus[ledSensorPinIds[i]] = true;
      }
      digitalWrite(LedPins[ledSensorPinIds[i]], PinStatus[ledSensorPinIds[i]]);
    }
  }


  len = ether.packetReceive(); // check for ethernet packet / проверить ethernet пакеты.
  pos = ether.packetLoop(len); // check for tcp packet / проверить TCP пакеты.
  if (pos) {
    bfill = ether.tcpOffset();
    data = (char *) Ethernet::buffer + pos;
    if (strncmp("GET /status ", data, 12) == 0) {
      statusJson();
    } else if (strncmp("GET /switch", data, 11) == 0) {
      data += 11;
      if (strncmp(data, " ", 1) != 0 ) {
        if (strncmp(data, "?2=true ", 8) == 0 ) {
            PinStatus[0] = false;
        }
        if (strncmp(data, "?2=false ", 9) == 0 ) {
            PinStatus[0] = true;
        }
        if (strncmp(data, "?3=true ", 8) == 0 ) {
            PinStatus[1] = false;
        }
        if (strncmp(data, "?3=false ", 9) == 0 ) {
            PinStatus[1] = true;
        }
        if (strncmp(data, "?5=true ", 8) == 0 ) {
            PinStatus[2] = false;
        }
        if (strncmp(data, "?5=false ", 9) == 0 ) {
            PinStatus[2] = true;
        }
        if (strncmp(data, "?6=true ", 8) == 0 ) {
            PinStatus[3] = false;
        }
        if (strncmp(data, "?6=false ", 9) == 0 ) {
            PinStatus[3] = true;
        }
        for (i = 0; i < PinCount; i++) {
          // Применяем изменения состояния пинов
          digitalWrite(LedPins[i], PinStatus[i]);
        }
      }
      statusJson();
    } else {
      bfill.emit_p(http_404);
    }
    ether.httpServerReply(bfill.position()); // send http response
  }
}
