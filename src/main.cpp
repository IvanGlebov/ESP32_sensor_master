// MASTER
#include <Arduino.h>

#define BLYNK_PRINT Serial


#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
#include <Wire.h>
#include <EEPROM.h>
#include "PCF8574.h"


#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

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

enum time { dayTime, night};
enum modes {automatic, manual, timeControlled, alert};
enum lightModes {timed = 0, timed_with_light_level};
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
  float groundHumDay;
  float groundHumNight;
  float groundTempDay;
  float groundTempNight;
  float lowAirHumDay;
  float lowAirHumNight;
  float highAirHumDay;
  float highAirHumNight;
  float lowAirTempDay;
  float lowAirTempNight;
  float highAirTempDay;
  float highAirTempNight;
  float lightLevelDay;
  float lightLevelNight;
};
// Функция для сброса значений границ определённой структуры к стандартным(предустановленным)
void dropBorders(borderValues &b1){
  b1.groundHumDay = 0;
  b1.groundHumNight = 0;
  b1.groundTempDay = 0;
  b1.groundTempNight = 0;
  b1.lowAirHumDay = 0;
  b1.lowAirHumNight = 0;
  b1.highAirHumDay = 0;
  b1.highAirHumNight = 0;
  b1.lowAirTempDay = 0;
  b1.lowAirTempNight = 0;
  b1.highAirTempDay = 0;
  b1.highAirTempNight = 0;
  b1.lightLevelDay = 0;
  b1.lightLevelNight = 0;
}


// Класс описывающий одно конкретное реле
class relay{
  private:
    
    bool state;
    int virtualPin;
  public:
    int number;
    String name;


    relay(int setNumber, String relayName) {number = setNumber; name = relayName;};
    relay(int setNumber, String relayName, bool defaultState) { number = setNumber; name = relayName; state = defaultState; };
    relay(int setNumber, String relayName, int virtualPinNumber) { number = setNumber, name = relayName, virtualPin = virtualPinNumber; };
    void setBindpin(int setNumber){ number = setNumber; }
    void setState(bool newState){ state  = newState; }
    void printInfo(){ Serial.println("Relay with name " + name + " on pin " + String(number) + " is " + (state)? String(true) : String(false)); }
    void on(){ state = true;  }
    void off(){ state = false;  }
    bool returnState(){ return state; }
    int getVPinNumber(){ return virtualPin; }
};




relay pump04_1 = relay(1, "Pump0.4Kv-1", 10); // Помпа капельного полива

relay valve1_1 = relay(2, "Valve1-1", 11); // Вентиль верхней аэрации блока 1
relay valve1_2 = relay(3, "Valve1-2", 12); // Вентиль верхней аэрации блока 2

relay valve2_1 = relay(4, "Valve2-1", 13); // Вентиль нижней аэрации блока 1
relay valve2_2 = relay(5, "Valve2-2", 14); // Вентиль нижней аэрации блока 2

relay light1_1 = relay(6, "Light1Kv-1", 15); // Основное освещение блока 1
relay light1_2 = relay(7, "Light1Kv-2", 16); // Основное отвещение блока 2
relay light01_1 = relay(8, "Light0.1KV-1", 17); // Длинный красный свет блока 1
relay light01_2 = relay(9, "Light0.1Kv-2", 18); // Длинный красный свет блока 2
relay distrif1_1 = relay(10, "Distrificator1Kv-1", 19); // Вентилятор 1
relay distrif1_2 = relay(11, "Distrificator1Kv-1", 20); // Вентилятор 2

relay steamgen1_1 = relay(12, "SteamGenerator1Kv-1", 21); // Парогенератор 1
relay steamgen1_2 = relay(13, "SteamGenerator1Kv-2", 22); // Парогенератор 2

relay heater1_1 = relay(14, "Hearet1Kv-1", 23); // Отопление 1
relay heater1_2 = relay(15, "Heater1kv_2", 24); // Отопление 2
// relay siod1_1 = relay(14, "SIOD1Kv");


// Функция для применения значений реле
void setRelay(relay r1);
void callRelays();


class workObj{
  private:
    int mode;
    int lightModeMain1, lightModeMain2;
    long mainLightOn_1, mainLightOn_2;
    long mainLightOff_1, mainLightOff_2;
    long aerTopStartTime_1, aerTopStartTime_2; // Точка отсчёта верхней аэрации обоих блоков
    long aerDownStartTime_1, aerDownStartTime_2; // Точка отсчёта нижней аэрации обоих блоков
    
    int timeNow; // Текущее время в секундах с начала дня. Время тянется с Blynk
    packetData sensors1, sensors2, sensors3;
    borderValues borders[2];
  public:

    int airTempFlag, airHumFlag; // Флаги для хранения значений из вызванных функций airTempCheck() и airHumCheck()

    int n1_1, n1_2, m1_1, m1_2; // Длительность включения и выключения верхней и нижней аэрации в блоке 1
    int n2_1, n2_2, m2_1, m2_2; // Длительность включения и выключения верхней и нижней аэрации в блоке 2
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
    
    // Запись времени старта отсчёта работы
    void setAerTime(String position, int block, long startTime){
      if (position == "top"){
        if (block == 1){
          aerTopStartTime_1 = startTime;
        }
        if (block == 2){
          aerTopStartTime_2 = startTime;
        }
      }
      if (position == "down"){
        if (block == 1){
          aerDownStartTime_1 = startTime;
        }
        if (block == 2){
          aerDownStartTime_2 = startTime;
        }
      }
    }

    // Flags
    // start / end 
    void setMainLightTime(String timeType, int block, long timeValue){
      if (timeType == "start"){
        if (block == 1){
          mainLightOn_1 = timeValue;
        }
        if (block == 2){
          mainLightOn_2 = timeValue;
        }
      }
      if (timeType == "end"){
        if (block == 1){
          mainLightOff_1 = timeValue;
        }
        if (block == 2){
          mainLightOff_2 = timeValue;
        }
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

    */
    void setBorder(String border, float value, int bordersGroup){
      
      if (border == "groundHumDay"){
        borders[bordersGroup].groundHumDay = value;
      } else if (border == "groundHumNight"){
        borders[bordersGroup].groundHumNight = value;
      } else if (border == "groundTempDay"){
        borders[bordersGroup].groundTempDay = value;
      } else if (border == "groundTempNight"){
        borders[bordersGroup].groundTempNight = value;
      } else if (border == "lowAirHumDay"){
        borders[bordersGroup].lowAirHumDay = value;
      } else if (border == "lowAirHumNight"){
        borders[bordersGroup].lowAirHumNight = value;
      } else if (border == "highAirHumDay"){
        borders[bordersGroup].highAirHumDay = value;
      } else if (border == "highAirHumNight"){
        borders[bordersGroup].highAirHumNight = value;
      } else if (border == "lowAirTempDay"){
        borders[bordersGroup].lowAirTempDay = value;
      } else if (border == "lowAirTempNight"){
        borders[bordersGroup].lowAirTempNight = value;
      } else if (border == "highAirTempDay"){
        borders[bordersGroup].highAirTempDay = value;
      } else if (border == "highAirTempNight"){
        borders[bordersGroup].highAirTempNight = value;
      } else if (border == "lightLevelDay"){
        borders[bordersGroup].lightLevelDay = value;
      } else if (border == "lightLevelNight"){
        borders[bordersGroup].lightLevelNight = value;
      }

    }
    // Функция для установки всех значений границ разом
    // void setAllBordersGroup(float lowGroundHum, float highGroundHum, float lowGroundTemp, 
    //                    float highGroundTemp, float lowAirHum, float highAirHum, 
    //                    float lowAirTemp, float highAirTemp, float lowLightLevel, 
    //                    float highLightLevel, int bordersGroup)
    // {
    //   borders[bordersGroup].lowGroundHum = lowGroundHum;
    //   borders[bordersGroup].highGroundHum = highGroundHum;
    //   borders[bordersGroup].lowGroundTemp = lowGroundTemp;
    //   borders[bordersGroup].highGroundTemp = highGroundTemp;
    //   borders[bordersGroup].lowAirHum = lowAirHum;
    //   borders[bordersGroup].highAirHum = highAirHum;
    //   borders[bordersGroup].lowAirTemp = lowAirTemp;
    //   borders[bordersGroup].highAirTemp = highAirTemp;
    //   borders[bordersGroup].lowLightLevel = lowLightLevel;
    //   borders[bordersGroup].highLightLevel = highLightLevel;
    // }
    
    // Функция для переключения состояний реле
    void useRelays() {
      // Relays are working in inverted mode - 0 is ON, 1 is OFF
      setRelay(pump04_1); // Помпа капельного полива. Общая на 2 блока
      setRelay(valve1_1); // Вентиль верхней аэрации блока 1
      setRelay(valve1_2); // Вентиль верхней аэрации блока 2
      setRelay(valve2_1); // Вентиль нижней аэрации блока 1
      setRelay(valve2_2); // Вентиль нижней аэрации блока 2
      // setRelay(light3_1);
      setRelay(light1_1); // Основное освещение в блоке 1
      setRelay(light1_2); // Основное освещение в блоке 2
      setRelay(light01_1); // Дальнее красное освещение в блоке 1
      setRelay(light01_2); // Дальнее красное освещение в блоке 2
      setRelay(distrif1_1); // Вентиляция в блоке 1
      setRelay(distrif1_2); // Вентиляция в блоке 2
      setRelay(steamgen1_1); // Увлажнитель в блоке 1
      setRelay(steamgen1_2); // Увлажнитель в блоке 2
      setRelay(heater1_1);  // Отопрелние в блоке 1
      // setRelay(heater1_2);  // Отобление в блоке 2
    }
    
    // Функция для обработки автоматического полива
    void groundHumidControl(){
      
    }
    // Функция для автоматической обработки автоматики отдельных блоков
    void lightControl();

    // Функция для включения или выключения полива отдельного блока


    // Функция для включения или выключения освещения отдельного блока
    
    // Функция обработки температуры воздуха. 
    // Если температура низкая, то включается отопление,
    // если температура высокая, то включается вентиляция конкретного блока
    // теплицы (из 2-х)
    // Возвращает 2 если функция занята отоплением и 1 если вентиляцией.
    // 0 возвращается только если функция ничего не сделала
    int airTempCheck(){
      // Переменная для хранения состояния условий. Если возвращается "1", то активна вентиляция. Если "2", то активно отопление
      // если возвращается "0", то с устройсовом исполнителем можно взаимодействовать спокойно
      int returnFlag = 0;
      
      // Если сейчас днейной режим, то используем дневные границы
      if (timeNow == dayTime){
        // Нагрев если маленькая температура
        // Блок 1
        if (sensors1.airTemp < borders[0].lowAirTempDay){
          heater1_1.on();
          returnFlag = 2;
        }
        else{
          heater1_1.off();
          returnFlag = (returnFlag == 2) ? 2 : 0;
        }

        // Блок 2
        if (sensors2.airTemp < borders[1].lowAirTempDay){
          heater1_2.on();
          returnFlag = 2;
        }
        else{
          heater1_2.off();
          returnFlag = (returnFlag == 2) ? 2 : 0;
        }

        // Включение вентиляции если высокая температура
        // Блок 1
        if (sensors1.airTemp > borders[0].highAirTempDay){
          distrif1_1.on();
          returnFlag = 1;
        } // Если вентиляция не занята другой функцией, то выключаем её
        else if (airHumFlag != 1){
          distrif1_1.off();
          returnFlag = (returnFlag == 1) ? 1 : 0;
        }
        //  Блок 2
        if (sensors2.airTemp > borders[1].highAirTempDay){
          distrif1_2.on();
          returnFlag = 1;
        } // Если вентиляция не занята другой функцией, то выключаем её
        else if (airHumFlag != 1) {
          distrif1_2.off();
          returnFlag = (returnFlag == 1) ? 1 : 0;
        }
      }

      // Если сейчас ночной режим, то используем ночные границы
      if (timeNow == night){
        // Нагрев если маленькая температура
        // Блок 1
        if (sensors1.airTemp < borders[0].lowAirTempNight){
          heater1_1.on();
          returnFlag = 2;
        }
        else{
          heater1_1.off();
          returnFlag = (returnFlag == 2) ? 2 : 0;
        }

        // Блок 2
        if (sensors2.airTemp < borders[1].lowAirTempNight){
          heater1_2.on();
          returnFlag = 2;
        }
        else{
          heater1_2.off();
          returnFlag = (returnFlag == 2) ? 2 : 0;
        }

        // Включение вентиляции если высокая температура
        // Блок 1
        if (sensors1.airTemp > borders[0].highAirTempNight){
          distrif1_1.on();
          returnFlag = 1;
        }
        else if (airHumFlag != 1) {
          distrif1_1.off();
          returnFlag = (returnFlag == 1) ? 1 : 0;
        }
        //  Блок 2
        if (sensors2.airTemp > borders[1].highAirTempNight){
          distrif1_2.on();
          returnFlag = 1;
        }
        else if (airHumFlag != 1) {
          distrif1_2.off();
          returnFlag = (returnFlag == 1) ? 1 : 0;
        }
      }
      
      return returnFlag;
    }
    
    // Возвращает 1 если функция занята вентиляцией и 2 если увлежнением.
    // 0 возвращается только если функция ничего не сделала
    int airHumCheck(){
      // Переменная для хранения состояния условий. Если возвращается "1", то активно каоке-то условие и выключать не надо
      // если возвращается "0", то с устройсовом исполнителем можно взаимодействовать спокойно
      int returnFlag = 0;

      // timeNow = dayTime;
      // Если сейчас днейной режим, то используем дневные границы
      if (timeNow == dayTime){
        // Если низкая влажность, то включить парогенератор
        // Блок 1
        if (sensors1.airHum < borders[0].lowAirHumDay){
          steamgen1_1.on();
          returnFlag = 1;
        }
        else{
          steamgen1_1.off();
          returnFlag = (returnFlag == 1) ? 1 : 0;
        }
        // Блок 2
        if (sensors2.airHum < borders[1].lowAirHumDay){
          steamgen1_2.on();
          returnFlag = 1;
        }
        else{
          steamgen1_2.off();
          returnFlag = (returnFlag == 1) ? 1 : 0;
        }


        // Если высокая влажность, то включить вентиляцию. Если влажность вернулась в норму и другое устройство не задействует 
        // исполнитель, то выключить вентиляцию
        // Блок 1

        // ТУТ БЛЯДСКАЯ ОШИБКА УБИВАЮЩАЯ ПРОЦЕСС
        // пофикшено :)
        // sensors1.airHum = 20;
        if (sensors1.airHum > borders[0].highAirHumDay){
          distrif1_1.on();
          returnFlag = 1;
          // Serial.println("AirTempCheck : " + String(airTempFlag));
        }
        else if (airTempFlag != 1) {
          distrif1_1.off();
          // Если возвращаемое значение уже успело стать равным 1, то не надо его обнулять и ломать логику.
          // returnFlag = (returnFlag == 1) ? 1 : 0;
          
        }
        // Блок 2
        if (sensors2.airHum > borders[1].highAirHumDay){
          distrif1_2.on();
          returnFlag = 1;
        } 
        else if (airTempFlag != 1){
          distrif1_2.off();
          returnFlag = (returnFlag == 1) ? 1 : 0;
        }
      
      }

      // Если сейчас ночной режим, то используем ночные границы
      if (timeNow == night){
        // Если низкая влажность, то включить парогенератор
        // Блок 1
        if (sensors1.airHum < borders[0].lowAirHumNight){
          steamgen1_1.on();
          returnFlag = 2;
        }
        else{
          steamgen1_1.off();
          returnFlag = (returnFlag == 2) ? 2 : 0;
        }
        // Блок 2
        if (sensors2.airHum < borders[1].lowAirHumNight){
          steamgen1_2.on();
          returnFlag = 2;
        }
        else{
          steamgen1_2.off();
          returnFlag = (returnFlag == 2) ? 2 : 0;
        }

        // Если высокая влажность, то включить вентиляцию. Если влажность вернулась в норму и другое устройство не задействует 
        // исполнитель, то выключить вентиляцию
        // Блок 1
        if (sensors1.airHum > borders[0].highAirHumNight){
          distrif1_1.on();
          returnFlag = 1;
        }
        else if (airTempFlag != 1) {
          distrif1_1.off();
          // Если возвращаемое значение уже успело стать равным 1, то не надо его обнулять и ломать логику.
          returnFlag = (returnFlag == 1) ? 1 : 0;
          // 
        }
        // Блок 2
        if (sensors2.airHum > borders[1].highAirHumNight){
          distrif1_2.on();
          returnFlag = 1;
        } 
        else if (airTempFlag != 1){
          distrif1_2.off();
          returnFlag = (returnFlag == 1) ? 1 : 0;
        }
      
        return returnFlag;
      }
      

    }
    // Функция для сохранения всех значений границ в энергонезависимую память

    void saveBordersToEEPROM(int bordersGroup, String border){
      if (border == "groundHumDay")
        EEPROM.write(0 * bordersGroup-1, borders[bordersGroup].groundHumDay);
      if (border == "groundHumNight")
        EEPROM.write(1 * bordersGroup-1, borders[bordersGroup].groundHumNight);
      if (border == "groundTempDay")
        EEPROM.write(2 * bordersGroup-1, borders[bordersGroup].groundTempDay);
      if (border == "groundTempNight")
        EEPROM.write(3 * bordersGroup-1, borders[bordersGroup].groundTempNight);
      if (border == "lowAirHumDay")
        EEPROM.write(4 * bordersGroup-1, borders[bordersGroup].lowAirHumDay);
      if (border == "lowAirHumNight")
        EEPROM.write(5 * bordersGroup-1, borders[bordersGroup].lowAirHumNight);
      if (border == "highAirHumDay")
        EEPROM.write(6 * bordersGroup-1, borders[bordersGroup].highAirHumDay);
      if (border == "highAirHumNight")
        EEPROM.write(7 * bordersGroup-1, borders[bordersGroup].highAirHumNight);
      if (border == "lowAirTempDay")
        EEPROM.write(8 * bordersGroup-1, borders[bordersGroup].lowAirTempDay);
      if (border == "lowAirTempNight")
        EEPROM.write(9 * bordersGroup-1, borders[bordersGroup].lowAirTempNight);
      if (border == "highAirTempDay")
        EEPROM.write(10 * bordersGroup-1, borders[bordersGroup].highAirTempDay);
      if (border == "highAirTempNight")
        EEPROM.write(11 * bordersGroup-1, borders[bordersGroup].highAirTempNight);
      if (border == "lightLevelDay")
        EEPROM.write(12 * bordersGroup-1, borders[bordersGroup].lightLevelDay);
      if (border == "lightLevelNight")
        EEPROM.write(13 * bordersGroup-1, borders[bordersGroup].lightLevelNight);
      EEPROM.commit();
    }
    // Функция для чтения всех значений границ из энергонезависимой памяти
    void restoreBordersFromEEPROM(int bordersGroup){
      borders[bordersGroup].groundHumDay = (EEPROM.read(0 + bordersGroup) == 255) ? 0 : EEPROM.read(0 + bordersGroup);
      borders[bordersGroup].groundHumNight = (EEPROM.read(1 + bordersGroup) == 255) ? 0 : EEPROM.read(1 + bordersGroup);
      borders[bordersGroup].groundTempDay = (EEPROM.read(2 + bordersGroup) == 255) ? 0 : EEPROM.read(2 + bordersGroup);
      borders[bordersGroup].groundTempNight = (EEPROM.read(3 + bordersGroup) == 255) ? 0 : EEPROM.read(3 + bordersGroup);
      borders[bordersGroup].lowAirHumDay = (EEPROM.read(4 + bordersGroup) == 255) ? 0 : EEPROM.read(4 + bordersGroup);
      borders[bordersGroup].lowAirHumNight = (EEPROM.read(5 + bordersGroup) == 255) ? 0 : EEPROM.read(5 + bordersGroup);
      borders[bordersGroup].highAirHumDay = (EEPROM.read(6 + bordersGroup) == 255) ? 0 : EEPROM.read(6 + bordersGroup);
      borders[bordersGroup].highAirHumNight = (EEPROM.read(7 + bordersGroup) == 255) ? 0 : EEPROM.read(7 + bordersGroup);
      borders[bordersGroup].lowAirTempDay = (EEPROM.read(8 + bordersGroup) == 255) ? 0 : EEPROM.read(8 + bordersGroup);
      borders[bordersGroup].lowAirTempNight = (EEPROM.read(9 + bordersGroup) == 255) ? 0 : EEPROM.read(9 + bordersGroup);
      borders[bordersGroup].highAirTempDay = (EEPROM.read(10 + bordersGroup) == 255) ? 0 : EEPROM.read(10 + bordersGroup);
      borders[bordersGroup].highAirTempNight = (EEPROM.read(11 + bordersGroup) == 255) ? 0 : EEPROM.read(11 + bordersGroup);
      borders[bordersGroup].lightLevelDay = (EEPROM.read(12 + bordersGroup) == 255) ? 0 : EEPROM.read(12 + bordersGroup);
      borders[bordersGroup].lightLevelNight = (EEPROM.read(13 + bordersGroup) == 255) ? 0 : EEPROM.read(13 + bordersGroup);
    }
    // Функция для смены режима финкционирования
    void changeModeTo(int changeToMode)
    {
      mode = changeToMode;
    }
    // Функция для смены режима работы основного освещения в разных блоках
    void changeMainLightMode(int toMode, int block){
      if (block == 1){
        lightModeMain1 = toMode;
      }
      if (block == 2){
        lightModeMain2 = toMode;
      }
    }

    // Функция для вывода в кончоль всех граничных значений
    void showBorders(int bordersGroup){
      Serial.println("-------------------------------------------");
      Serial.println("Group #" + String(bordersGroup));
      Serial.println("groundHumDay" + String(bordersGroup) + " :" + String(borders[bordersGroup].groundHumDay));
      Serial.println("groundHumNight" + String(bordersGroup) + " :" + String(borders[bordersGroup].groundHumNight));
      Serial.println("groundTempDay" + String(bordersGroup) + " :" + String(borders[bordersGroup].groundTempDay));
      Serial.println("groundTempNight" + String(bordersGroup) + " :" + String(borders[bordersGroup].groundTempNight));
      Serial.println("lowAirHumDay" + String(bordersGroup) + " :" + String(borders[bordersGroup].lowAirHumDay));
      Serial.println("lowAirHumNight" + String(bordersGroup) + " :" + String(borders[bordersGroup].lowAirHumNight));
      Serial.println("highAirHumDay" + String(bordersGroup) + " :" + String(borders[bordersGroup].highAirHumDay));
      Serial.println("highAirHumNight" + String(bordersGroup) + " :" + String(borders[bordersGroup].highAirHumNight));
      Serial.println("lowAirTempDay" + String(bordersGroup) + " :" + String(borders[bordersGroup].lowAirTempDay));
      Serial.println("lowAirTempNight" + String(bordersGroup) + " :" + String(borders[bordersGroup].lowAirTempNight));
      Serial.println("highAirTempDay" + String(bordersGroup) + " :" + String(borders[bordersGroup].highAirTempDay));
      Serial.println("highAirTempNight" + String(bordersGroup) + " :" + String(borders[bordersGroup].highAirTempNight));
      Serial.println("lightLevelDay" + String(bordersGroup) + " :" + String(borders[bordersGroup].lightLevelDay));
      Serial.println("lightLevelNight" + String(bordersGroup) + " :" + String(borders[bordersGroup].lightLevelNight));
      Serial.println("-------------------------------------------");
    }
    int getMode(){ return mode; }
    int getMainLightMode(int block){
      if (block == 1) return lightModeMain1;
      if (block == 2) return lightModeMain2;
    }
    // Функция для обновления переменной времени тянущейся с Blynk
    void calculateTimeBlynk(){
      timeNow = hour()*3600 + minute()*60 + second();
    }
    // Функция для получения значения переменной времени Blynk
    long getTimeBlynk(){
      return timeNow;
    }
    long getMainLightTime(String timeType, int block){
      if (timeType == "start"){
        if (block == 1){
          return mainLightOn_1;
        }
        if (block == 2){
          return mainLightOn_2;
        }
      }
      if (timeType == "end"){
        if (block == 1){
          return mainLightOff_1;
        }
        if (block == 2){
          return mainLightOff_2;
        }
      }
    }

};


void workObj::lightControl() {
  
  if (getMode() == automatic){
    // Режим 1 Блока 1
    if (getMainLightMode(1) == timed){
      // Включение освещения по времени
      if (getTimeBlynk() == getMainLightTime("start", 1)){
        light1_1.on();
      }
      // Выключение освещения по времени
      if (getTimeBlynk() == getMainLightTime("end", 1)){
        light1_1. off();
      }
    }
    // Режим 1 блока 2
    if (getMainLightMode(2) == timed){
      // Включение освещения по времени
      if (getTimeBlynk() == getMainLightTime("start", 2)){
        light1_2.on();
      }
      // Выключение освещения по времени
      if (getTimeBlynk() == getMainLightTime("end", 2)){
        light1_2.off();
      }
    }
  
  }



}


// Прототип функции разбора пакета данных с блока сенсоров
void parsePackage(packetData&, String);
// Прототип функции отображения данных пакета в консоль
void showPackage(packetData);

// Зачаток функции для опроса всех блоков сенсоров разом
void slavesQuery();

workObj obj1(1, false);


WidgetRTC rtcBlynk;

BLYNK_CONNECTED() {
  rtcBlynk.begin();
}

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
// Вентиль верхней аэрации блока 1
BLYNK_WRITE(V11){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    valve1_1.setState( (a == 0)? true : false );
}

// Реле 3
// Вентиль верхней аэрации блока 2
BLYNK_WRITE(V12){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    valve1_2.setState( (a == 0)? true : false );
}

// Реле 4
// Вентиль нижней аэрации блока 1
BLYNK_WRITE(V13){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    valve2_1.setState( (a == 0)? true : false );
}

// Реле 5
// Вентиль нижней аэрации блока 2
BLYNK_WRITE(V14){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    valve2_2.setState( (a == 0)? true : false );
}

// Реле 6
// Основное освещение блока 1
BLYNK_WRITE(V15){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    light1_1.setState( (a == 0)? true : false );
}

// Реле 7
// Основное освещение блока 2
BLYNK_WRITE(V16){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    light1_2.setState( (a == 0)? true : false );
}

// Реле 8
// Длинный красный свет блока 1
BLYNK_WRITE(V17){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    light01_1.setState( (a == 0)? true : false );
}

// Реле 9
// Длинный красный свет блока 2
BLYNK_WRITE(V18){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    light01_2.setState( (a == 0)? true : false );
}

// Реле 10
// Вентилятор блока 1
BLYNK_WRITE(V19){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    distrif1_1.setState( (a == 0)? true : false );
}

// Реле 11
// Вентилятор блока 2
BLYNK_WRITE(V20){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    distrif1_2.setState( (a == 0)? true : false );
}

// Реле 12
// Парогенератор блока 1
BLYNK_WRITE(V21){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    steamgen1_1.setState( (a == 0)? true : false );
}

// Реле 13
// Парогенератор блока 2
BLYNK_WRITE(V22){
  int a = param.asInt();
  if (obj1.getMode() == manual)
    steamgen1_2.setState( (a == 0)? true : false );
}

// Реле 14
// Обогреватель блока 1
BLYNK_WRITE(V23){
  int a = param.asInt();
  if (obj1.getMode() == manual){
    heater1_1.setState( (a == 0)? true : false );
    Serial.println("Heater 1 is now " + String(heater1_1.returnState()));
  }
}

// Реле 15
// Обогреватель блока 2
BLYNK_WRITE(V24){
  int a = param.asInt();
  if (obj1.getMode() == manual){
    heater1_2.setState( (a == 0)? true : false );
    Serial.println("Heater 2 is now " + String(heater1_2.returnState()));
  }
  
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
// V30 -> V43
// Group2
// V50 -> V63

// GROUP 1
// groundHumDay1
BLYNK_WRITE(V30){
  float a = param.asFloat();
  obj1.setBorder("groundHumDay", a, 1);
  obj1.saveBordersToEEPROM(1, "groundHumDay");
}
// groundHumNight1
BLYNK_WRITE(V31){
  float a = param.asFloat();
  obj1.setBorder("groundHumNight", a, 1);
  obj1.saveBordersToEEPROM(1, "groundHumNight");
}
// groundTempDay1
BLYNK_WRITE(V32){
  float a = param.asFloat();
  obj1.setBorder("groundTempDay", a, 1);
  obj1.saveBordersToEEPROM(1, "groundTempDay");
}
// groundTempNight1
BLYNK_WRITE(V33){
  float a = param.asFloat();
  obj1.setBorder("groundTempNight", a, 1);
  obj1.saveBordersToEEPROM(1, "groundTempNight");
}
// lowAirHumDay1
BLYNK_WRITE(V34){
  float a = param.asFloat();
  obj1.setBorder("lowAirHumDay", a, 1);
  obj1.saveBordersToEEPROM(1, "lowAirHumDay");
}
// lowAirHumNight1
BLYNK_WRITE(V35){
  float a = param.asFloat();
  obj1.setBorder("lowAirHumNight", a, 1);
  obj1.saveBordersToEEPROM(1, "lowAirHumNight");
}
// highAirHumDay1
BLYNK_WRITE(V36){
  float a = param.asFloat();
  obj1.setBorder("highAirHumDay", a, 1);
  obj1.saveBordersToEEPROM(1, "highAirHumDay");
}
// highAirHumNight1
BLYNK_WRITE(V37){
  float a = param.asFloat();
  obj1.setBorder("highAirHumNight", a, 1);
  obj1.saveBordersToEEPROM(1, "highAirHumNight");
}
// lowAirTempDay1
BLYNK_WRITE(V38){
  float a = param.asFloat();
  obj1.setBorder("lowAirTempDay", a, 1);
  obj1.saveBordersToEEPROM(1, "lowAirTempDay");
}
// lowAirTempNight1
BLYNK_WRITE(V39){
  float a = param.asFloat();
  obj1.setBorder("lowAirTempNight", a, 1);
  obj1.saveBordersToEEPROM(1, "lowAirTempNight");
}
// highAirTempDay1
BLYNK_WRITE(V40){
  float a = param.asFloat();
  obj1.setBorder("highAirTempDay", a, 1);
  obj1.saveBordersToEEPROM(1, "highAirTempDay");
}
// highAirTempNight1
BLYNK_WRITE(V41){
  float a = param.asFloat();
  obj1.setBorder("highAirTempNight", a, 1);
  obj1.saveBordersToEEPROM(1, "highAirTempNight");
}
// lightLevelDay1
BLYNK_WRITE(V42){
  float a = param.asFloat();
  obj1.setBorder("lightLevelDay", a, 1);
  obj1.saveBordersToEEPROM(1, "lightLevelDay");
}
// lightLevelNight1
BLYNK_WRITE(V43){
  float a = param.asFloat();
  obj1.setBorder("lightLevelNight", a, 1);
  obj1.saveBordersToEEPROM(1, "lightLevelNight");
}


// GROUP 2
// groundHumDay2
BLYNK_WRITE(V50){
  float a = param.asFloat();
  obj1.setBorder("groundHumDay", a, 2);
  obj1.saveBordersToEEPROM(2, "groundHumDay");
}
// groundHumNight2
BLYNK_WRITE(V51){
  float a = param.asFloat();
  obj1.setBorder("groundHumNight", a, 2);
  obj1.saveBordersToEEPROM(2, "groundHumNight");
}
// groundTempDay2
BLYNK_WRITE(V52){
  float a = param.asFloat();
  obj1.setBorder("groundTempDay", a, 2);
  obj1.saveBordersToEEPROM(2, "groundTempDay");
}
// groundTempNight2
BLYNK_WRITE(V53){
  float a = param.asFloat();
  obj1.setBorder("groundTempNight", a, 2);
  obj1.saveBordersToEEPROM(2, "groundTempNight");
}
// lowAirHumDay2
BLYNK_WRITE(V54){
  float a = param.asFloat();
  obj1.setBorder("lowAirHumDay", a, 2);
  obj1.saveBordersToEEPROM(2, "lowAirHumDay");
}
// lowAirHumNight2
BLYNK_WRITE(V55){
  float a = param.asFloat();
  obj1.setBorder("lowAirHumNight", a, 2);
  obj1.saveBordersToEEPROM(2, "lowAirHumNight");
}
// highAirHumDay2
BLYNK_WRITE(V56){
  float a = param.asFloat();
  obj1.setBorder("highAirHumDay", a, 2);
  obj1.saveBordersToEEPROM(2, "highAirHumDay");
}
// highAirHumNight1
BLYNK_WRITE(V57){
  float a = param.asFloat();
  obj1.setBorder("highAirHumNight", a, 1);
  obj1.saveBordersToEEPROM(1, "highAirHumNight");
}
// lowAirTempDay1
BLYNK_WRITE(V58){
  float a = param.asFloat();
  obj1.setBorder("lowAirTempDay", a, 1);
  obj1.saveBordersToEEPROM(1, "lowAirTempDay");
}
// lowAirTempNight2
BLYNK_WRITE(V59){
  float a = param.asFloat();
  obj1.setBorder("lowAirTempNight", a, 2);
  obj1.saveBordersToEEPROM(2, "lowAirTempNight");
}
// highAirTempDay2
BLYNK_WRITE(V60){
  float a = param.asFloat();
  obj1.setBorder("highAirTempDay", a, 2);
  obj1.saveBordersToEEPROM(2, "highAirTempDay");
}
// highAirTempNight2
BLYNK_WRITE(V61){
  float a = param.asFloat();
  obj1.setBorder("highAirTempNight", a, 2);
  obj1.saveBordersToEEPROM(2, "highAirTempNight");
}
// lightLevelDay2
BLYNK_WRITE(V62){
  float a = param.asFloat();
  obj1.setBorder("lightLevelDay", a, 2);
  obj1.saveBordersToEEPROM(2, "lightLevelDay");
}
// lightLevelNight2
BLYNK_WRITE(V63){
  float a = param.asFloat();
  obj1.setBorder("lightLevelNight", a, 2);
  obj1.saveBordersToEEPROM(2, "lightLevelNight");
}






// Режимы основного освещения блока 1
BLYNK_WRITE(V2){
  int a = param.asInt();
  // Записываем новое значение режима
  obj1.changeMainLightMode(a, 1);
}

// Режимы основного освещения блока 2
BLYNK_WRITE(V3){
  int a = param.asInt();
  // Записываем новое значение режима
  obj1.changeMainLightMode(a, 2);
}

// Время включения для режима 1 основного освещения блока 1
BLYNK_WRITE(V4){

  obj1.setMainLightTime("start", 1, param[0].asLong()); // Время старта
  }// Если автоматический режим и освещение в режиме 1, то освещение включается и выключается по таймеру
  
  // int a = param.asInt();
  // if ((obj1.getMode() == automatic) && (obj1.getMainLightMode(1) == timed)){
    // if (a == 1){
      // light1_1.on();
      // Serial.println("Main light 1 on");
    // }
    // if (a == 0){
      // light1_1.off();
    // }
    
  }
}

// Время выключения для режима 1 основного освещения блока 1
BLYNK_WRITE(V5){
  obj1.setMainLightTime("end", 1, param[0].asLong());
  if ((obj1.getMode() == automatic) && (obj1.getMainLightMode(1) == timed)){
    if (obj1.getTimeBlynk() == obj1.getMainLightTime("end", 2)){
      light1_1.off();
    }
  }

}
// Таймер для режима 1 основного освещения блока 2
BLYNK_WRITE(V6){
  int a = param.asInt();
  // Если автоматический режим и освещение в режиме 1, то освещение включается и выключается по таймеру
  if ((obj1.getMode() == automatic) && (obj1.getMainLightMode(2) == timed)){
    if (a == 1){
      light1_2.on();
    }
    if (a == 0){
      light1_2.off();
    }
    
  }
}



// Точка отсчёта верхней аэрации блока 1
BLYNK_WRITE(V44){
  obj1.setAerTime("top", 1, param.asLong());
}
// Верхняя аэрация блока 1 n
BLYNK_WRITE(V45){
  obj1.n1_1 = param.asInt();
}
// Верхняя аэрация блока 1 m
BLYNK_WRITE(V46){
  obj1.m1_1 = param.asInt();
}
// Точка отсчёта нижней аэрации блока 1
BLYNK_WRITE(V47){
  obj1.setAerTime("down", 1, param.asLong());
}
// Нижняя аэрация блока 1 n
BLYNK_WRITE(V48){
  obj1.n1_2 = param.asInt();
}
// Нижняя аэрация блока 1 m
BLYNK_WRITE(V49){
  obj1.m1_2 = param.asInt();
}


// Точка отсчёта верхней аэрации блока 2
BLYNK_WRITE(V64){
  obj1.setAerTime("top", 2, param.asLong());
}
// Верхняя аэрация блока 2 n
BLYNK_WRITE(V65){
  obj1.n2_1 = param.asInt();
}
// Верхняя аэрация блока 2 m
BLYNK_WRITE(V66){
  obj1.m2_1 = param.asInt();
}
// Точка отсчёта нижней аэрации блока 2
BLYNK_WRITE(V67){
  obj1.setAerTime("down", 2, param.asLong());
}
// Нижняя аэрация блока 2 n
BLYNK_WRITE(V68){
  obj1.n2_2 = param.asInt();
}
// Нижняя аэрация блока 2 m
BLYNK_WRITE(V69){
  obj1.m2_2 = param.asInt();
}


void setup() {
  // Очень плохое решение проблемы проседания питания при старте WiFi
  // Но другого у меня нет. Так что будет пока так.
  // Хорошо бы сделать отдельное питание на 3.3 линию в 3.4В
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);



  EEPROM.begin(30); // Init 30 bytes of EEPROM
  Wire.begin();        // Join I2C bus
  pcf_1.begin();       // Connect PCF8574_1 pin extension
  pcf_2.begin();       // Connect PCF8574_2 pin extension
  Serial.begin(115200);  // start serial for output
  obj1.restoreBordersFromEEPROM(1);
  obj1.restoreBordersFromEEPROM(2);
  // Blynk.begin(auth, ssid, pass);
  setSyncInterval(10 * 60); // Для виджета часов реального времени
  Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,106), 8080);
  // packetData data[slavesNumber]; 
  
}

void loop() {
  
  // saveEEPROM.run();
  // obj1.showBorders(1);
  // delay(1000);
  
  // obj1.showBorders(2);
  // heater1_1.printInfo();
  // heater1_2.printInfo();
  
  Blynk.run();  
  obj1.useRelays();
  if (obj1.getMode() == automatic){
    obj1.airHumFlag = obj1.airHumCheck();
    obj1.airTempFlag = obj1.airTempCheck();
    // obj1.groundHumidControl();
    // obj1.lightSeparateControl();
  }

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
      // Serial.println("pcf_2_p5 is now " + String(!r1.returnState()));
      break;
    case 15:
      pcf_2.write(6, !r1.returnState());
      Serial.println("pcf_2_p6 is now " + String(!r1.returnState()));
      break;
    case 16:
      pcf_2.write(7, !r1.returnState());
      Serial.println("pcf_2_p7 is now " + String(!r1.returnState()));
      break;
    default:
      Serial.println("Error no such relay. Requered number :" + String(r1.number));
      break;
  }
  // Отправка значения об изменении состояния реле.
  Blynk.virtualWrite(r1.getVPinNumber(), r1.returnState()); 
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


