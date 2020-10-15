// MASTER
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include "PCF8574.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#define BLYNK_PRINT Serial

char auth[] = "Rz8hI-YjZVfUY7qQb8hJGBFh48SuUn84";
char ssid[] = "Keenetic-4926";
char pass[] = "Q4WmFQTa";

PCF8574 pcf_1(0x20);
PCF8574 pcf_2(0x21);

#define slavesNumber 1
#define debug true

enum modes {automatic, manual, timeControlled, alert};
BlynkTimer relaysUsing;
bool setFlag = false;
/* virtual pins mapping
V0 - mode
V10 - relay1 control 
^
|
V25 - relay16 control



*/





// Structure for packet variables saving
struct packetData{
  int id;
  float airTemp;
  float airHum;
  float groundTemp;
  float groundHum;
  float lightLevel;
};

// Структура для хранения пинов к которым подключены реле


// Структура для хранения граничных значений для блоков сенсоров и автоматики
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
// Функция для сброса значений границ определённой структуры к стандартным(предустановленным)
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

// Класс описывающий одно конкретное реле
class relay{
  private:
    
    String name;
    bool state;
  public:
    int number;
    relay(int setNumber, String relayName) {number = setNumber; name = relayName;};
    relay(int setNumber, String relayName, bool defaultState) { number = setNumber; name = relayName; state = defaultState; };
    void setBindpin(int setNumber){ number = setNumber; }
    void setState(bool newState){ state  = newState; }
    void printInfo(){ Serial.println("Relay with name " + name + " on pin " + String(number) + " is " + (state)? String(true) : String(false)); }
    void on(){ state = true; }
    void off(){ state = false; }
    bool returnState(){ return state; }
};

void changeRelays(){}
// Функция для применения значений реле
void setRelay(relay r1);
void callRelays();
void setFlagTrue();
// void switchRelayTo(int relayNumber, bool state);

class workObj{
  private:
    int mode;

    packetData sensors1, sensors2, sensors3;
    borderValues borders;
  public:

    relay pump04_1 = relay(1, "Pump0.4Kv-1");
    relay pump04_2 = relay(2, "Pump0.4Kv-2");
    relay pump04_3 = relay(3, "Pump0.4Kv-3");
    relay valve_1 = relay(4, "Valve-1");
    relay valve_2 = relay(5, "Valve-2");
    relay light3_1 = relay(6, "Light3Kv-1");
    relay light1_1 = relay(7, "Light1Kv-1");
    relay light1_2 = relay(8, "Light1Kv-2");
    relay light01_1 = relay(9, "Light0.1KV-1");
    relay light01_2 = relay(10, "Light0.1Kv-2");
    relay distrif1_1 = relay(11, "Distrificator1Kv-1");
    relay distrif1_2 = relay(12, "Distrificator1Kv-1");
    relay steamgen1_1 = relay(13, "SteamGenerator1Kv-1");
    relay siod1_1 = relay(14, "SIOD1Kv");


    // If setAllDefaultFlag is false - border values will be recovered from EEPROM
    // Modes - automatic(0), manual(1), timeConrolled(2), alert(3)
    workObj(int startMode, bool setAllDefaultFlag) : mode(startMode) {
      // mode = startMode;
      if(setAllDefaultFlag == true)
      {
        dropBorders(borders);
      } else
      {
        restoreBordersFromEEPROM();  
      }


    }


    // Функция для записи данных с сенсоров
    void setSensorsData(packetData d1, int n) { 
      switch(n){
        case 1:
          sensors1 = d1; 
          break;
        case 2:
          sensors2 = d1;
          break;
        case 3:
          sensors3 = d1;
          break;
        default:
          Serial.println("Error writting data to sensors struct");
      }
      // sensors = d1;
      
    }
    // Функция для установки конкретной границы и её значения
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
    // Функция для установки всех значений границ разом
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
    
    


    void useRelays() const {
      // Relays are working in inverted mode - 0 is ON, 1 is OFF
      setRelay(pump04_1);
      setRelay(pump04_2);
      setRelay(pump04_3);
      setRelay(valve_1);
      setRelay(valve_2);
      setRelay(light3_1);
      setRelay(light1_1);
      setRelay(light1_2);
      setRelay(light01_1);
      setRelay(light01_2);
      setRelay(distrif1_1);
      setRelay(distrif1_2);
      setRelay(steamgen1_1);
      setRelay(siod1_1);
    }


    // Функция для автоматического полива по границам для каждого блока сенсоров
    bool autoGroundHum(int sensorBlocknumber){
      // Watering
      // 
      if (sensorBlocknumber == 1){
        if (sensors1.groundHum < borders.lowGroundHum){
          pump04_1.on();
        }
        if (sensors1.groundHum > borders.highGroundHum){
          pump04_2.off();
        }
      }
      
      if (sensorBlocknumber == 2){
        if (sensors2.groundHum < borders.lowGroundHum){
          pump04_2.on();
        }
        if (sensors1.groundHum > borders.highGroundHum){
          pump04_2.off();
        }
      }

      if (sensorBlocknumber == 3){
        if (sensors3.groundHum < borders.lowGroundHum){
          pump04_3.on();
        }
        if (sensors1.groundHum > borders.highGroundHum){
          pump04_3.off();
        }
      }

      return 0; // Left for errors
    }
    

    // TODO - допиать autoLight() спросив что она должна делать

    // Функция для автоматического включения света 
    bool autoLight(){
      return 0; // Left for errors
    }
    
    bool autoVent(){

    }
    // Функция для сохранения всех значений границ в энергонезависимую память
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
    // Функция для чтения всех значений границ из энергонезависимой памяти
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
    // Функция для смены режима финкционирования
    void changeModeTo(int changeToMode)
    {
      mode = changeToMode;
    }


    int getMode(){ return mode; }
};
// Прототип функции разбора пакета данных с блока сенсоров
void parsePackage(packetData&, String);
// Прототип функции отображения данных пакета в консоль
void showPackage(packetData);

// Зачаток функции для опроса всех блоков сенсоров разом
void slavesQuery();

workObj obj1(1, true);


// Прототип функции 




// Функиця для получения режима от Blynk
BLYNK_WRITE(V1)
{
  int a = param.asInt();
  // Если всё в штатном режиме, то меняем режим по указу Blynk
  if (obj1.getMode() != alert)
    obj1.changeModeTo(a);
};

// Реле 1. Можно управлять с Blynk только в режиме manual
// Pump04_1
BLYNK_WRITE(V10)
{
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.pump04_1.setState( (a == 0)? true : false );
}
// Реле 2
// Pump04_2
BLYNK_WRITE(V11){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.pump04_2.setState( (a == 0)? true : false );
}

// Реле 3
// Pump04_4
BLYNK_WRITE(V12){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.pump04_3.setState( (a == 0)? true : false );
}

// Реле 4
// valve1_1
BLYNK_WRITE(V13){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.valve_1.setState( (a == 0)? true : false );
}

// Реле 5
// valve1_2
BLYNK_WRITE(V14){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.valve_2.setState( (a == 0)? true : false );
}

// Реле 6
// light3_1
BLYNK_WRITE(V15){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.light3_1.setState( (a == 0)? true : false );
}

// Реле 7
// light1_1
BLYNK_WRITE(V16){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.light1_1.setState( (a == 0)? true : false );
}

// Реле 8
// light1_2
BLYNK_WRITE(V17){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.light1_2.setState( (a == 0)? true : false );
}

// Реле 9
// light01_1
BLYNK_WRITE(V18){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.light01_1.setState( (a == 0)? true : false );
}

// Реле 10
// light01_2
BLYNK_WRITE(V19){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.light01_2.setState( (a == 0)? true : false );
}

// Реле 11
// distrif1_1
BLYNK_WRITE(V20){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.distrif1_1.setState( (a == 0)? true : false );
}

// Реле 12
// distrif1_2
BLYNK_WRITE(V21){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.distrif1_2.setState( (a == 0)? true : false );
}

// Реле 13
// steamgen1_1
BLYNK_WRITE(V22){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.steamgen1_1.setState( (a == 0)? true : false );
}

// Реле 14
// siod1_1
BLYNK_WRITE(V23){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    obj1.siod1_1.setState( (a == 0)? true : false );
}

// Реле 15
// empty
BLYNK_WRITE(V24){
  int a = param.asInt();
  
}

// Реле 16
// empty
BLYNK_WRITE(V25){
  int a = param.asInt();
  
}



void setup() {
  // Wire.begin();        // join i2c bus (address optional for master)
  Serial.begin(115200);  // start serial for output
  // obj1.initRelays(relay1, relay2, relay3);

  Blynk.begin(auth, ssid, pass, IPAddress(192,168,0,106), 8080);
  // packetData data[slavesNumber]; 
  // relaysUsing.setInterval(500L, setFlagTrue);
  // sensorsQuerry.setInterval(2000L, slavesQuery(data));
}

void loop() {
  // relaysUsing.run();
  // sensorsQuerry.run();
  // callRelays();
  Blynk.run();

  
  
  
  // delay(1000);
}


void setFlagTrue(){
  setFlag = true;
}

void callRelays(){
  if (setFlag == true){
    obj1.useRelays();
  }
  setFlag = false;
}
// Функция для применения значения реле по его номеру
void setRelay(relay r1){
      switch(r1.number){
        case 1:
          pcf_1.write(0, !r1.returnState());
          break;
        case 2:
          pcf_1.write(1, !r1.returnState());
          break;
        case 3:
          pcf_1.write(2, !r1.returnState());
          break;
        case 4:
          pcf_1.write(3, !r1.returnState());
          break;
        case 5:
          pcf_1.write(4, !r1.returnState());
          break;
        case 6:
          pcf_1.write(5, !r1.returnState());
          break;
        case 7:
          pcf_1.write(6, !r1.returnState());
          break;
        case 8:
          pcf_1.write(7, !r1.returnState());
          break;
        case 9:
          pcf_2.write(0, !r1.returnState());
          break;
        case 10:
          pcf_2.write(1, !r1.returnState());
          break;
        case 11:
          pcf_2.write(2, !r1.returnState());
          break;
        case 12:
          pcf_2.write(3, !r1.returnState());
          break;
        case 13:
          pcf_2.write(4, !r1.returnState());
          break;
        case 14:
          pcf_2.write(5, !r1.returnState());
          break;
        case 15:
          pcf_2.write(6, !r1.returnState());
          break;
        case 16:
          pcf_2.write(7, !r1.returnState());
          break;
        default:
          Serial.print("Error no such relay");
          break;
      }
}


// Функция для опроса всех блоков сенсоров разом
// TODO - переделать тк сложно будет наладить обмен данныим между функцией и классом
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
// Функция для разбора пакета данных с блока сенсоров
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
// Функция для отображения данных пакета в консоль
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


