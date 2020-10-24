// MASTER
#include <Arduino.h>

#define BLYNK_PRINT Serial


#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <EEPROM.h>
#include "PCF8574.h"
// home
// Rz8hI-YjZVfUY7qQb8hJGBFh48SuUn84
// rwuSDq5orOfSV8nF75xsQT7YSR_e2Xqf
char auth[] = "Rz8hI-YjZVfUY7qQb8hJGBFh48SuUn84";
char ssid[] = "Keenetic-4926"; // Keenetic-4926
char pass[] = "Q4WmFQTa"; // Q4WmFQTa

PCF8574 pcf_1(0x20);
PCF8574 pcf_2(0x21);

#define slavesNumber 1
#define debug true

enum modes {automatic, manual, timeControlled, alert};
// bool setFlag = false;

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


// Функция для применения значений реле
void setRelay(relay r1);
void callRelays();


class workObj{
  private:
    int mode;

    packetData sensors1, sensors2, sensors3;
    borderValues borders[2];
  public:

    


    // If setAllDefaultFlag is false - border values will be recovered from EEPROM
    // Modes - automatic(0), manual(1), timeConrolled(2), alert(3)
    workObj(int startMode, bool setAllDefaultFlag) : mode(startMode) {
      // mode = startMode;
      if(setAllDefaultFlag == true)
      {
        dropBorders(borders[1]);
        dropBorders(borders[2]);
      } else
      {
        restoreBordersFromEEPROM(1);  
        restoreBordersFromEEPROM(2);
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
    /* Ключи для значений
    - lowGroundHum
    - highGroundHum
    - lowGroundTemp
    - highGroundTemp
    - lowAirHum
    - highAirHum
    - lowAirTemp
    - highAirTemp
    - lowLightLevel
    - highLightLevel
    */
    void setBorder(String border, float value, int bordersGroup){
      
      if (border == "lowGroundHum"){
        borders[bordersGroup].lowGroundHum = value;
      } else if (border == "highGroundHum"){
        borders[bordersGroup].highGroundHum = value;
      } else if (border == "lowGroundTemp"){
        borders[bordersGroup].lowGroundTemp = value;
      } else if (border == "highGroundTemp"){
        borders[bordersGroup].highGroundTemp = value;
      } else if (border == "lowAirHum"){
        borders[bordersGroup].lowAirHum = value;
      } else if (border == "highAirHum"){
        borders[bordersGroup].highAirHum = value;
      } else if (border == "lowAirTemp"){
        borders[bordersGroup].lowAirTemp = value;
      } else if (border == "highAirTemp"){
        borders[bordersGroup].highAirTemp = value;
      } else if (border == "lowLightLevel"){
        borders[bordersGroup].lowLightLevel = value;
      } else if (border == "highLightLevel"){
        borders[bordersGroup].highLightLevel = value;
      }

    }
    // Функция для установки всех значений границ разом
    void setAllBordersGroup(float lowGroundHum, float highGroundHum, float lowGroundTemp, 
                       float highGroundTemp, float lowAirHum, float highAirHum, 
                       float lowAirTemp, float highAirTemp, float lowLightLevel, 
                       float highLightLevel, int bordersGroup)
    {
    
      borders[bordersGroup].lowGroundHum = lowGroundHum;
      borders[bordersGroup].highGroundHum = highGroundHum;
      borders[bordersGroup].lowGroundTemp = lowGroundTemp;
      borders[bordersGroup].highGroundTemp = highGroundTemp;
      borders[bordersGroup].lowAirHum = lowAirHum;
      borders[bordersGroup].highAirHum = highAirHum;
      borders[bordersGroup].lowAirTemp = lowAirTemp;
      borders[bordersGroup].highAirTemp = highAirTemp;
      borders[bordersGroup].lowLightLevel = lowLightLevel;
      borders[bordersGroup].highLightLevel = highLightLevel;

    }
    
    // Функция для переключения состояний реле
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
    

    // TODO - допиать autoLight() спросив что она должна делать

    // Функция для автоматического включения света 
    
    
    // Функция для сохранения всех значений границ в энергонезависимую память
    /* Ключи для значений
    - lowGroundHum
    - highGroundHum
    - lowGroundTemp
    - highGroundTemp
    - lowAirHum
    - highAirHum
    - lowAirTemp
    - highAirTemp
    - lowLightLevel
    - highLightLevel
    */
    void saveBordersToEEPROM(int bordersGroup, String border){
      if (border == "lowGroundHum")
        EEPROM.write(0 + bordersGroup, borders[bordersGroup].lowGroundHum);
      if (border == "highGroundHum")
        EEPROM.write(1 + bordersGroup, borders[bordersGroup].highGroundHum);
      if (border == "lowGroundTemp")
        EEPROM.write(2 + bordersGroup, borders[bordersGroup].lowGroundTemp+40);
      if (border == "highGroundTemp")
        EEPROM.write(3 + bordersGroup, borders[bordersGroup].highGroundTemp+40);
      if (border == "lowAirHum")
        EEPROM.write(4 + bordersGroup, borders[bordersGroup].lowAirHum);
      if (border == "highAirHum")
        EEPROM.write(5 + bordersGroup, borders[bordersGroup].highAirHum);
      if (border == "lowAirTemp")
        EEPROM.write(6 + bordersGroup, borders[bordersGroup].lowAirTemp+40);
      if (border == "highAirTemp")
        EEPROM.write(7 + bordersGroup, borders[bordersGroup].highAirTemp+40);
      if (border == "lowLightLevel")
        EEPROM.write(8 + bordersGroup, borders[bordersGroup].lowLightLevel/10);
      if (border == "highLightLevel")
        EEPROM.write(9 + bordersGroup, borders[bordersGroup].highLightLevel/10);
      EEPROM.commit();
    }
    // Функция для чтения всех значений границ из энергонезависимой памяти
    void restoreBordersFromEEPROM(int bordersGroup){
      borders[bordersGroup].lowGroundHum = (EEPROM.read(0 + bordersGroup) == 255) ? 0 : EEPROM.read(0 + bordersGroup);
      borders[bordersGroup].highGroundHum = (EEPROM.read(1 + bordersGroup) == 255) ? 0 : EEPROM.read(1 + bordersGroup);
      borders[bordersGroup].lowGroundTemp = (EEPROM.read(2 + bordersGroup) == 255) ? 0 : EEPROM.read(2 + bordersGroup)-40;
      borders[bordersGroup].highGroundTemp = (EEPROM.read(3 + bordersGroup) == 255) ? 0 : EEPROM.read(3 + bordersGroup)-40;
      borders[bordersGroup].lowAirHum = (EEPROM.read(4 + bordersGroup) == 255) ? 0 : EEPROM.read(4 + bordersGroup);
      borders[bordersGroup].highAirHum = (EEPROM.read(5 + bordersGroup) == 255) ? 0 : EEPROM.read(5 + bordersGroup);
      borders[bordersGroup].lowAirTemp = (EEPROM.read(6 + bordersGroup) == 255) ? 0 : EEPROM.read(6 + bordersGroup)-40;
      borders[bordersGroup].highAirTemp = (EEPROM.read(7 + bordersGroup) == 255) ? 0 : EEPROM.read(7 + bordersGroup)-40;
      borders[bordersGroup].lowLightLevel = (EEPROM.read(8 + bordersGroup) == 255) ? 0 : EEPROM.read(8 + bordersGroup)*10;
      borders[bordersGroup].highLightLevel = (EEPROM.read(9 + bordersGroup) == 255) ? 0 : EEPROM.read(9 + bordersGroup)*10;
    }
    // Функция для смены режима финкционирования
    void changeModeTo(int changeToMode)
    {
      mode = changeToMode;
    }

    void showBorders(int bordersGroup){
      Serial.println("-------------------------------------------");
      Serial.println("LowGroundHum" + String(bordersGroup) + " :" + String(borders[bordersGroup].lowGroundHum));
      Serial.println("HighGroundHum" + String(bordersGroup) + " :" + String(borders[bordersGroup].highGroundHum));
      Serial.println("LowGroundTemp" + String(bordersGroup) + " :" + String(borders[bordersGroup].lowGroundTemp));
      Serial.println("HighGroundTemp" + String(bordersGroup) + " :" + String(borders[bordersGroup].highGroundTemp));
      Serial.println("LowAirHum" + String(bordersGroup) + " :" + String(borders[bordersGroup].lowAirHum));
      Serial.println("HighAirHum" + String(bordersGroup) + " :" + String(borders[bordersGroup].highAirHum));
      Serial.println("LowAirTemp" + String(bordersGroup) + " :" + String(borders[bordersGroup].lowAirTemp));
      Serial.println("HighAirTemp" + String(bordersGroup) + " :" + String(borders[bordersGroup].highAirTemp));
      Serial.println("LowLightLeve" + String(bordersGroup) + " :" + String(borders[bordersGroup].lowLightLevel));
      Serial.println("HighLightLevel" + String(bordersGroup) + " :" + String(borders[bordersGroup].highLightLevel));
      Serial.println("-------------------------------------------");
    }
    int getMode(){ return mode; }
};
// Прототип функции разбора пакета данных с блока сенсоров
void parsePackage(packetData&, String);
// Прототип функции отображения данных пакета в консоль
void showPackage(packetData);

// Зачаток функции для опроса всех блоков сенсоров разом
void slavesQuery();

workObj obj1(1, false);





//  000000  000000  00        00    00   00   00000
//  00  00  00      00       0  0    00 00   00
//  000000  000000  00      000000    000     0000
//  00 00   00      00      00  00   00         00
//  00  00  000000  000000  00  00  00       00000

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
  // Serial.println("switching r1");
  int a = param.asInt();
  if (obj1.getMode() == manual){
    pump04_1.setState( (a == 0)? true : false );
  }

    //
}
// Реле 2
// Pump04_2
BLYNK_WRITE(V11){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    pump04_2.setState( (a == 0)? true : false );
}

// Реле 3
// Pump04_4
BLYNK_WRITE(V12){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    pump04_3.setState( (a == 0)? true : false );
}

// Реле 4
// valve1_1
BLYNK_WRITE(V13){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    valve_1.setState( (a == 0)? true : false );
}

// Реле 5
// valve1_2
BLYNK_WRITE(V14){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    valve_2.setState( (a == 0)? true : false );
}

// Реле 6
// light3_1
BLYNK_WRITE(V15){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    light3_1.setState( (a == 0)? true : false );
}

// Реле 7
// light1_1
BLYNK_WRITE(V16){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    light1_1.setState( (a == 0)? true : false );
}

// Реле 8
// light1_2
BLYNK_WRITE(V17){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    light1_2.setState( (a == 0)? true : false );
}

// Реле 9
// light01_1
BLYNK_WRITE(V18){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    light01_1.setState( (a == 0)? true : false );
}

// Реле 10
// light01_2
BLYNK_WRITE(V19){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    light01_2.setState( (a == 0)? true : false );
}

// Реле 11
// distrif1_1
BLYNK_WRITE(V20){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    distrif1_1.setState( (a == 0)? true : false );
}

// Реле 12
// distrif1_2
BLYNK_WRITE(V21){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    distrif1_2.setState( (a == 0)? true : false );
}

// Реле 13
// steamgen1_1
BLYNK_WRITE(V22){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    steamgen1_1.setState( (a == 0)? true : false );
}

// Реле 14
// siod1_1
BLYNK_WRITE(V23){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    siod1_1.setState( (a == 0)? true : false );
}

// Реле 15
// empty
BLYNK_WRITE(V24){
  // int a = param.asInt();
  
}

// Реле 16
// empty
BLYNK_WRITE(V25){
  // int a = param.asInt();
  
}



// 00000   000000  00000   00000   000000  00000   000000
// 00  00  00  00  00  00  00  00  00      00  00  00
// 000000  00  00  00000   00  00  000000  00000   000000
// 00  00  00  00  00 00   00  00  00      00 00       00
// 00000   000000  00  00  00000   000000  00  00  000000
// Граничные значения 

// Group1
// V30 -> V39
// Group2
// V40 -> V49

// GROUP 1
// lowGroundHum1
BLYNK_WRITE(V30){
  float a = param.asFloat();
  obj1.setBorder("lowGroundHum", a, 1);
  obj1.saveBordersToEEPROM(1, "lowGroundHum");
}
// highGroundHum1
BLYNK_WRITE(V31){
  float a = param.asFloat();
  obj1.setBorder("highGroundHum", a, 1);
  obj1.saveBordersToEEPROM(1, "highGroundHum");
}
// lowGroundTemp1
BLYNK_WRITE(V32){
  float a = param.asFloat();
  obj1.setBorder("lowGroundTemp", a, 1);
  obj1.saveBordersToEEPROM(1, "lowGroundTemp");
}
// highGroundTemp1
BLYNK_WRITE(V33){
  float a = param.asFloat();
  obj1.setBorder("highGroundTemp", a, 1);
  obj1.saveBordersToEEPROM(1, "highGroundTemp");
}
// lowAirHum1
BLYNK_WRITE(V34){
  float a = param.asFloat();
  obj1.setBorder("lowAirHum", a, 1);
  obj1.saveBordersToEEPROM(1, "lowAirHum");
}
// highAirHum1
BLYNK_WRITE(V35){
  float a = param.asFloat();
  obj1.setBorder("highAirHum", a, 1);
  obj1.saveBordersToEEPROM(1, "highAirHum");
}
// lowAirTemp1
BLYNK_WRITE(V36){
  float a = param.asFloat();
  obj1.setBorder("lowAirTemp", a, 1);
  obj1.saveBordersToEEPROM(1, "lowAirTemp");
}
// highAirTemp1
BLYNK_WRITE(V37){
  float a = param.asFloat();
  obj1.setBorder("highAirTemp", a, 1);
  obj1.saveBordersToEEPROM(1, "highAirTemp");
}
// lowLightLevel1
BLYNK_WRITE(V38){
  float a = param.asFloat();
  obj1.setBorder("lowLightLevel", a*10, 1);
  obj1.saveBordersToEEPROM(1, "lowLightLevel");
}
// highLightLevel1
BLYNK_WRITE(V39){
  float a = param.asFloat();
  obj1.setBorder("highLightLevel", a*10, 1);
  obj1.saveBordersToEEPROM(1, "highLightLevel");
}

// GROUP 2
// lowGroundHum2
BLYNK_WRITE(V40){
  float a = param.asFloat();
  obj1.setBorder("lowGroundHum", a, 2);
  obj1.saveBordersToEEPROM(2, "lowGroundHum");
}
// highGroundHum2
BLYNK_WRITE(V41){
  float a = param.asFloat();
  obj1.setBorder("highGroundHum", a, 2);
  obj1.saveBordersToEEPROM(2, "highGrundHum");
}
// lowGroundTemp2
BLYNK_WRITE(V42){
  float a = param.asFloat();
  obj1.setBorder("lowGroundTemp", a, 2);
  obj1.saveBordersToEEPROM(2,"lowGroundTemp");
}
// highGroundTemp2
BLYNK_WRITE(V43){
  float a = param.asFloat();
  obj1.setBorder("highGroundTemp", a, 2);
  obj1.saveBordersToEEPROM(2, "highGroundTemp");
}
// lowAirHum2
BLYNK_WRITE(V44){
  float a = param.asFloat();
  obj1.setBorder("lowAirHum", a, 2);
  obj1.saveBordersToEEPROM(2, "highAirHum");
}
// highAirHum2
BLYNK_WRITE(V45){
  float a = param.asFloat();
  obj1.setBorder("highAirHum", a, 2);
  obj1.saveBordersToEEPROM(2, "highAirHum");
}
// lowAirTemp2
BLYNK_WRITE(V46){
  float a = param.asFloat();
  obj1.setBorder("lowAirTemp", a, 2);
  obj1.saveBordersToEEPROM(2, "lowAirTemp");
}
// highAirTemp2
BLYNK_WRITE(V47){
  float a = param.asFloat();
  obj1.setBorder("highAirTemp", a, 2);
  obj1.saveBordersToEEPROM(2, "highAirTemp");
}
// lowLightLevel2
BLYNK_WRITE(V48){
  float a = param.asFloat();
  obj1.setBorder("lowLightLeve", a*10, 2);
  obj1.saveBordersToEEPROM(2, "lowLightLevel");
}
// highLightLevel2
BLYNK_WRITE(V49){
  float a = param.asFloat();
  obj1.setBorder("HighLightLevel", a*10, 2);
  obj1.saveBordersToEEPROM(2, "highLightLevel");
}

void setup() {
  EEPROM.begin(25); // Init 25 bytes of EEPROM
  Wire.begin();        // Join I2C bus
  pcf_1.begin();       // Connect PCF8574_1 pin extension
  pcf_2.begin();       // Connect PCF8574_2 pin extension
  Serial.begin(115200);  // start serial for output
  obj1.restoreBordersFromEEPROM(1);
  // obj1.restoreBordersFromEEPROM(2);
  // Blynk.begin(auth, ssid, pass);
  Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,106), 8080);
  // packetData data[slavesNumber]; 

}

void loop() {
  
  // saveEEPROM.run();
  obj1.showBorders(1);
  delay(1000);
  
  // obj1.showBorders(2);
  
  Blynk.run();  
  obj1.useRelays();
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
          // Serial.println("Error no such relay. Requered number :" + String(r1.number));
          break;
      }
}


// Функция для опроса всех блоков сенсоров разом
// 
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


