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

// Количество рабочих пинов
const int PinCount = 8;

// Массив задействованных номеров Pins Arduino, для управления например 8 реле.
int LedPins[PinCount] = {2,3,4,5,6,7,8,9};

// Массив для фиксации изменений.
boolean PinStatus[PinCount] = {false,false,false,false,false,false,false,false};

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
    for(int i = 0; i < PinCount; i++) {
        pinMode(LedPins[i],OUTPUT);
        digitalWrite(LedPins[i], PinStatus[i]);
    }
}

void loop() {
  delay(1); // Дёргаем микроконтроллер.

  word len = ether.packetReceive(); // check for ethernet packet / проверить ethernet пакеты.
  word pos = ether.packetLoop(len); // check for tcp packet / проверить TCP пакеты.
  if (pos) {
    bfill = ether.tcpOffset();
    char *data = (char *) Ethernet::buffer + pos;
    if (strncmp("GET /status ", data, 12) == 0) {
      statusJson();
    } else if (strncmp("GET /switch", data, 11) == 0) {
      data += 11;
      
        if (strncmp(data, "?on=2 ", 6) == 0 ) {
            PinStatus[0] = true;
        }
        if (strncmp(data, "?on=3 ", 6) == 0 ) {
            PinStatus[1] = true;
        }
        if (strncmp(data, "?on=4 ", 6) == 0 ) {
            PinStatus[2] = true;
        }
        if (strncmp(data, "?on=5 ", 6) == 0 ) {
            PinStatus[3] = true;
        }
        if (strncmp(data, "?on=6 ", 6) == 0 ) {
            PinStatus[4] = true;
        }
        if (strncmp(data, "?on=7 ", 6) == 0 ) {
            PinStatus[5] = true;
        }
        if (strncmp(data, "?on=8 ", 6) == 0 ) {
            PinStatus[6] = true;
        }
        if (strncmp(data, "?on=9 ", 6) == 0 ) {
            PinStatus[7] = true;
        }
        if (strncmp(data, "?off=2 ", 7) == 0 ) {
            PinStatus[0] = false;
        }
        if (strncmp(data, "?off=3 ", 7) == 0 ) {
            PinStatus[1] = false;
        }
        if (strncmp(data, "?off=4 ", 7) == 0 ) {
            PinStatus[2] = false;
        }
        if (strncmp(data, "?off=5 ", 7) == 0 ) {
            PinStatus[3] = false;
        }
        if (strncmp(data, "?off=6 ", 7) == 0 ) {
            PinStatus[4] = false;
        }
        if (strncmp(data, "?off=7 ", 7) == 0 ) {
            PinStatus[5] = false;
        }
        if (strncmp(data, "?off=8 ", 7) == 0 ) {
            PinStatus[6] = false;
        }
        if (strncmp(data, "?off=9 ", 7) == 0 ) {
            PinStatus[7] = false;
        }


      for (int i = 0; i < PinCount; i++) {
        //int pin = LedPins[i];
        //char *buffer;
        //String bf;
        //sprintf(buffer, "on=" + String(pin), pin);
        //buffer = String(pin);

        //bf = "?on=" + String(pin) + " ";
        //if (strncmp(data, bf.c_str(), 6) == 0 ) {
        //    PinStatus[i] = true;
        //}
        //sprintf(buffer, "?on=%d ", pin);
        //if (strncmp(data, buffer, 6) == 0 ) {
        //    PinStatus[i] = true;
        //}


        //sprintf(buffer, "off=%d", pin);
        //if (strncmp(data, buffer, 7) == 0 ) {
        //    PinStatus[i] = false;
        //}
        //if (strstr(data, buffer)) {
        //    PinStatus[i] = false;
        //}
        //PinStatus[7] = true;
        // Применяем изменения состояния пинов
        digitalWrite(LedPins[i], PinStatus[i]);
      }
      statusJson();
    } else {
      bfill.emit_p(http_404);
    }
    ether.httpServerReply(bfill.position()); // send http response
  }
}
