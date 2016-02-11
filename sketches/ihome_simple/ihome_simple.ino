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
      PinStatus[i]?PSTR("false"):PSTR("true"),
      LedPins[i]
    );
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
        PinStatus[i]=false;
    }
}

void loop() {
  delay(1); // Дёргаем микроконтроллер.

  word len = ether.packetReceive(); // check for ethernet packet / проверить ethernet пакеты.
  word pos = ether.packetLoop(len); // check for tcp packet / проверить TCP пакеты.
  if (pos) {
    bfill = ether.tcpOffset();
    char *data = (char *) Ethernet::buffer + pos;
    if (strncmp("GET /switch", data, 11) != 0) {
      bfill.emit_p(http_404);
    } else {
      data += 11;
      if (strncmp("on=1", data, 4) == 0) {
          PinStatus[1] = true;
      }
      if (strncmp("off=1", data, 4) == 0) {
          PinStatus[1] = false;
      }
      // Применяем изменения состояния пинов
      for (int i = 0; i < PinCount; i++) {
        digitalWrite(LedPins[i], PinStatus[i]);
      }
      statusJson(); // Return home page Если обнаружено изменения на станице, запускаем функцию.
    }
    ether.httpServerReply(bfill.position()); // send http response
  }
}
