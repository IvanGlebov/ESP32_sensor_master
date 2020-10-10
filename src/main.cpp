// MASTER
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#define BLYNK_PRINT Serial

char auth[] = "YourAuthToken";
char ssid[] = "YourNetworkName";
char pass[] = "YourPassword";



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

struct perfRelays{
  bool r1, r2, r3, r4;
};

struct perfData{
  // bool lightRelay;
  // bool pumpRealy;
  // bool ventRealy;
  int mode;
};

struct borderValues{
  float lowGroundHum;
  float highGroundHum;
  float lowGroundTemp;
  float highGroundTemp;
  float lowAirHum;
  float highAirHum;
  float lowAirTemp;
  float highAirTemp;
  float lowLightLevel;
  float highLightLevel;
};

void dropBorders(borderValues &b1){
  b1.highAirHum = 80;
  b1.highAirTemp = 40;
  b1.highGroundHum = 90;
  b1.highGroundTemp = 30;
  b1.highLightLevel = 5000;
  b1.lowAirHum = 20;
  b1.lowAirTemp = 10;
  b1.lowGroundHum = 20;
  b1.lowGroundTemp = 12;
  b1.lowLightLevel = 200;
}


class workObj{
  private:
    enum modes {automatic, manual, timeControlled, alert};
    perfRelays relays1, relays2;
    int mode;
    // perfData perfData1;
    packetData sensors;
    borderValues borders;
  public:
    // If setAllDefaultFlag is false - border values will be recovered from EEPROM
    // Modes - automatic(0), manual(1), timeConrolled(2), alert(3)
    workObj(int startMode, bool setAllDefaultFlag){
      mode = startMode;
      if(setAllDefaultFlag == true)
      {
        dropBorders(borders);
      } else
      {
        restoreBordersFromEEPROM();  
      }
      
      
      // perfData1.mode = startMode;
      // if (setAllDefaultFlag == true){
        // perfData1.lightRelay = false;
        // perfData1.pumpRealy = false;
        // perfData1.ventRealy = false;
      // }

    }
    
    void initRelays1(int r1, int r2, int r3, int r4){
      relays1.r1 = r1;
      relays1.r2 = r2;
      relays1.r3 = r3;
      relays1.r4 = r4;
    }
    
    void initRelays2(int r1, int r2, int r3, int r4){
      relays2.r1 = r1;
      relays2.r2 = r2;
      relays2.r3 = r3;
      relays2.r4 = r4;
    }
    
    void setSensorsData(packetData d1, int n) { 
      /*switch(n){
        case 1:
          sensors1 = d1; 
          break;
        case 2:
          sensors2 = d1;
          break;
      }*/
      sensors = d1;
      
    }
    
    void setBorder(String border, float value){
      if (border == "lowGroundHum"){
        borders.lowGroundHum = value;
      } else if (border == "highGroundHum"){
        borders.highGroundHum = value;
      } else if (border == "lowGroundTemp"){
        borders.lowGroundTemp = value;
      } else if (border == "highGroundTemp"){
        borders.highGroundTemp = value;
      } else if (border == "lowAirHum"){
        borders.lowAirHum = value;
      } else if (border == "highAirHum"){
        borders.highAirHum = value;
      } else if (border == "lowAirTemp"){
        borders.lowAirTemp = value;
      } else if (border == "highAirTemp"){
        borders.highAirTemp = value;
      } else if (border == "lowLightLevel"){
        borders.lowLightLevel = value;
      } else if (border == "highLightLevel"){
        borders.highLightLevel = value;
      }
    }
    
    void setAllBorders(float lowGroundHum, float highGroundHum, float lowGroundTemp, 
                    float highGroundTemp, float lowAirHum, float highAirHum, 
                    float lowAirTemp, float highAirTemp, float lowLightLevel, 
                    float highLightLevel)
    {
      borders.lowGroundHum = lowGroundHum;
      borders.highGroundHum = highGroundHum;
      borders.lowGroundTemp = lowGroundTemp;
      borders.highGroundTemp = highGroundTemp;
      borders.lowAirHum = lowAirHum;
      borders.highAirHum = highAirHum;
      borders.lowAirTemp = lowAirTemp;
      borders.highAirTemp = highAirTemp;
      borders.lowLightLevel = lowLightLevel;
      borders.highLightLevel = highLightLevel;
    }
    
    bool autoGroundHum(){
      // Watering
      if (sensors.groundHum < borders.lowGroundHum){
        perfData1.pumpRealy = true;
      }
      if (sensors.groundHum > borders.highGroundHum){
        perfData1.pumpRealy = false;
      }
      return 0; // Left for errors
    }
    
    bool autoLight(){
      if (sensors.lightLevel < borders.lowLightLevel && perfData1.lightRelay == false){
        perfData1.lightRelay = true;
      }
      if (sensors.lightLevel > borders.highLightLevel && perfData1.lightRelay == true){
        perfData1.lightRelay = false;
      }
      return 0; // Left for errors
    }
    
    bool autoVent(){

    }

    void saveBordersToEEPROM(){
      EEPROM.write(0, borders.lowGroundHum);
      EEPROM.write(1, borders.highGroundHum);
      EEPROM.write(2, borders.lowGroundTemp);
      EEPROM.write(3, borders.highGroundTemp);
      EEPROM.write(4, borders.lowAirHum);
      EEPROM.write(5, borders.highAirHum);
      EEPROM.write(6, borders.lowAirTemp);
      EEPROM.write(7, borders.highAirTemp);
      EEPROM.write(8, borders.lowLightLevel);
      EEPROM.write(9, borders.highLightLevel);
    }

    void restoreBordersFromEEPROM(){
      borders.lowGroundHum = EEPROM.read(0);
      borders.highGroundHum = EEPROM.read(1);
      borders.lowGroundTemp = EEPROM.read(2);
      borders.highGroundTemp = EEPROM.read(3);
      borders.lowAirHum = EEPROM.read(4);
      borders.highAirHum = EEPROM.read(5);
      borders.lowAirTemp = EEPROM.read(6);
      borders.highAirTemp = EEPROM.read(7);
      borders.lowLightLevel = EEPROM.read(8);
      borders.highLightLevel = EEPROM.read(9);
    }

    void changeModeTo(int mode)
    {

    }

};

void parsePackage(packetData&, String);
void showPackage(packetData);

// BlynkTimer sensorsQuerry;
void slavesQuery();

workObj obj1(0, true);

void setup() {
  Wire.begin();        // join i2c bus (address optional for master)
  Serial.begin(115200);  // start serial for output
  // obj1.initRelays(relay1, relay2, relay3);

  Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,100), 8080);
  packetData data[slavesNumber]; 
  // sensorsQuerry.setInterval(2000L, slavesQuery(data));
}

void loop() {
  
  // sensorsQuerry.run();
  Blynk.run();

  
  
  
  delay(1000);
}


void slavesQuery(packetData* data[]){
  for(int i = 1; i <= slavesNumber; i++){
    Wire.requestFrom(i, 28);
    String arrivedData = "";
    // packetData data;
    while (Wire.available() > 0){
      char c = Wire.read();
      arrivedData += c;
      if(debug) Serial.print(c);
    }
    parsePackage(*(data[i]), arrivedData);
    showPackage(*(data[i]));
  }
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


