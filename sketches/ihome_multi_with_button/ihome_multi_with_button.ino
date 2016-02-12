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
const int LedPins[PinCount] = {3,4,5,6,7,8,9};

// Массив для фиксации изменений.
boolean PinStatus[PinCount] = {false,false,false,false,false,false,false};

// Количество рабочих кнопок
const int buttonCount = 2;

// Кнопки
const int buttonPins[buttonCount] = {1,2};

// Пины, на которые влияют кнопки
const int ledButtonPins[buttonCount] = {3,4};

// Состояние только что снятой кнопки
int buttonState = 0;

// Предыдущие статусы кнопок
int lastButtonStates[buttonCount] = {0,0};

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
    delay(10);
    buttonState = digitalRead(buttonPins[pinId]);
    if (lastButtonStates[pinId] != buttonState) {
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
        digitalWrite(LedPins[i], PinStatus[i]);
    }
    // Инициализация пинов кнопок
    for(int i = 0; i < buttonCount; i++) {
      pinMode(ledButtonPins[i], INPUT);
      digitalWrite(buttonPins[i], lastButtonStates[i]);
    }


}

void loop() {
  delay(1); // Дёргаем микроконтроллер.

  // Кнопочное включение
  for(int i = 0; i < buttonCount; i++) {
    digitalWrite(ledButtonPins[i], getPinStatus(i));
  }

  word len = ether.packetReceive(); // check for ethernet packet / проверить ethernet пакеты.
  word pos = ether.packetLoop(len); // check for tcp packet / проверить TCP пакеты.
  if (pos) {
    bfill = ether.tcpOffset();
    char *data = (char *) Ethernet::buffer + pos;
 
    
    Serial.println(data); // Выводим в Serial монитор IP адрес который нам присвоил Router.
    
    if (strncmp("GET /status ", data, 12) == 0) {
      statusJson();
    } else if (strncmp("GET /switch", data, 11) == 0) {
      // Запрос такого формата: /switch?1=0&2=0&3=1&4=1
      // Оставляем первую строку
      char *str = strtok (data,"\r\n");
      // Выделяем метод
      char *method = strtok (str," ");
      Serial.print("Method: "); // Выводим в Serial монитор IP адрес который нам присвоил Router.
      Serial.println(method);
      // Получаем строку запроса
      char *req = strtok (NULL," ");
      Serial.print("Request: "); // Выводим в Serial монитор IP адрес который нам присвоил Router.
      Serial.println(req);
      // Получаем локейшн запроса
      char *uri = strtok (req,"?"); // во втором параметре указаны разделитель (пробел, запятая, точка, тире)
      Serial.print("URI: "); // Выводим в Serial монитор IP адрес который нам присвоил Router.
      Serial.println(uri);
      if (uri) {
        // Получаем первый аргумент
        char *arg = strtok (NULL, "&");
        // пока есть лексемы
        while (arg != NULL) {
          char *value = strchr(arg, '=');
          if (value != 0) {
            *value = 0;
            Serial.print("Arg name: "); // Выводим в Serial монитор IP адрес который нам присвоил Router.
            Serial.println(arg);
            int key = atoi(arg);
            Serial.print("Arg key: "); // Выводим в Serial монитор IP адрес который нам присвоил Router.
            Serial.println(key);
            ++value;
            Serial.print("Arg value: "); // Выводим в Serial монитор IP адрес который нам присвоил Router.
            Serial.println(value);
            int val = atoi(value);
            Serial.print("Arg val: "); // Выводим в Serial монитор IP адрес который нам присвоил Router.
            Serial.println(val);
    
            PinStatus[key] = val?true:false;
  
            printf("Arg: name=%s, value=%d\n", arg, val);
          }
          arg = strtok (NULL, "&");
        }
      }
      statusJson();
    } else if (strncmp("GET /switch_one", data, 15) == 0) {
      data += 15;
      
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
        //char buffer[];
        //String bf;
        //sprintf(buffer, "on=" + String(pin), pin);
        //buffer = String(pin);

        //bf = "?on=" + String(pin) + " ";
        //if (strncmp(data, bf.c_str(), 6) == 0 ) {
        //    PinStatus[i] = true;
        //}
        //char buffer[7];
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
