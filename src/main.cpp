
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"

RTC_PCF8523 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const int chipSelect = 33;


#define CHANNEL_COUNT 4
const int Analog_channel_pin[CHANNEL_COUNT]= {15,34,39,36};
int ADC_values[CHANNEL_COUNT];
double voltage_value = 0; 
double a = 14.0/1305.0;
double b = 10 -(a *775);


String fileName;
bool error = false;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }
  
  if (! rtc.initialized() || rtc.lostPower()) {
    Serial.println("RTC is NOT initialized, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    //
    // Note: allow 2 seconds after inserting battery or applying external power
    // without battery before calling adjust(). This gives the PCF8523's
    // crystal oscillator time to stabilize. If you call adjust() very quickly
    // after the RTC is powered, lostPower() may still return true.
  }
  rtc.start();
  DateTime now = rtc.now();

  fileName = (String)"/" +now.year() + "_" + now.month() + "_" + now.day() + "_" + now.hour() + "_" + now.minute() + "_" + now.second()+".csv";
  
  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    while (1);
  }
  Serial.println("sd card initialized.");

  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);
  delay(3000);
  digitalWrite(LED_BUILTIN,LOW);
  
}

void loop() {
  //read data - fast loop:
   for (int i = 0 ; i<CHANNEL_COUNT ; i++){

      ADC_values[i] = analogRead(Analog_channel_pin[i]);
      
      if (ADC_values[i]<1920){ //value that indicates a voltage drop below 22v
        error = true; //raise voltage drop event flag
        //Serial.printf("channel %d: ADC VALUE = %d \n",i,ADC_values[i]);
        //delay(100);
      }
   }

    if (error){
      DateTime now = rtc.now();
      String dataline = (String)now.year() + "/" + now.month() + "/" + now.day() + "-" +now.hour() + ":" + now.minute() + ":" + now.second() + " , " + String(a*ADC_values[0]+b) + " , " + String(a*ADC_values[1]+b) + " , " + String(a*ADC_values[2]+b) +" , " + String(a*ADC_values[3]+b);
      Serial.println (dataline.c_str());
      //print to file:
      File dataFile = SD.open(fileName.c_str(), FILE_APPEND);
    
      // if the file is available, write to it:
      if (dataFile) {
        dataFile.println(dataline);
        dataFile.close();
        // print to the serial port too:
        Serial.println(dataline);
      } else {    // if the file isn't open, pop up an error:
        Serial.print("error opening ");
        Serial.println(fileName.c_str());
      }
      //turn led ON
      digitalWrite(LED_BUILTIN,HIGH); //stays high untill next reset
      error = false;
    }
}