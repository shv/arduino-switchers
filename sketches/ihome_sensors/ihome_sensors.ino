/*
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 */

#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <Ethernet.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 9

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 177);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
DeviceAddress insideThermometer, outsideThermometer;

EthernetClient client;
boolean currentLineIsBlank;
float tempC;

void setup(void)
{
  Serial.begin(9600);
  //while (!Serial) {
  //  ; // wait for serial port to connect. Needed for Leonardo only
  //}
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  // start serial port
  Serial.println("Dallas Temperature IC Control Library Demo");

  // Start up the library
  sensors.begin();

  // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");

  // assign address manually.  the addresses below will beed to be changed
  // to valid device addresses on your bus.  device address can be retrieved
  // by using either oneWire.search(deviceAddress) or individually via
  // sensors.getAddress(deviceAddress, index)
  //insideThermometer = { 0x28, 0x1D, 0x39, 0x31, 0x2, 0x0, 0x0, 0xF0 };
  //outsideThermometer   = { 0x28, 0x3F, 0x1C, 0x31, 0x2, 0x0, 0x0, 0x2 };

  // search for devices on the bus and assign based on an index.  ideally,
  // you would do this to initially discover addresses on the bus and then 
  // use those addresses and manually assign them (see above) once you know 
  // the devices on your bus (and assuming they don't change).
  // 
  // method 1: by index
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0"); 
  if (!sensors.getAddress(outsideThermometer, 1)) Serial.println("Unable to find address for Device 1"); 

  // method 2: search()
  // search() looks for the next device. Returns 1 if a new address has been
  // returned. A zero might mean that the bus is shorted, there are no devices, 
  // or you have already retrieved all of them.  It might be a good idea to 
  // check the CRC to make sure you didn't get garbage.  The order is 
  // deterministic. You will always get the same devices in the same order
  //
  // Must be called before search()
  //oneWire.reset_search();
  // assigns the first address found to insideThermometer
  //if (!oneWire.search(insideThermometer)) Serial.println("Unable to find address for insideThermometer");
  // assigns the seconds address found to outsideThermometer
  //if (!oneWire.search(outsideThermometer)) Serial.println("Unable to find address for outsideThermometer");

  // set the resolution to 9 bit
  sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
  sensors.setResolution(outsideThermometer, TEMPERATURE_PRECISION);

  Serial.print("Device 0 Resolution: ");
  Serial.print(sensors.getResolution(insideThermometer), DEC); 
  Serial.println();

  Serial.print("Device 1 Resolution: ");
  Serial.print(sensors.getResolution(outsideThermometer), DEC); 
  Serial.println();
}

void sendHeader200 ()
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");  // the connection will be closed after completion of the response
  client.println();
}

void sendSensorJSON (DeviceAddress thermometer)
{
  client.println("{\"value\": ");
  tempC = sensors.getTempC(thermometer);
  client.print(tempC);
  client.print(", \"pin\": ");
  client.print(ONE_WIRE_BUS);
  client.print(", \"sid\": \"");
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (thermometer[i] < 16) client.print("0");
    client.print(thermometer[i], HEX);
  }
  client.println("\"}");
}

void sendSensorsJSON ()
{
  client.println("[");
  sendSensorJSON(insideThermometer);
  client.println(",");
  sendSensorJSON(outsideThermometer);
  client.println("]");
}


void loop(void)
{
  // listen for incoming clients
  client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          // call sensors.requestTemperatures() to issue a global temperature 
          // request to all devices on the bus
          Serial.print("Requesting temperatures...");
          sensors.requestTemperatures();
          Serial.println("DONE");
          sendHeader200();

          sendSensorsJSON();

          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}

