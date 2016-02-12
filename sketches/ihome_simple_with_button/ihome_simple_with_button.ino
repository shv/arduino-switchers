/*
  Допущения: ипользуем пины от 1 до 9
  На двузначных номерах могут сломаться множественный операции
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

// Массив задействованных номеров Pins Arduino, для управления реле.
// На первой ардуинке не работает пин 4
const int LedPins[] = {2,3,5,6};

// Количество рабочих пинов
const int PinCount = sizeof(LedPins) / sizeof(int);

// Массив для фиксации изменений.
boolean PinStatus[PinCount];

// Кнопки
const int buttonPins[] = {9};

// Количество рабочих кнопок
const int buttonCount = sizeof(buttonPins) / sizeof(int);

// id пинов, на которые влияют кнопки
const int ledButtonPinIds[buttonCount] = {3};

// Состояние только что снятой кнопки
int buttonState = 0;

// Предыдущие статусы кнопок
int lastButtonStates[buttonCount] = {0};

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
  for(int i = 0; i < PinCount; i++) {
    bfill.emit_p(PSTR("{\"on\": $F, \"pin\": $D}"),
      PinStatus[i]?PSTR("true"):PSTR("false"),
      LedPins[i]
    );
    if (i+1 < PinCount && PinCount > 1) {
      bfill.emit_p(PSTR(","));
    }
  }
  bfill.emit_p(PSTR("]"));
}

int getPinStatus(int pinId) {
  buttonState = digitalRead(buttonPins[pinId]);
  // Убираем дребезг контактов
  if (lastButtonStates[pinId] != buttonState) {
    // Со временем нужно еще поиграться
    Serial.print("Change 1. ");
    Serial.print(" OldStatus: ");
    Serial.print(lastButtonStates[pinId]);
    Serial.print(" NewStatus: ");
    Serial.print(buttonState);
    Serial.print(" PinId: ");
    Serial.println(pinId);
    delay(10);
    buttonState = digitalRead(buttonPins[pinId]);
    if (lastButtonStates[pinId] != buttonState) {
      Serial.print("Change 2. ");
      Serial.print(" OldStatus: ");
      Serial.print(lastButtonStates[pinId]);
      Serial.print(" NewStatus: ");
      Serial.print(buttonState);
      Serial.print(" PinId: ");
      Serial.println(pinId);
      lastButtonStates[pinId] = buttonState;
    }
  }
  return lastButtonStates[pinId];
}


void setup() {
    Serial.begin(9600);
    // По умолчанию в Библиотеке "ethercard" (CS-pin) = № 8.
    // if (ether.begin(sizeof Ethernet::buffer, mymac) == 0).
    // and change it to: Меняем (CS-pin) на 10.
    // if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0).
    if (ether.begin(sizeof Ethernet::buffer, mymac,10) == 0);
    if (!ether.dhcpSetup());
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
    for(int i = 0; i < PinCount; i++) {
        pinMode(LedPins[i],OUTPUT);
        PinStatus[i] = false;
        digitalWrite(LedPins[i], PinStatus[i]);
    }
    // Инициализация пинов кнопок
    for(int i = 0; i < buttonCount; i++) {
      pinMode(buttonPins[i], INPUT);
      digitalWrite(buttonPins[i], lastButtonStates[i]);
    }


}

void loop() {
  delay(1); // Дёргаем микроконтроллер.

  // Кнопочное включение
  for(int i = 0; i < buttonCount; i++) {
    PinStatus[ledButtonPinIds[i]] = getPinStatus(i);
    digitalWrite(LedPins[ledButtonPinIds[i]], PinStatus[ledButtonPinIds[i]]);
  }

  word len = ether.packetReceive(); // check for ethernet packet / проверить ethernet пакеты.
  word pos = ether.packetLoop(len); // check for tcp packet / проверить TCP пакеты.
  if (pos) {
    bfill = ether.tcpOffset();
    char *data = (char *) Ethernet::buffer + pos;
    if (strncmp("GET /status ", data, 12) == 0) {
      statusJson();
    } else if (strncmp("GET /switch", data, 11) == 0) {
      data += 11;
      if (strncmp(data, " ", 1) != 0 ) {
        if (strncmp(data, "?2=true ", 8) == 0 ) {
            PinStatus[0] = true;
        }
        if (strncmp(data, "?2=false ", 9) == 0 ) {
            PinStatus[0] = false;
        }
        if (strncmp(data, "?3=true ", 8) == 0 ) {
            PinStatus[1] = true;
        }
        if (strncmp(data, "?3=false ", 9) == 0 ) {
            PinStatus[1] = false;
        }
        if (strncmp(data, "?5=true ", 8) == 0 ) {
            PinStatus[2] = true;
        }
        if (strncmp(data, "?5=false ", 9) == 0 ) {
            PinStatus[2] = false;
        }
        if (strncmp(data, "?6=true ", 8) == 0 ) {
            PinStatus[3] = true;
        }
        if (strncmp(data, "?6=false ", 9) == 0 ) {
            PinStatus[3] = false;
        }
        for (int i = 0; i < PinCount; i++) {
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
