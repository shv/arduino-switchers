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
//#include <etherShield.h> //https://geektimes.ru/post/255430/
//#include <ETHER_28J60.h> //https://geektimes.ru/post/255430/

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


// Список сущностей, которые можно включать и выключать
const int PowerPins[] = {2, 3, 4, 5, 6};

// Количество рабочих пинов
const int PowerPinCount = sizeof(PowerPins) / sizeof(int);

// Из этих сущностей диммеры и обычные пины
const boolean PowerPinIsDimmers[] = {false, false, false, false, true};

// Массив для фиксации изменений.
boolean PowerPinStatuses[PowerPinCount];

// Массив значений.
int PowerPinLevels[PowerPinCount];


// Пины кнопок
const int ControlPins[] = {A2, A3, 7};

// Количество пинов кнопок
const int ControlPinCount = sizeof(ControlPins) / sizeof(int);

// Типы пинов контроля
// 0 - аналоговый ввод с состояниями: 50% - выключено, 100% - включено
// 1 - аналоговый ввод с состояниями: 100% - сменить состояние
// 2 - цифровая кнопка с состояниями: 1 - сменить состояние
// 3 - цифровая кнопка с состояниями: 1 - включить
// 4 - цифровая кнопка с состояниями: 1 - выключить
// 5 - цифровая кнопка с состояниями: 1 - включить, 0 - выключить
const int ControlPinTypes[] = {0, 0, 2};

// ID Power пинов, на которые действуют пины контроля
const int ControlPowerPinIds[] = {0, 2, 3};

// Зафиксированные значения контролов
int PermanetnControlValues[] = {0, 0, 0};
// Кнопки на аналоговых вводах
// Для включения используем диапазон от 768 до 1023
// Для выключения используем диапазон от 256 до 767
// На значения до 255 не реагируем
// Список сенсоров

// Полученное значение сенсора
int ControlValue = 0;


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
  for(i = 0; i < PowerPinCount; i++) {
    if (PowerPinIsDimmers[i]) {
      bfill.emit_p(PSTR("{\"on\": $F, \"pin\": $D, \"level\": $D}"),
        PowerPinStatuses[i]?PSTR("false"):PSTR("true"),
        PowerPins[i],
        PowerPinLevels[i]
      );
    } else {
    bfill.emit_p(PSTR("{\"on\": $F, \"pin\": $D}"),
      PowerPinStatuses[i]?PSTR("false"):PSTR("true"),
      PowerPins[i]
    );
    }
    if (i+1 < PowerPinCount && PowerPinCount > 1) {
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
  //bfill = ether.tcpOffset();
  ether.browseUrl(PSTR("/mainframe/"), "check", ecchost, my_callback);
}

void apply_pin_values() {
  for (i = 0; i < PowerPinCount; i++) {
    // Применяем изменения состояния пинов
    // false - включен
    // true - выключен
    if (PowerPinIsDimmers[i]) {
      if (PowerPinStatuses[i]) {
        analogWrite(PowerPins[i], 0);
      } else {
        analogWrite(PowerPins[i], PowerPinLevels[i] * 2);
      }
    } else {
      digitalWrite(PowerPins[i], PowerPinStatuses[i]);
    }
  }
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

    // Пытаемся разрезолвить адрес сервере
    if (!ether.dnsLookup(ecchost))
      Serial.println("DNS failed");

    ether.printIp("SRV: ", ether.hisip);

    //-----
    // Инициализация пинов ламп
    for(i = 0; i < PowerPinCount; i++) {
        pinMode(PowerPins[i],OUTPUT);
        // false - включен
        // true - выключен
        PowerPinStatuses[i] = true;
        if (PowerPinIsDimmers[i]) {
          PowerPinLevels[i] = 0;
          // Пока использую ШИМ для светодиода
          analogWrite(PowerPins[i], PowerPinLevels[i]);
        } else {
          digitalWrite(PowerPins[i], PowerPinStatuses[i]);
        }
    }
    // Инициализация пинов ламп
    for(i = 0; i < ControlPinCount; i++) {
      if (ControlPinTypes[i] >= 2 && ControlPinTypes[i] >= 5) {
        // Цифровые входы
        pinMode(ControlPins[i],INPUT);
      }
    }

}

void loop() {
  delay(5); // Дёргаем микроконтроллер.

  // Пока так кривенько, потом причешу
  for(i = 0; i < ControlPinCount; i++) {
    if (ControlPinTypes[i] == 0) {
      ControlValue = analogRead(ControlPins[i]);
      if (ControlValue > 255) {
        // Перепроверяем для сглаживания шума
        delay(10);
        ControlValue = analogRead(ControlPins[i]);
        changedStatus = false;
        if (ControlValue > 767) {
          // On
          if (PowerPinStatuses[ControlPowerPinIds[i]] == true) {
            changedStatus = true;
          }
          PowerPinStatuses[ControlPowerPinIds[i]] = false;
        } else if (ControlValue > 255) {
          // Off
          if (PowerPinStatuses[ControlPowerPinIds[i]] == false) {
            changedStatus = true;
          }
          PowerPinStatuses[ControlPowerPinIds[i]] = true;
        }
        // Записываем новый статус
        apply_pin_values();

        if (changedStatus) {
          // Если статус изменился, сообщаем серверу
          send_check_request();
        }
  
      }
    } else if (ControlPinTypes[i] == 2) {
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
            send_check_request();
          }
        }
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
            PowerPinStatuses[0] = false;
        }
        if (strncmp(data, "?2=false ", 9) == 0 ) {
            PowerPinStatuses[0] = true;
        }
        if (strncmp(data, "?3=true ", 8) == 0 ) {
            PowerPinStatuses[1] = false;
        }
        if (strncmp(data, "?3=false ", 9) == 0 ) {
            PowerPinStatuses[1] = true;
        }
        if (strncmp(data, "?4=true ", 8) == 0 ) {
            PowerPinStatuses[2] = false;
        }
        if (strncmp(data, "?4=false ", 9) == 0 ) {
            PowerPinStatuses[2] = true;
        }
        if (strncmp(data, "?5=true ", 8) == 0 ) {
            PowerPinStatuses[3] = false;
        }
        if (strncmp(data, "?5=false ", 9) == 0 ) {
            PowerPinStatuses[3] = true;
        }
        if (strncmp(data, "?6=true ", 8) == 0 ) {
            PowerPinStatuses[4] = false;
        }
        if (strncmp(data, "?6=false ", 9) == 0 ) {
            PowerPinStatuses[4] = true;
        }
        apply_pin_values();
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
              PowerPinLevels[4] = atoi(data);
              delete value;
            }
        }
        apply_pin_values();
      }
      statusJson();
    } else {
      bfill.emit_p(http_404);
    }
    // Отправляем ответ
    ether.httpServerReply(bfill.position()); // send http response
  }
}
