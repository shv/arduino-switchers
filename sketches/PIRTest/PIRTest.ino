boolean PIRValue = false;
boolean OldPIRValue = PIRValue;
int PIPPin = 9;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(PIPPin, INPUT);
  pinMode(13, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  PIRValue = digitalRead(PIPPin);
  if (PIRValue != OldPIRValue) {
    OldPIRValue = PIRValue;
    Serial.print("Switch ");
    Serial.println(PIRValue ? "on": "off");
    digitalWrite(13, PIRValue);
  }
  delay(1);
}
