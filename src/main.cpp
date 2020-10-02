// MASTER
#include <Arduino.h>
#include <Wire.h>


#define relay1 2
#define relay2 4
#define relay3 12
#define relay4 5
#define relay5 16 
#define relay6 17
#define relay7 18
#define relay8 19
#define relay9 14
#define relay10 15
#define relay11 23
#define relay12 25
#define relay13 26
#define relay14 27
#define relay15 32
#define relay16 33


#define slavesNumber 1
#define debug true
void switchRelayTo(int relayNumber, bool state);

// Structure for packet variables saving
struct packetData{
  int id;
  float airTemp;
  float airHum;
  float groundTemp;
  float groundHum;
  float lightLevel;
};


void parsePackage(packetData&, String);
void showPackage(packetData);


void setup() {
  Wire.begin();        // join i2c bus (address optional for master)
  Serial.begin(115200);  // start serial for output
}

void loop() {
  // Wire.requestFrom(1, 28);    // request 6 bytes from slave device #8
  // String arrivedData = "";
  // packetData data1;
  // while (Wire.available() > 0) { // slave may send less than requested
    // char c = Wire.read(); // receive a byte as character
  //   arrivedData += c;   // Add symbol to arrivedData string for furute work
  //   Serial.print(c);         // print the character
  // }
  // Serial.println();
  // parsePackage(data1, arrivedData);
  // showPackage(data1);
  //Serial.println("Attempt to read data");
  packetData data[slavesNumber];
  
  for(int i = 1; i <= slavesNumber; i++){
    Wire.requestFrom(i, 28);
    String arrivedData = "";
    // packetData data;
    while (Wire.available() > 0){
      char c = Wire.read();
      arrivedData += c;
      if(debug) Serial.print(c);
    }
    parsePackage(data[i], arrivedData);
    showPackage(data[i]);
  }
  delay(1000);
}

void parsePackage(packetData& d1, String arrData){
  String temp = "";
  
  // ID reading
  char tempChar[10];
  tempChar[0] = arrData[0]; tempChar[1] = arrData[1];
  d1.id = atof(tempChar);

  // Air temperature reading
  tempChar[0] = arrData[3];
  tempChar[1] = arrData[4];
  d1.airTemp = atof(tempChar);
  if(arrData[2] == '-') d1.airTemp = -d1.airTemp;
  tempChar[0] = arrData[5];
  tempChar[1] = arrData[6];
  d1.airTemp += atof(tempChar)/100;

  // Air humidity reading
  tempChar[0] = arrData[7];
  tempChar[1] = arrData[8];
  tempChar[2] = arrData[9];
  d1.airHum = atof(tempChar);
  tempChar[0] = arrData[10];
  tempChar[1] = arrData[11];
  tempChar[2] = '\0';
  d1.airHum += atof(tempChar)/100;

  // Ground temperature reading
  tempChar[0] = arrData[13];
  tempChar[1] = arrData[14];
  d1.groundTemp = atof(tempChar);
  if(arrData[12] == '-') d1.groundTemp = -d1.groundTemp;
  tempChar[0] = arrData[15];
  tempChar[1] = arrData[16];
  d1.groundTemp += atof(tempChar)/100;

  // Ground humidity reading
  tempChar[0] = arrData[17];
  tempChar[1] = arrData[18];
  tempChar[2] = arrData[19];
  d1.groundHum = atof(tempChar);
  tempChar[2] = '\0';
  tempChar[0] = arrData[20];
  tempChar[1] = arrData[21];
  d1.groundHum += atof(tempChar)/100;

  // Light level reading
  tempChar[0] = arrData[22];
  tempChar[1] = arrData[23];
  tempChar[2] = arrData[24];
  d1.lightLevel = atof(tempChar);
  tempChar[2] = '\0';
  tempChar[0] = arrData[26];
  tempChar[1] = arrData[27];
  d1.lightLevel += atof(tempChar)/100;
}
void showPackage(packetData p1){
  Serial.println("/-----------PACKAGE-DATA-----------");
  Serial.println("ID of sender       : " + String(p1.id));
  Serial.println("Air temperature    : " + String(p1.airTemp));
  Serial.println("Air humidity       : " + String(p1.airHum));
  Serial.println("Ground temperature : " + String(p1.groundTemp));
  Serial.println("Ground humidity    : " + String(p1.groundHum));
  Serial.println("Light level        : " + String(p1.lightLevel));
  Serial.println("/---------------------------------/");
}


void switchRelayTo(int relayNumber, bool state){
  if (state)
    digitalWrite(relayNumber, HIGH);
  else
    digitalWrite(relayNumber, LOW);
}