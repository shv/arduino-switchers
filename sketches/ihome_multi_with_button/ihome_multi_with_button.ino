/*
  Полезные ссылки:
  – http://jeelabs.org/pub/docs/ethercard/classBufferFiller.html
  – http://arduino.ru/Reference
  Назначение
  – дискретное включение / выключение нагрузки в количестве 4-х штук по сети
  – дискретное включение / выключение нагрузки в количестве 4-х штук с помощью 4-х 3-х позиционных кнопок (0, 0.5, 1)
  – сбор данных с аналоговых входов
  Допущения
  – для управления нагрузкой ипользуем пины от 2 до 9
  – для физических выключателей испльзуем аналоговые пины (0 - нет реакции, 1/2 - выключение, 1 - включение)
  – запросы на управления через сеть имеют формат: GET /switch?<int pin>=<boolean status>
  – запрос на       получение состояний через сеть имеют формат: GET /switch или GET /status
  Подключаем Pins "ENC28J60 Module" к Arduino Uno.
  VCC - 3.3V
  GND - GND
  SCK - Pin 13
  SO - Pin 12
  SI - Pin 11
  CS - Pin 10 Можно выбрать любой.
*/
#include <EtherCard.h>

static uint32_t timer;

// IP адрес сервера (позже вынести в конфигурацию)
const char ecchost[] PROGMEM = "192.168.1.126";

// MAC адрес узла
static byte mymac[] = { 0x5A,0x5A,0x5A,0x5A,0x5A,0x5A };

byte Ethernet::buffer[500];

BufferFiller bfill;

// Общие переменные
int i;
char *data;
word pos;
boolean changedStatus = false;


// Массив задействованных номеров Pins Arduino, для управления реле.
// При правке, не забыть поправить в loop() проверку запроса
const int LedPins[] = {2,3,4,5};

// Количество рабочих пинов
const int PinCount = sizeof(LedPins) / sizeof(int);

// Массив для фиксации изменений.
boolean PinStatus[PinCount];

// Массив задействованных pin, для диммеров
// При правке, не забыть поправить в loop() проверку запроса
const int DimmerPins[] = {6};

// Количество рабочих пинов
const int DimmerCount = sizeof(DimmerPins) / sizeof(int);

// Массив для фиксации изменений.
boolean DimmerStatus[DimmerCount];

// Массив значений.
int DimmerLevels[DimmerCount];


// Кнопки на аналоговых вводах
// Для включения используем диапазон от 768 до 1023
// Для выключения используем диапазон от 256 до 767
// На значения до 255 не реагируем
// Список сенсоров
const int sensorPins[] = {A0,A1,A2,A3};

// Количество сенсоров
const int sensorCount = sizeof(sensorPins) / sizeof(int);

// id пинов, на которые влияют сенсоры
const int ledSensorPinIds[sensorCount] = {0,1,2,3};

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
  // Вывод информации о состоянии пинов
  for(i = 0; i < PinCount; i++) {
    bfill.emit_p(PSTR("{\"on\": $F, \"pin\": $D}"),
      PinStatus[i]?PSTR("false"):PSTR("true"),
      LedPins[i]
    );
    if (DimmerCount > 0 || i+1 < PinCount && PinCount > 1) {
      bfill.emit_p(PSTR(","));
    }
  }
  // Вывод информации о диммируемых пинах
  for(i = 0; i < DimmerCount; i++) {
    bfill.emit_p(PSTR("{\"on\": $F, \"pin\": $D, \"level\": $D}"),
      DimmerStatus[i]?PSTR("true"):PSTR("false"),
      DimmerPins[i],
      DimmerLevels[i]
    );
    if (i+1 < DimmerCount && DimmerCount > 1) {
      bfill.emit_p(PSTR(","));
    }
  }

  bfill.emit_p(PSTR("]"));
}

static void my_callback (byte status, word off, word len) {
  // Обработка ответа от сервера
  Ethernet::buffer[off] = 0;
}

void send_check_request() {
  // Просим сервер перечитать статус
  pos = ether.packetLoop(ether.packetReceive()); // check for tcp packet / проверить TCP пакеты.
  ether.browseUrl(PSTR("/mainframe/"), "check", ecchost, my_callback);
}

void setup() {
    Serial.begin(9600);
    // По умолчанию в Библиотеке "ethercard" (CS-pin) = № 8.
    if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0);

    if (!ether.dhcpSetup())
      Serial.println(F("DHCP failed"));

    // Тут есть проблема с таймаутом при попытке назначить все эти адреса (30 секунд)
    //ether.staticSetup(myip, mygw, mydns, mymask);
    ether.printIp("My SET IP: ", ether.myip); // Выводим в Serial монитор статический IP адрес.
    ether.printIp("GW:  ", ether.gwip);
    ether.printIp("DNS: ", ether.dnsip);
    //-----
    // Инициализация пинов ламп
    for(i = 0; i < PinCount; i++) {
        pinMode(LedPins[i],OUTPUT);
        PinStatus[i] = true;
        digitalWrite(LedPins[i], PinStatus[i]);
    }
    for(i = 0; i < DimmerCount; i++) {
        pinMode(DimmerPins[i],OUTPUT);
        DimmerStatus[i] = false;
        DimmerLevels[i] = 0;
        // Пока использую ШИМ для светодиода
        analogWrite(DimmerPins[i], DimmerLevels[i]);

    }

    // Пытаемся разрезолвить адрес сервере
    if (!ether.dnsLookup(ecchost))
      Serial.println("DNS failed");

    ether.printIp("SRV: ", ether.hisip);
}

void loop() {
  delay(5); // Дёргаем микроконтроллер.

  // Пока так кривенько, потом причешу
  // Диммируемые сейчас не переключаются
  for(i = 0; i < sensorCount; i++) {
    sensorValue = analogRead(sensorPins[i]);
    if (sensorValue > 255) {
      // Перепроверяем для сглаживания шума
      delay(10);
      sensorValue = analogRead(sensorPins[i]);
      changedStatus = false;
      if (sensorValue > 767) {
        // On
        if (PinStatus[ledSensorPinIds[i]] == true) {
          changedStatus = true;
        }
        PinStatus[ledSensorPinIds[i]] = false;
      } else if (sensorValue > 255) {
        // Off
        if (PinStatus[ledSensorPinIds[i]] == false) {
          changedStatus = true;
        }
        PinStatus[ledSensorPinIds[i]] = true;
      }
      // Записываем новый статус
      digitalWrite(LedPins[ledSensorPinIds[i]], PinStatus[ledSensorPinIds[i]]);
      if (changedStatus) {
        // Если статус изменился, сообщаем серверу
        send_check_request();
      }

    }
  }

  pos = ether.packetLoop(ether.packetReceive()); // check for tcp packet / проверить TCP пакеты.

  if (pos) {
    bfill = ether.tcpOffset();
    data = (char *) Ethernet::buffer + pos;
    // Варианты запроса
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
        if (strncmp(data, "?4=true ", 8) == 0 ) {
            PinStatus[2] = false;
        }
        if (strncmp(data, "?4=false ", 9) == 0 ) {
            PinStatus[2] = true;
        }
        if (strncmp(data, "?5=true ", 8) == 0 ) {
            PinStatus[3] = false;
        }
        if (strncmp(data, "?5=false ", 9) == 0 ) {
            PinStatus[3] = true;
        }
        // Диммеры пока включаются и выключаются тут
        if (strncmp(data, "?6=true ", 8) == 0 ) {
            DimmerStatus[0] = true;
        }
        if (strncmp(data, "?6=false ", 9) == 0 ) {
            DimmerStatus[0] = false;
        }

        for (i = 0; i < PinCount; i++) {
          // Применяем изменения состояния пинов
          digitalWrite(LedPins[i], PinStatus[i]);
        }

        for (i = 0; i < DimmerCount; i++) {
          // Применяем изменения состояния уровня
          if (DimmerStatus[i]) {
            analogWrite(DimmerPins[i], DimmerLevels[i] * 2);
          } else {
            analogWrite(DimmerPins[i], 0);
          }
        }

      }
      statusJson();
    } else if (strncmp("GET /dim", data, 8) == 0) {
      data += 8;
      if (strncmp(data, " ", 1) != 0 ) {
        if (strncmp(data, "?6=", 3) == 0 ) {
            data += 3;
            char *value = strchr(data, ' ');
            if (value != 0) {
              *value = 0;
              DimmerLevels[0] = atoi(data);
              delete value;
            }
        }
        for (i = 0; i < DimmerCount; i++) {
          // Применяем изменения состояния уровня
          if (DimmerStatus[i]) {
            analogWrite(DimmerPins[i], DimmerLevels[i] * 2);
          } else {
            analogWrite(DimmerPins[i], 0);
          }
        }
      }
      statusJson();
    } else {
      bfill.emit_p(http_404);
    }
    // Отправляем ответ
    ether.httpServerReply(bfill.position()); // send http response
  }
}
