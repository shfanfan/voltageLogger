
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"
#include "iomanip"

//i2c in esp32 D1 mini : SDA=GPIO 21, SCL=GPIO 22
//i2c in adafruit feather32 : SDA=GPIO 23, SCL=GPIO 22
#include "beeSplash.h"
#include <sstream>
#include <SPI.h> 
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

// #include <WiFi.h>
// #include <AsyncTCP.h>
// #include <ESPAsyncWebServer.h>
// #include <AsyncElegantOTA.h>
// const char* ssid = "vlogger";
// const char* password = "vlogger";
// AsyncWebServer server(80);

// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306* display;

bool prevCycleHadError=false;
long errorCounter = 0;
int currentError = 0;
String errors[] = {
  "all is OK",
  "error1",
  "error2",
  "error3",
  "SD error",
  "volt error"
};

#define SPLASH_DELAY_MS 3000

RTC_PCF8523 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const int chipSelect = 33;

int SDCardUsedPercent = 0;

float getBattVoltage(){
  return (analogRead(A13)*2/4096.0*3.3);
}


#define CHANNEL_COUNT 4
const int Analog_channel_pin[CHANNEL_COUNT]= {15,34,39,36}; //box1 setup. other boxes- worked: {15,34,39,36}; 13=A12
int ADC_values[CHANNEL_COUNT];
float Voltages_values[CHANNEL_COUNT];
double a1 = 14.0/1305.0;
double b1 = 10 -(a1 *775);
hw_timer_t * OLED_refreshTimer = NULL;
hw_timer_t * SD_CheckTimer = NULL;
bool shouldDisplay = false;
IPAddress IP(50, 50, 50, 50);

void writeLastReadingToOLED(){
  static int i = 0;
  i++;
  //Serial.print("sec1\n");
  display->clearDisplay();
  display->setCursor(0, 0); 
  std::ostringstream strs;
  strs << errors[currentError].c_str();

  display->setTextSize(2);
  display->write(strs.str().c_str());

  strs.clear();
  strs.seekp(0);
  display->setCursor(0,16);

  // strs << "IP:";
  // strs << (int)IP[0] << "." << (int)IP[1] << "." << (int)IP[2] << "." << (int)IP[3] ;
  // strs << "\n";
  strs << "errors:";
  strs << errorCounter;
  strs << " , bat->";
  strs << std::fixed << std::setw( 4 ) << std::setprecision( 2 ) << getBattVoltage();
  strs << "V";
  strs << "\n\nV1=";
  strs << std::fixed << std::setw( 5 ) << std::setprecision( 2 ) << Voltages_values[0];
  strs << "   V2=";
  strs << std::fixed << std::setw( 5 ) << std::setprecision( 2 ) << Voltages_values[1];
  strs << "\nV3=";
  strs << std::fixed << std::setw( 5 ) << std::setprecision( 2 ) << Voltages_values[2];
  strs << "   V4=";
  strs << std::fixed << std::setw( 5 ) << std::setprecision( 2 ) << Voltages_values[3];    
  // // strs << "\nelapsed:";
  // // strs << pf.getElapsedSeconds()/1000;
  strs << "\n\nsd:";
  strs << std::setw(2) << std::setprecision(0) << SD.cardSize()/1000000000.0;
  display->setTextSize(1);
  display->write(strs.str().c_str());

  //display->setCursor(0,30);
  for (int u = 27 ; u<(28+SDCardUsedPercent) ; u++){
    display->drawPixel(u,63,SSD1306_WHITE);
  }

  //display->write(",.");

  //Serial.println(strs.str().c_str());
  //Serial.println(Voltages_values[0]);

  display->display();

  //Serial.printf("/n battery voltage = %f V\n",getBattVoltage());

}


void IRAM_ATTR onOledRefreshTick() {
  shouldDisplay = true;
}

bool shouldcheckSDCard = true;
void IRAM_ATTR onSDcardCheckTick() {
  shouldcheckSDCard = true;
}

#define SD_SIZE_MB 32
void checkSDCard(){
  SDCardUsedPercent = SD.usedBytes()/SD.cardSize()*100.0 ;
  Serial.printf("SD used :%d percent. total: %d, used: %d\n",SDCardUsedPercent, SD.usedBytes(),SD.cardSize());
}

String fileName;
bool error = false;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  pinMode(LED_BUILTIN,OUTPUT);
  //Serial.println ("boo1");

/*******************************************************/
/***********              oled             **************/
/*******************************************************/
  {
  
  display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display->clearDisplay();
  display->drawBitmap(0, 0,beesplash_data, beesplash_width, beesplash_height, 1);
  display->display();
  delay(SPLASH_DELAY_MS); // Pause 
  display->clearDisplay();


  //for text writing:
  display->setTextSize(1);      // Normal 1:1 pixel scale
  display->setTextColor(SSD1306_WHITE); // Draw white text
  display->setCursor(0, 0);     // Start at top-left corner
  display->cp437(true);         // Use full 256 char 'Code Page 437' font

  OLED_refreshTimer = timerBegin(1, 80, true);
  timerAttachInterrupt(OLED_refreshTimer, &onOledRefreshTick, true); 
  timerAlarmWrite(OLED_refreshTimer, 1000000, true);           
  timerAlarmEnable(OLED_refreshTimer);

  SD_CheckTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(SD_CheckTimer, &onSDcardCheckTick, true); 
  timerAlarmWrite(SD_CheckTimer, 1000000000, true);    //once every 1000 seconds        
  timerAlarmEnable(SD_CheckTimer);
  
  }

// /*******************************************************/
// /***********              OTA             **************/
// /*******************************************************/
//     // Connect to Wi-Fi network with SSID and password
//   Serial.println("Setting AP (Access Point)…");
//   // Remove the password parameter, if you want the AP (Access Point) to be open
//   bool wifigood = WiFi.softAP(ssid, password);
//   Serial.printf ("wifi is %s\n", wifigood?"good":"bad");
  

//   //wait for SYSTEM_EVENT_AP_START:
//   delay(100);
//   IPAddress NMask(255, 255, 255, 0);
//   WiFi.softAPConfig(IP,IP,NMask);

//   IP = WiFi.softAPIP();

//   Serial.print("AP IP address: ");
//   Serial.println(IP);
//   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
//     request->send(200, "text/plain", "Hi! I am your trusty voltage logger.\n to update me over the air use /update. \n\n don't forget to tell Aviv i said hi... :)");
//   });
//   AsyncElegantOTA.begin(&server);    // Start ElegantOTA
//   server.begin();
//   Serial.println("HTTP server started");


/*******************************************************/
/***********        Real Time Clock       **************/
/*******************************************************/
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

/*******************************************************/
/***********          SD card             **************/
/*******************************************************/
  DateTime now = rtc.now();
  fileName = (String)"/" +now.year() + "_" + now.month() + "_" + now.day() + "_" + now.hour() + "_" + now.minute() + "_" + now.second()+".csv";
  Serial.print("Initializing SD card...");
  bool sdInitGood = false;
  for (int i=0; i<20 && !sdInitGood ; i++){
    Serial.printf("SD card init attempt #%d\n",i);
    sdInitGood = SD.begin(chipSelect);
    delay(50);
  }
  // see if the card is present and can be initialized:
  if (!sdInitGood) {
    Serial.println("Card failed, or not present");
    currentError = 4;
    errorCounter++;
    display->setTextSize(2); 

    //put out a SD warning sign (three blinks)
    for (int j = 1 ; j<5 ; j++){
        digitalWrite(LED_BUILTIN,HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN,LOW);
        delay(500);
    }

    //delay(1000);
    // don't do anything more:
    writeLastReadingToOLED();
    while (1);
  }
  Serial.println("sd card initialized.");

}

#define MAX_LOOPS_WO_PRINTING 100
#define MAX_ENTRIES_PER_FILE_NORMAL 5000 // 1000 ~ 60k
#define MAX_ENTRIES_PER_FILE_ERROR 50000 // ~22 minutes
long loopsWithoutprinting = 0;
long EntriesAdded = 0;
String dataline="";



void loop() {

  if (shouldcheckSDCard) {
    checkSDCard();
    shouldcheckSDCard = false;
  }

  if (shouldDisplay) {
    writeLastReadingToOLED();
    shouldDisplay = false;
  }

  error = false;

  //read data - fast loop:
  for (int i = 0 ; i<CHANNEL_COUNT ; i++){

    ADC_values[i] = analogRead(Analog_channel_pin[i]);
    if (ADC_values[i]<1920){ //value that indicates a voltage drop below 22v
      error = true; //raise voltage drop event flag
      //Serial.printf("channel %d: ADC VALUE = %d ",i,ADC_values[i]);
      //delay(100);
    }
  }

  //change to another file in case file is too big (in error - aloow the file to grow larger so not to miss data)
  if ( (!error && EntriesAdded>MAX_ENTRIES_PER_FILE_NORMAL) || EntriesAdded> MAX_ENTRIES_PER_FILE_ERROR ){
    DateTime now = rtc.now();
    fileName = (String)"/" +now.year() + "_" + now.month() + "_" + now.day() + "_" + now.hour() + "_" + now.minute() + "_" + now.second()+".csv";
    Serial.println(fileName.c_str());
    EntriesAdded=0;
  }

  if (error || loopsWithoutprinting > MAX_LOOPS_WO_PRINTING){
    loopsWithoutprinting = 0;
    EntriesAdded++;
    DateTime now = rtc.now();

    for ( int w=0 ; w< CHANNEL_COUNT; w++){
      Voltages_values[w] = a1*ADC_values[w]+ (ADC_values[w]>0?b1:0);
    }
    //Serial.printf(" V1=%f V2=%f V3=%f V4=%f\n",Voltages_values[0],Voltages_values[1],Voltages_values[2],Voltages_values[3]);
    

    dataline = (String)now.unixtime() + " , " + (String)now.year() + "/" + now.month() + "/" + now.day() + "-" +now.hour() + ":" + now.minute() + ":" + now.second() + " , " + String(Voltages_values[0]) + " , " + String(Voltages_values[1]) + " , " + String(Voltages_values[2]) +" , " + String(Voltages_values[3]);

    //print to file:
    File dataFile = SD.open(fileName.c_str(), FILE_APPEND);
    // if the file is available, write to it:
    if (dataFile) {
      dataFile.println(dataline);
      dataFile.close();
      // print to the serial port too:
      //Serial.println(dataline);
    } else {    // if the file isn't open, pop up an error:
      Serial.print("error opening ");
      Serial.println(fileName.c_str());
    }
  }else{
    loopsWithoutprinting++;
  }


    //turn led ON if error
  if (error){
    digitalWrite(LED_BUILTIN,HIGH); //stays high untill next reset
    if (!prevCycleHadError){
      errorCounter++;
      prevCycleHadError = true;
      currentError = 5;
    }
  }else{                          //no error this round
    prevCycleHadError = false;
    currentError = 0;
  }
  


}
