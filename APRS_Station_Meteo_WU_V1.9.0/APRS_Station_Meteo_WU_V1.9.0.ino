
/*
   Mini Weather Station Ver 1.9.0

     Date 2021-Jun-06

   esp8266 core 2.4.2
   ide 1.8.5

  **** Thank-You ****
  https://lastminuteengineers.com/bme280-esp8266-weather-station/
  https://www.arduinolibraries.info/libraries/adafruit-bme280-library
  *******************

  NodeMCU and BME280 PIN ASSIGMENT
  3v --------- 3v
  Gnd -------- Gnd
  D5 --------- SCL
  D6 --------- SDA

  D4 --------- Push Button
  Gnd -------- Push Button

  Note: user = admin, password = your wifi password

  ///////////////////////////////////////////////////////////////////////////////////////////

*/

const String ver = "Ver 1.9.0";

#include <NTPClient.h>

// Define NTP properties
#define NTP_OFFSET   0     // In seconds
#define NTP_INTERVAL 30 * 60000    // In miliseconds
#define NTP_ADDRESS  "ca.pool.ntp.org"  // change this to whatever pool is closest (see ntp.org)

#include <ESP8266WiFi.h>
#include <EEPROM.h>

#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>

#include <ESP8266mDNS.h>
#include <Wire.h>

#include <BlynkSimpleEsp8266.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#define SEALEVELPRESSURE_HPA (1013.25) // sea level reference.

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

Adafruit_BME280 bme;

float tFar, temperature, humidity, pressure, feet, meter;
float tMin = 50.00;
float tMax = -50.00;
int Hum, Bar;

/////////////////////
// Battery Monitor //
//////////////////////////////////////////////////////
int BAT = A0;
float battRef = 0.0;
float RatioFactor = 0.0; // resistor ratio factor
float Tvoltage = 0.0;
float Vvalue = 0.0, Rvalue = 0.0;
bool lowBattery = false;
int vbatt;
float lowNotification = 3.00;

///////////
// Blynk //
//////////////////////////////////////////////////////
String AuthToken = "";                  // Blynk APPlication token
String BlynkServer = "";                // Blynk local raspi server
int BlynkPort;                          // Blynk local raspi port
volatile bool Connected2Blynk = false;  // true if nodemcu is connected to blynk server
volatile bool Setup = false;            // NodeMcu in setup(AP) mode 192.168.4.1 , only wifi web pages available.

///////////////////////
// Blynk GPS Stream //
//////////////////////////////////////////////////////
String blat, blon;                  // GPS converted to dmm.mm ready for aprs.
float gpsalt;                       // Altitude in meter.
String lat_SouthN, lon_EastW;       // ex: -74=W° or 74°=E, 45°=N or -45°=S

//////////////
// WebPages //
//////////////////////////////////////////////////////////////////////////////
String webSite, javaScript, XML, header, footer, ssid, password;

////////////////
// Button CSS //
//////////////////////////////////////////////////////////////////////////////
const String button = ".button {background-color: white;border: 2px solid #555555;color: black;padding: 16px 32px;text-align: center;text-decoration: none;display: inline-block;font-size: 16px;margin: 4px 2px;-webkit-transition-duration: 0.4s;transition-duration: 0.4s;cursor: pointer;}.button:hover {background-color: #555555;color: white;}.disabled {opacity: 0.6;pointer-events: none;}";

///////////////////
// System Uptime //
//////////////////////////////////////////////////////////////////////////////
byte hh = 0; byte mi = 0; byte ss = 0;      // hours, minutes, seconds
unsigned int dddd = 0;                      // days (for if you want a calendar)
unsigned long lastTick = 0;                 // time that the clock last "ticked"
char timestring[25];                        // for output

///////////////
// WU PSW ID //
//////////////////////////////////////////////////////////////////////////////
String WUID = "";                                 // Weather Underground ID
String WUKEY = "";                                // Weather Underground KEY
const char* wuhost = "rtupdate.wunderground.com"; // Weather Underground HOST
String wurl = "";                                 // Weather Underground URL for Broadcast

////////////////////////
// APRS STATION SETUP //
//////////////////////////////////////////////////////////////////////////////
String CallSign = "";             // Weather Station CallSign
String Lat = "";                  // used for Weather Station
String Lon = "";                  // used for Weather Station
String Comment = "";              // used for Weather Station

/////////////////
// APRS SERVER //
//////////////////////////////////////////////////////////////////////////////
String Password = "" ;                                                      // callsign password for aprs server
String broadcastStatus = "Weather Station Rebooted! Broadcast every 10min";           // message display on ajax home web page
bool broadcastWX = false;                                                   // true = ready to broadcast on aprs server

//////////////////////
// Altitude Setting //
//////////////////////////////////////////////////////////////////////////////
int ALTITUDE;           // BME280 altitude
int epromALTITUDE;      // EEPROM altitude

//////////////////////////////
// display on ajax web page //
//////////////////////////////////////////////////////////////////////////////
bool Mode = 1;          // 0 = Imperial, 1 = Metric - display on ajax home web page
byte percentQ = 0;      // wifi signal strength % - display on ajax home web page

/////////////////////////////
// Setup Mode Access Point //
//////////////////////////////////////////////////////////////////////////////
const int buttonPin = 2; // D4 - hold 10sec to enter in setup mode!

////////////
// Millis //
//////////////////////////////////////////////////////////////////////////////

const unsigned long ReadSensor = 2000;     // Read sensor every 2s
unsigned long previousReadSensor = 0;

const unsigned long check = 30000;         // check wifi connected every 30sec
unsigned long previouscheck = 0;

const unsigned long checkBatt = 25000;     // check battery every 25sec
unsigned long previouscheckBatt = 0;

const unsigned long SendWu = 360000;       // Broadcast to WU every 6min
unsigned long previousSendWu = 0;

const unsigned long SendWx = 600000;       // Broadcast to APRS Server every 10min
unsigned long previousSendWx = 0;

WidgetTerminal terminal(V10);

/////////////////////////////////////////////
// display on Blynk terminal on mobile app //
//////////////////////////////////////////////////////////////////////////////
BLYNK_WRITE(V10) {
  if (String("?") == param.asStr()) {
    String IP = (WiFi.localIP().toString());

    terminal.println();
    terminal.print(F("mini Weather Station "));
    terminal.println(ver);
    terminal.println(F("======================================"));
    terminal.println(F("http://cwop-wx.local/api"));
    terminal.print(F("Connected to: "));
    terminal.println(WiFi.SSID());
    terminal.print(F("IP: "));
    terminal.println(IP);
    terminal.print(F("Admin Password: "));
    terminal.println(password);
    terminal.print(F("Batt: "));
    terminal.print(Tvoltage);
    terminal.println(" v");
    terminal.print(F("Signal Strength: "));
    terminal.print(percentQ);
    terminal.println(F("%"));
    terminal.println(F("======================================"));
    terminal.flush();
  }
}

////////////////////////////////
// GPS stream widget - dmm.mm //
//////////////////////////////////////////////////////////////////////////////
BLYNK_WRITE(V20) {
  float gpslat = param[0].asFloat();
  float gpslon = param[1].asFloat();
  gpsalt = param[2].asFloat();                //altitude in meter
  float gpsspeed = param[3].asFloat();

  /////////////////////////////////////////
  // Make String for use for APRS Server //
  /////////////////////////////////////////

  // Lat Converting to dmm.mm

  if (gpslat < 0) {
    lat_SouthN = "S";
    gpslat = (gpslat * -1);
  }
  else {
    lat_SouthN = "N";
  }

  int lat_d = gpslat;
  gpslat -= lat_d;
  gpslat = gpslat * 60;
  int lat_mm = gpslat;
  gpslat -= lat_mm;
  int lat_m = gpslat * 100;

  char blynkLat_d[3];
  sprintf (blynkLat_d, "%02d", lat_d);
  char blynkLat_mm[3];
  sprintf (blynkLat_mm, "%02d", lat_mm);
  char blynkLat_m[3];
  sprintf (blynkLat_m, "%02d", lat_m);

  blat = "";
  blat += (blynkLat_d);
  blat += (blynkLat_mm);
  blat += (".");
  blat += (blynkLat_m);
  blat += String(lat_SouthN);

  // Lon Converting to dmm.mm

  if (gpslon < 0) {
    lon_EastW = "W";
    gpslon = (gpslon * -1);
  }
  else {
    lon_EastW = "E";
  }

  int lon_d = gpslon;
  gpslon -= lon_d;
  gpslon = gpslon * 60;
  int lon_mm = gpslon;
  gpslon -= lon_mm;
  int lon_m = gpslon * 100;

  char blynkLon_d[3];
  sprintf (blynkLon_d, "%03d", lon_d);
  char blynkLon_mm[3];
  sprintf (blynkLon_mm, "%02d", lon_mm);
  char blynkLon_m[3];
  sprintf (blynkLon_m, "%02d", lon_m);

  blon = "";
  blon += (blynkLon_d);
  blon += (blynkLon_mm);
  blon += (".");
  blon += (blynkLon_m);
  blon += String(lon_EastW);

  broadcastWX = true;
}

// Every time we connect to the cloud...
BLYNK_CONNECTED() {
  // Request the latest state from the server
  Blynk.syncVirtual(V20);
}

ESP8266WebServer server(80);

// OTA UPDATE
ESP8266HTTPUpdateServer httpUpdater; //ota

void setup() {

  Wire.begin(12, 14); // NodeMcu sda=D6, scl=D5
  delay(10);
  bme.begin(0x76);
  delay(10);
  /////////////////////////////////////

  pinMode(buttonPin, INPUT_PULLUP);

  //////////////////////////////////////
  // START EEPROM
  EEPROM.begin(512);
  delay(10);

  //////////////////////////////////////
  // Init EEPROM
  byte Init = EEPROM.read(451);

  if (Init != 123) {
    for (int i = 100; i < 512; ++i)
    {
      EEPROM.write(i, 0);
    }
    delay(10);
    EEPROM.commit();
    delay(10);

    EEPROM.write(451, 123);
    delay(10);
    EEPROM.commit();
    delay(10);
  }

  //////////////////////////////////////
  // READ EEPROM SSID & PASSWORD
  String esid;
  for (int i = 34; i < 67; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  delay(10);
  ssid = esid;

  String epass = "";
  for (int i = 67; i < 100; ++i)
  {
    epass += char(EEPROM.read(i));
  }
  delay(10);
  password = epass;

  ////////// CALL SIGN //////////
  int CallSignlength = EEPROM.read(134) + 135;
  delay(10);
  String eCallSign = "";
  for (int i = 135; i < CallSignlength; ++i)
  {
    eCallSign += char(EEPROM.read(i));
  }
  CallSign = eCallSign;

  //////////////////////////////////////
  // Blynk Ip Local Server
  int IndexBlynkServer = EEPROM.read(146) + 147;
  String ReadBlynkServer = "";
  for (int i = 147; i < IndexBlynkServer; ++i)
  {
    ReadBlynkServer += char(EEPROM.read(i));
  }
  BlynkServer = ReadBlynkServer;

  //////////////////////////////////////
  // Battery RatioFactor
  String Ratio = "";
  for (int i = 180; i < 191; ++i)
  {
    Ratio += char(EEPROM.read(i));
  }

  RatioFactor = Ratio.toFloat();

  //////////////////////////////////////
  // Weather Underground ID
  int IndexWUID = EEPROM.read(197) + 198;
  String ReadWUID = "";
  for (int i = 198; i < IndexWUID; ++i)
  {
    ReadWUID += char(EEPROM.read(i));
  }
  WUID = ReadWUID;

  //////////////////////////////////////
  // Weather Underground Key
  int IndexWUKey = EEPROM.read(215) + 216;
  String ReadWUKey = "";
  for (int i = 216; i < IndexWUKey; ++i)
  {
    ReadWUKey += char(EEPROM.read(i));
  }
  WUKEY = ReadWUKey;

  ////////// Comment ////////
  int eCommentlength = EEPROM.read(233) + 234;
  delay(10);
  String eComment = "";
  for ( int i = 234; i < eCommentlength; ++i)
  {
    eComment += char(EEPROM.read(i));
  }
  Comment = eComment;

  ////// APRS PASSWORD ///////
  int Passwordlength = EEPROM.read(267) + 268;
  delay(10);
  String ePassword = "";
  for (int i = 268; i < Passwordlength; ++i)
  {
    ePassword += char(EEPROM.read(i));
  }
  Password = ePassword;

  ////////// LATITUDE ///////
  int Latlength = EEPROM.read(275) + 276;
  delay(10);
  String eLat = "";
  for (int i = 276; i < Latlength; ++i)
  {
    eLat += char(EEPROM.read(i));
  }
  Lat = eLat;

  ///////// LONGITUDE ////////
  int Lonlength = EEPROM.read(287) + 288;
  delay(10);
  String eLon = "";
  for (int i = 288; i < Lonlength; ++i)
  {
    eLon += char(EEPROM.read(i));
  }
  Lon = eLon;

  ///////// ALTITUDE ////////
  int Altlength = EEPROM.read(299) + 300;
  delay(10);
  String eAlt = "";
  for (int i = 300; i < Altlength; ++i)
  {
    eAlt += char(EEPROM.read(i));
  }
  epromALTITUDE = eAlt.toInt();

  /////////////////////////////////////
  // Blynk Port Local Server
  int IndexBlynkPort = EEPROM.read(305) + 306;
  String ReadBlynkPort = "";
  for (int i = 306; i < IndexBlynkPort; ++i)
  {
    ReadBlynkPort += char(EEPROM.read(i));
  }
  BlynkPort = ReadBlynkPort.toInt();

  //////////////////////////////////////
  //BLYNK AUTH TOKEN
  int IndexToken = EEPROM.read(311) + 312;
  String ReadToken = "";
  for (int i = 312; i < IndexToken; ++i)
  {
    ReadToken += char(EEPROM.read(i));
  }
  AuthToken = ReadToken;

  EEPROM.end();

  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1, // temperature
                  Adafruit_BME280::SAMPLING_X1, // pressure
                  Adafruit_BME280::SAMPLING_X1, // humidity
                  Adafruit_BME280::FILTER_OFF   );

  // suggested rate is 1/60Hz (1m)

  ////////
  // API //
  ///////////////////////////////////////////////////////////////
  server.on("/api", []() {

    String JsonResponse = "{\n\"station\": ";
    JsonResponse += "{\n\"id\":\"" + String (CallSign) + "\", \n";
    JsonResponse += "\"lat\":\"" + String (Lat) + "\", \n";
    JsonResponse += "\"lon\":\"" + String (Lon) + "\", \n";

    JsonResponse += "\"temp-c\":\"" + String (temperature) + "\", \n";
    JsonResponse += "\"temp-f\":\"" + String (tFar) + "\", \n";

    JsonResponse += "\"hum\":\"" + String (Hum) + "\", \n";
    JsonResponse += "\"pres\":\"" + String (pressure) + "\", \n";

    JsonResponse += "\"alt-m\":\"" + String (meter) + "\", \n";
    JsonResponse += "\"alt-f\":\"" + String (feet) + "\", \n";

    JsonResponse += "\"Vbatt\":\"" + String (Tvoltage) + "\", \n";

    JsonResponse += "\"wifi_signal\":\"" + String (percentQ) + "\", \n";

    JsonResponse += "\"broadcast_status\":\"" + String (broadcastStatus) + "\"";

    JsonResponse += "\n}\n}";

    server.send(200, "application/json",  JsonResponse);
  });

  ///////////////////
  // SSID PASSWORD //
  ///////////////////////////////////////////////////////////////
  server.on("/Wifi", []() {
    if (WiFi.status() == WL_CONNECTED) {
      if (!server.authenticate("admin", password.c_str()))
        return server.requestAuthentication();
    }

    String WifiSsid = server.arg("ssid");
    String WifiPassword = server.arg("pass");

    //////////////////////////////////////////////////////
    if (WifiSsid.length() <= 32 and WifiPassword.length() <= 32) {
      // START EEPROM
      EEPROM.begin(512);
      delay(10);

      ///// Erase EEPROM
      for (int i = 34; i < 100; ++i)
      {
        EEPROM.write(i, 0);
      }

      // Write Password to EEPROM
      for (int i = 0; i < WifiPassword.length(); ++i)
      {
        EEPROM.write(67 + i, WifiPassword[i]);
      }

      // Write SSID to EEPROM
      for (int i = 0; i < WifiSsid.length(); ++i)
      {
        EEPROM.write(34 + i, WifiSsid[i]);
      }

      delay(10);
      EEPROM.commit();
      delay(10);
      EEPROM.end();

      handleREBOOT();
    }
    else {
      server.send(200, "text/html", "<header><h1>Error!, Please enter valid SSID! and PASS! max32 character <a href=/wifisetting >Back!</a></h1></header>");
    }
  });

  /////////////////////
  // WU Write Config //
  ///////////////////////////////////////////////////////////////
  server.on("/WU", []() {
    if (WiFi.status() == WL_CONNECTED) {
      if (!server.authenticate("admin", password.c_str()))
        return server.requestAuthentication();
    }

    String ReadWUID = server.arg("id");
    String ReadWUKEY = server.arg("key");

    //////////////////////////////////////////////////////
    if (ReadWUID.length() <= 16 and ReadWUKEY.length() <= 16) {
      // START EEPROM
      EEPROM.begin(512);
      delay(10);

      EEPROM.write(197, ReadWUID.length());

      for (int i = 0; i < ReadWUID.length(); ++i)
      {
        EEPROM.write(198 + i, ReadWUID[i]);
      }
      WUID = ReadWUID;

      //////////////////////////////////////////////////////
      EEPROM.write(215, ReadWUKEY.length());

      for (int i = 0; i < ReadWUKEY.length(); ++i)
      {
        EEPROM.write(216 + i, ReadWUKEY[i]);
      }
      WUKEY = ReadWUKEY;

      delay(10);
      EEPROM.commit();
      delay(10);
      EEPROM.end();

      handleWU();
    }
    else {
      server.send(200, "text/html", "<header><h1>Error! , Please enter valid ID and KEY! max 16 character <a href=/wu >Back!</a></h1></header>");
    }
  });

  ////////////////////
  // BlynkGpsStream //
  ///////////////////////////////////////////////////////////////
  server.on("/BlynkGpsStream", []() {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();

    String ReadLat = server.arg("lat");
    String ReadLon = server.arg("lon");
    String ReadAlt = server.arg("alt");

    // START EEPROM
    EEPROM.begin(512);
    delay(10);

    //////////////////////////////////////////////////////
    if (ReadLat.length() <= 10) {
      EEPROM.write(275, ReadLat.length());

      for (int i = 0; i < ReadLat.length(); ++i)
      {
        EEPROM.write(276 + i, ReadLat[i]);
      }
      Lat = ReadLat;
    }

    //////////////////////////////////////////////////////
    if (ReadLon.length() <= 10) {
      EEPROM.write(287, ReadLon.length());

      for (int i = 0; i < ReadLon.length(); ++i)
      {
        EEPROM.write(288 + i, ReadLon[i]);
      }
      Lon = ReadLon;
    }

    //////////////////////////////////////////////////////
    if (ReadAlt.length() <= 4) {
      EEPROM.write(299, ReadAlt.length());

      for (int i = 0; i < ReadAlt.length(); ++i)
      {
        EEPROM.write(300 + i, ReadAlt[i]);
      }
      epromALTITUDE = ReadAlt.toInt();
    }

    delay(10);
    EEPROM.commit();
    delay(10);
    EEPROM.end();

    handleSTATION();
  });

  //////////////////
  // Station Info //
  ///////////////////////////////////////////////////////////////
  server.on("/Station", []() {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();

    String ReadCallSign = server.arg("callsign");
    String ReadPassword = server.arg("password");
    String ReadLat = server.arg("lat");
    String ReadLon = server.arg("lon");
    String ReadAlt = server.arg("alt");
    String ReadComment = server.arg("comment");

    // START EEPROM
    EEPROM.begin(512);
    delay(10);

    //////////////////////////////////////////////////////
    if (ReadCallSign.length() <= 9) {
      EEPROM.write(134, ReadCallSign.length());

      for (int i = 0; i < ReadCallSign.length(); ++i)
      {
        EEPROM.write(135 + i, ReadCallSign[i]);
      }
      CallSign = ReadCallSign;
    }
    else {
      server.send(200, "text/html", "<header><h1>Error! , Please enter valid Call Sign! max 9 character <a href=/station >Back!</a></h1></header>");
    }
    //////////////////////////////////////////////////////
    if (ReadPassword.length() <= 6) {
      EEPROM.write(267, ReadPassword.length());

      for (int i = 0; i < ReadPassword.length(); ++i)
      {
        EEPROM.write(268 + i, ReadPassword[i]);
      }
      Password = ReadPassword;
    }
    else {
      server.send(200, "text/html", "<header><h1>Error! , Please enter valid Password! max 6 character <a href=/station >Back!</a></h1></header>");
    }
    //////////////////////////////////////////////////////
    if (ReadLat.length() <= 10) {
      EEPROM.write(275, ReadLat.length());

      for (int i = 0; i < ReadLat.length(); ++i)
      {
        EEPROM.write(276 + i, ReadLat[i]);
      }
      Lat = ReadLat;
    }
    else {
      server.send(200, "text/html", "<header><h1>Error! , Please enter valid Lat! max 10 character <a href=/station >Back!</a></h1></header>");
    }
    //////////////////////////////////////////////////////
    if (ReadLon.length() <= 10) {
      EEPROM.write(287, ReadLon.length());

      for (int i = 0; i < ReadLon.length(); ++i)
      {
        EEPROM.write(288 + i, ReadLon[i]);
      }
      Lon = ReadLon;
    }
    else {
      server.send(200, "text/html", "<header><h1>Error! , Please enter valid Lon! max 10 character <a href=/station >Back!</a></h1></header>");
    }
    //////////////////////////////////////////////////////
    if (ReadAlt.length() <= 4) {
      EEPROM.write(299, ReadAlt.length());

      for (int i = 0; i < ReadAlt.length(); ++i)
      {
        EEPROM.write(300 + i, ReadAlt[i]);
      }
      epromALTITUDE = ReadAlt.toInt();
    }
    else {
      server.send(200, "text/html", "<header><h1>Error! , Please enter valid Alt! max 4 character <a href=/station >Back!</a></h1></header>");
    }
    //////////////////////////////////////////////////////
    if (ReadComment.length() <= 32) {
      EEPROM.write(233, ReadComment.length());

      for (int i = 0; i < ReadComment.length(); ++i)
      {
        EEPROM.write(234 + i, ReadComment[i]);
      }
      Comment = ReadComment;
    }
    else {
      server.send(200, "text/html", "<header><h1>Error! , Please enter valid Comment! max 32 character <a href=/station >Back!</a></h1></header>");
    }
    //////////////////////////////////////////////////////

    delay(10);
    EEPROM.commit();
    delay(10);
    EEPROM.end();

    handleSTATION();
  });

  /////////////////////////
  // Battery Calibration //
  ///////////////////////////////////////////////////////////////
  server.on("/Battery", []() {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();

    String ReadBattVoltage = server.arg("BattVoltage");

    // START EEPROM
    EEPROM.begin(512);
    delay(10);

    //////////////////////////////////////////////////////
    if ((ReadBattVoltage.length() >= 1) and (ReadBattVoltage.length() <= 10)) {

      ////////////////////// Battery Voltage ///////////////////
      for (unsigned int i = 0; i < 10; i++) {
        Vvalue = Vvalue + analogRead(BAT);         //Read analog Voltage
        delay(5);                                  //ADC stable
      }
      // RatioFactor = battRef / Vvalue * 1024.0 / 5
      battRef = ReadBattVoltage.toFloat();
      Vvalue = (float)Vvalue / 10.0;               //Find average of 10 values
      Rvalue = (float)(Vvalue / 1024.0) * 5;       //Convert Voltage in 5v factor
      RatioFactor = battRef / Vvalue * 1024.0 / 5; //Find RatioFactor

      String ratio = String(RatioFactor);

      for (int i = 0; i < ratio.length(); ++i)
      {
        EEPROM.write(180 + i, ratio[i]);
      }
    }

    delay(10);
    EEPROM.commit();
    delay(10);
    EEPROM.end();

    handleSTATION();
  });

  ///////////////
  // Blynk Key //
  ////////////////////////////////////////////////////////////
  server.on("/Blynk", []() {
    if (WiFi.status() == WL_CONNECTED) {
      if (!server.authenticate("admin", password.c_str()))
        return server.requestAuthentication();
    }

    String ReadKey = server.arg("key");

    if (ReadKey.length() <= 32) {
      EEPROM.begin(512);
      delay(10);

      EEPROM.write(311, ReadKey.length());
      for (int i = 0; i < ReadKey.length(); ++i)
      {
        EEPROM.write(312 + i, ReadKey[i]);
      }
      delay(10);
      EEPROM.commit();
      delay(10);

      AuthToken = ReadKey;
      EEPROM.end();

      if (BlynkServer.length() > 5) {
        Blynk.config(AuthToken.c_str(), BlynkServer.c_str(), BlynkPort);
      }
      else {
        Blynk.config(AuthToken.c_str(), "blynk-cloud.com", 8442);
      }

      delay(1000);
      Blynk.connect(3333);  // timeout set to 10 seconds and then continue without Blynk
    }
    else {
      server.send(200, "text/html", "<header><h1>Error! , Please enter valid AuthToken! max 32 character <a href=/blynk >Back!</a></h1></header>");
    }

    handleBLYNK();
  });

  //////////////////
  // Blynk Server //
  ////////////////////////////////////////////////////////////
  server.on("/BlynkServer", []() {
    if (WiFi.status() == WL_CONNECTED) {
      if (!server.authenticate("admin", password.c_str()))
        return server.requestAuthentication();
    }

    String ReadBlynkServer = server.arg("server");
    String ReadBlynkPort = server.arg("port");

    EEPROM.begin(512);
    delay(10);

    if (ReadBlynkServer.length() <= 32) {
      EEPROM.write(146, ReadBlynkServer.length());
      for (int i = 0; i < ReadBlynkServer.length(); ++i)
      {
        EEPROM.write(147 + i, ReadBlynkServer[i]);
      }
      BlynkServer = ReadBlynkServer;
    }
    else {
      server.send(200, "text/html", "<header><h1>Error! , Please enter valid Server! max 32 character <a href=/blynk >Back!</a></h1></header>");
    }

    if (ReadBlynkPort.length() <= 4) {
      EEPROM.write(305, ReadBlynkPort.length());
      for (int i = 0; i < ReadBlynkPort.length(); ++i)
      {
        EEPROM.write(306 + i, ReadBlynkPort[i]);
      }
      BlynkPort = ReadBlynkPort.toInt();
    }
    else {
      server.send(200, "text/html", "<header><h1>Error! , Please enter valid Port! max 4 character <a href=/blynk >Back!</a></h1></header>");
    }

    delay(10);
    EEPROM.commit();
    delay(10);
    EEPROM.end();

    if (BlynkServer.length() > 5) {
      Blynk.config(AuthToken.c_str(), BlynkServer.c_str(), BlynkPort);
    }
    else {
      Blynk.config(AuthToken.c_str(), "blynk-cloud.com", 8442);
    }

    delay(1000);
    Blynk.connect(3333);  // timeout set to 10 seconds and then continue without Blynk

    handleBLYNK();
  });

  ///////////////
  // Broadcast //
  ///////////////////////////////////////////////////////////////
  server.on("/broadcast", []() {
    if (WiFi.status() == WL_CONNECTED) {
      if (!server.authenticate("admin", password.c_str()))
        return server.requestAuthentication();
    }

    String ReadWU = server.arg("WU");
    String ReadAPRS = server.arg("APRS");

    if (ReadWU == "1") {
      WU();
      handleWU();
    }

    if (ReadAPRS == "1") {
      broadcastStatus = (F("Broadcast to APRS server ..."));
      server.send(200, "text/html", SendHTML(temperature, humidity, pressure, feet, meter));
      broadcastWX = true;
      APRS();
      broadcastWX = false;
    }
  });

  //////////////////////
  // WiFi Connection //
  ////////////////////////////////////////////////////////////
  if ((ssid != "") and (password != "")) {
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    delay(10);
    WiFi.hostname("cwop-wx");
    delay(10);
    WiFi.begin(ssid.c_str(), password.c_str());
    Setup = false;
  } else {
    WiFi.disconnect();
    delay(10);
    WiFi.mode(WIFI_AP);
    delay(10);
    WiFi.softAP("cwop-wx-setup", "");

    Setup = true;
  }
  delay(5000);

  if (MDNS.begin("cwop-wx", WiFi.localIP())) {
    Serial.println("MDNS Started");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("MDNS started FAILED");
  }

  if (Setup == false) {
    httpUpdater.setup(&server, "/firmware", "admin", password.c_str());

    server.onNotFound(handleNotFound);
    server.on("/", handle_OnConnect);
    server.on("/station", handleSTATION);
    server.on("/wifisetting", handleWIFISETTING);
    server.on("/blynk", handleBLYNK);
    server.on("/wu", handleWU);
    server.on("/sw", handleSW);
    server.on("/reboot", handleREBOOT);

    ///////////
    // Blynk //
    ///////////////////////////////////////////////////
    if (BlynkServer.length() > 5) {
      Blynk.config(AuthToken.c_str(), BlynkServer.c_str(), BlynkPort);
    }
    else {
      Blynk.config(AuthToken.c_str(), "blynk-cloud.com", 8442);
    }
    Blynk.connect(3333);  // timeout set to 10 seconds and then continue without Blynk
  }  else {
    server.onNotFound(handleNotFound);
    server.on("/", handleWIFISETTING);
  }

  server.begin();
  timeClient.begin();
  delay(10);

  Batt();
}

void loop() {
  MDNS.update();
  Connected2Blynk = Blynk.connected();
  if (Connected2Blynk) {
    Blynk.run();
  }

  server.handleClient();

  ///////////////////
  // System Uptime //
  ////////////////////////////////////////////
  if ((micros() - lastTick) >= 1000000UL) {
    lastTick += 1000000UL;
    ss++;
    if (ss >= 60) {
      ss -= 60;
      mi++;
    }
    if (mi >= 60) {
      mi -= 60;
      hh++;
    }
    if (hh >= 24) {
      hh -= 24;
      dddd++;
    }
  }

  ////////////////////
  // RESET PASSWORD //
  ////////////////////////////////////////////////////////////
  bool PushButton = digitalRead(buttonPin);
  int PushButtonCount = 0;
  if (PushButton == LOW)
  {
    while (PushButton == LOW) {
      PushButton = digitalRead(buttonPin);

      PushButtonCount++;

      terminal.println();
      terminal.println(F("Press for 10sec to reset!"));
      terminal.println(PushButtonCount);
      terminal.flush();

      delay(1000);

      if (PushButtonCount >= 10) {

        terminal.println();
        terminal.println(F("Controller reboot in Setup Mode!"));
        terminal.println(F("SSID: cwop-wx-setup, IP: 192.168.4.1"));
        terminal.flush();

        // START EEPROM
        EEPROM.begin(512);
        delay(10);

        // Erase SSID and PASSWORD
        for (int i = 34; i < 100; ++i)
        {
          EEPROM.write(i, 0);
        }
        delay(10);
        EEPROM.commit();
        delay(10);
        EEPROM.end();
        delay(10);
        WiFi.disconnect();
        delay(3000);
        ESP.restart();
      }
    }
  }

  unsigned long currentMillis = millis();

  //////////////////////
  // Check Connection //
  ///////////////////////////////////////////////////////////////////////////////////////
  if (currentMillis - previouscheck > check) {
    checkConnection();
    previouscheck = currentMillis;
  }

  ///////////////////
  // Check Battery //
  ///////////////////////////////////////////////////////////////////////////////////////
  if (currentMillis - previouscheckBatt > checkBatt) {
    Batt();
    previouscheckBatt = currentMillis;
  }

  //////////////////////
  // Read Bosh Sensor //
  ///////////////////////////////////////////////////////////////////////////////////////
  if (currentMillis - previousReadSensor > ReadSensor) {
    BME280Read();
    previousReadSensor = currentMillis;
  }

  /////////////
  // Send WU //
  ///////////////////////////////////////////////////////////////////////////////////////
  if (currentMillis - previousSendWu > SendWu) {
    WU();
    previousSendWu = currentMillis;
  }

  /////////////
  // Send WX //
  ///////////////////////////////////////////////////////////////////////////////////////
  if (currentMillis - previousSendWx > SendWx) {
    broadcastWX = true;
    APRS();
    broadcastWX = false;
    previousSendWx = currentMillis;
  }

  yield();
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML(temperature, humidity, pressure, feet, meter));
}

void handleSW() {
  if (server.arg("sw") == "Metric") {
    Mode = 1;
  } else {
    Mode = 0;
  }
  server.send(200, "text/html", SendHTML(temperature, humidity, pressure, feet, meter));
}

/////////////////////
// HANDLE NOTFOUND //
////////////////////////////////////////////////////////////
void handleNotFound() {
  server.sendHeader("Location", "/", true);  //Redirect to our html web page
  server.send(302, "text/plane", "");
}

String SendHTML(float temperature, float humidity, float pressure, float feet, float meter) {

  String ptr = (F("<!DOCTYPE html>"));
  ptr += (F("<html>"));
  ptr += (F("<head>"));
  ptr += (F("<title>mini Weather Station</title>"));
  ptr += (F("<meta name='viewport' content='width=device-width, initial-scale=1.0'>"));
  ptr += (F("<link href='https://fonts.googleapis.com/css?family=Open+Sans:300,400,600' rel='stylesheet'>"));
  ptr += (F("<style>"));
  ptr += (F("html { font-family: 'Open Sans', sans-serif; display: block; margin: 0px auto; text-align: center;color: #444444;}"));
  ptr += (F("body{margin: 0px;} "));
  ptr += (F("h1 {margin: 50px auto 30px;} "));
  ptr += (F(".side-by-side{display: table-cell;vertical-align: middle;position: relative;}"));
  ptr += (F(".text{font-weight: 600;font-size: 19px;width: 200px;}"));
  ptr += (F(".reading{font-weight: 300;font-size: 50px;padding-right: 25px;}"));
  ptr += (F(".temperature .reading{color: #F29C1F;}"));
  ptr += (F(".humidity .reading{color: #3B97D3;}"));
  ptr += (F(".pressure .reading{color: #26B99A;}"));
  ptr += (F(".altitude .reading{color: #955BA5;}"));
  ptr += (F(".superscript{font-size: 17px;font-weight: 600;position: absolute;top: 10px;}"));
  ptr += (F(".data{padding: 10px;}"));
  ptr += (F(".container{display: table;margin: 0 auto;}"));
  ptr += (F(".icon{width:65px}"));
  ptr += (button);
  ptr += (F("</style>"));

  // AJAX script
  ptr += (F("<script>\n"));
  ptr += (F("setInterval(loadDoc,1000);\n")); // Update WebPage Every 1sec
  ptr += (F("function loadDoc() {\n"));
  ptr += (F("var xhttp = new XMLHttpRequest();\n"));
  ptr += (F("xhttp.onreadystatechange = function() {\n"));
  ptr += (F("if (this.readyState == 4 && this.status == 200) {\n"));
  ptr += (F("document.body.innerHTML =this.responseText}\n"));
  ptr += (F("};\n"));
  ptr += (F("xhttp.open(\"GET\", \"/\", true);\n"));
  ptr += (F("xhttp.send();\n"));
  ptr += (F("}\n"));
  ptr += (F("</script>\n"));
  ///////////////////////

  ptr += (F("</head>"));
  ptr += (F("<body>"));
  ptr += (F("<h1><p>"));
  ptr += (String)CallSign;
  ptr += (F("</h1></p>"));
  ptr += (F("<p><h2>Weather Station</h2></p>"));

  if (AuthToken.length() < 10) {
    ptr += (F("<p><mark>Blynk AuthToken not Set!</mark></p>"));
  } else {
    if (Connected2Blynk) {
      ptr += (F("<p><mark>Blynk Server Connected!</mark></p>"));
    }
    else {
      ptr += (F("<p><mark>Blynk Server Disconnected!</mark></p>"));
    }
  }

  char buf[21];
  sprintf(buf, "<h2>%02dh:%02dm:%02ds utc</h2>", timeClient.getHours(), timeClient.getMinutes(), timeClient.getSeconds());
  ptr += (F("<p>"));
  ptr += (buf);
  ptr += (F("</p><br>"));

  ptr += (F("<div class='container'>"));
  //////////////////////////////////////////////
  ptr += (F("<div class='data temperature'>"));
  ptr += (F("<div class='side-by-side icon'>"));
  ptr += (F("<svg enable-background='new 0 0 19.438 54.003'height=54.003px id=Layer_1 version=1.1 viewBox='0 0 19.438 54.003'width=19.438px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path d='M11.976,8.82v-2h4.084V6.063C16.06,2.715,13.345,0,9.996,0H9.313C5.965,0,3.252,2.715,3.252,6.063v30.982"));
  ptr += (F("C1.261,38.825,0,41.403,0,44.286c0,5.367,4.351,9.718,9.719,9.718c5.368,0,9.719-4.351,9.719-9.718"));
  ptr += (F("c0-2.943-1.312-5.574-3.378-7.355V18.436h-3.914v-2h3.914v-2.808h-4.084v-2h4.084V8.82H11.976z M15.302,44.833"));
  ptr += (F("c0,3.083-2.5,5.583-5.583,5.583s-5.583-2.5-5.583-5.583c0-2.279,1.368-4.236,3.326-5.104V24.257C7.462,23.01,8.472,22,9.719,22"));
  ptr += (F("s2.257,1.01,2.257,2.257V39.73C13.934,40.597,15.302,42.554,15.302,44.833z'fill=#F29C21 /></g></svg>"));
  ptr += (F("</div>"));
  ptr += (F("<div class='side-by-side text'>Temperature</div>"));
  ptr += (F("<div class='side-by-side reading'>"));

  if (Mode == 0) {
    char Temp[6];
    sprintf(Temp, "%.1f", tFar);
    ptr += String(Temp);
    ptr += (F("<span class='superscript'>&deg;F</span></div>"));
  }
  else {
    char Temp[6];
    sprintf(Temp, "%.1f", temperature);
    ptr += String(Temp);
    ptr += (F("<span class='superscript'>&deg;C</span></div>"));
  }

  ptr += (F("</div>"));
  //////////////////////////////////////////////
  ptr += (F("<div class='data humidity'>"));
  ptr += (F("<div class='side-by-side icon'>"));
  ptr += (F("<svg enable-background='new 0 0 29.235 40.64'height=40.64px id=Layer_1 version=1.1 viewBox='0 0 29.235 40.64'width=29.235px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><path d='M14.618,0C14.618,0,0,17.95,0,26.022C0,34.096,6.544,40.64,14.618,40.64s14.617-6.544,14.617-14.617"));
  ptr += (F("C29.235,17.95,14.618,0,14.618,0z M13.667,37.135c-5.604,0-10.162-4.56-10.162-10.162c0-0.787,0.638-1.426,1.426-1.426"));
  ptr += (F("c0.787,0,1.425,0.639,1.425,1.426c0,4.031,3.28,7.312,7.311,7.312c0.787,0,1.425,0.638,1.425,1.425"));
  ptr += (F("C15.093,36.497,14.455,37.135,13.667,37.135z'fill=#3C97D3 /></svg>"));
  ptr += (F("</div>"));
  ptr += (F("<div class='side-by-side text'>Humidity</div>"));
  ptr += (F("<div class='side-by-side reading'>"));
  ptr += (int)humidity;
  ptr += (F("<span class='superscript'>%</span></div>"));
  ptr += (F("</div>"));
  //////////////////////////////////////////////
  ptr += (F("<div class='data pressure'>"));
  ptr += (F("<div class='side-by-side icon'>"));
  ptr += (F("<svg enable-background='new 0 0 40.542 40.541'height=40.541px id=Layer_1 version=1.1 viewBox='0 0 40.542 40.541'width=40.542px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path d='M34.313,20.271c0-0.552,0.447-1,1-1h5.178c-0.236-4.841-2.163-9.228-5.214-12.593l-3.425,3.424"));
  ptr += (F("c-0.195,0.195-0.451,0.293-0.707,0.293s-0.512-0.098-0.707-0.293c-0.391-0.391-0.391-1.023,0-1.414l3.425-3.424"));
  ptr += (F("c-3.375-3.059-7.776-4.987-12.634-5.215c0.015,0.067,0.041,0.13,0.041,0.202v4.687c0,0.552-0.447,1-1,1s-1-0.448-1-1V0.25"));
  ptr += (F("c0-0.071,0.026-0.134,0.041-0.202C14.39,0.279,9.936,2.256,6.544,5.385l3.576,3.577c0.391,0.391,0.391,1.024,0,1.414"));
  ptr += (F("c-0.195,0.195-0.451,0.293-0.707,0.293s-0.512-0.098-0.707-0.293L5.142,6.812c-2.98,3.348-4.858,7.682-5.092,12.459h4.804"));
  ptr += (F("c0.552,0,1,0.448,1,1s-0.448,1-1,1H0.05c0.525,10.728,9.362,19.271,20.22,19.271c10.857,0,19.696-8.543,20.22-19.271h-5.178"));
  ptr += (F("C34.76,21.271,34.313,20.823,34.313,20.271z M23.084,22.037c-0.559,1.561-2.274,2.372-3.833,1.814"));
  ptr += (F("c-1.561-0.557-2.373-2.272-1.815-3.833c0.372-1.041,1.263-1.737,2.277-1.928L25.2,7.202L22.497,19.05"));
  ptr += (F("C23.196,19.843,23.464,20.973,23.084,22.037z'fill=#26B999 /></g></svg>"));
  ptr += (F("</div>"));
  ptr += (F("<div class='side-by-side text'>Pressure</div>"));
  ptr += (F("<div class='side-by-side reading'>"));

  ptr += (float)pressure;
  ptr += (F("<span class='superscript'>mbar</span></div>"));

  ptr += (F("</div>"));
  //////////////////////////////////////////////
  ptr += (F("<div class='data altitude'>"));
  ptr += (F("<div class='side-by-side icon'>"));
  ptr += (F("<svg enable-background='new 0 0 58.422 40.639'height=40.639px id=Layer_1 version=1.1 viewBox='0 0 58.422 40.639'width=58.422px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path d='M58.203,37.754l0.007-0.004L42.09,9.935l-0.001,0.001c-0.356-0.543-0.969-0.902-1.667-0.902"));
  ptr += (F("c-0.655,0-1.231,0.32-1.595,0.808l-0.011-0.007l-0.039,0.067c-0.021,0.03-0.035,0.063-0.054,0.094L22.78,37.692l0.008,0.004"));
  ptr += (F("c-0.149,0.28-0.242,0.594-0.242,0.934c0,1.102,0.894,1.995,1.994,1.995v0.015h31.888c1.101,0,1.994-0.893,1.994-1.994"));
  ptr += (F("C58.422,38.323,58.339,38.024,58.203,37.754z'fill=#955BA5 /><path d='M19.704,38.674l-0.013-0.004l13.544-23.522L25.13,1.156l-0.002,0.001C24.671,0.459,23.885,0,22.985,0"));
  ptr += (F("c-0.84,0-1.582,0.41-2.051,1.038l-0.016-0.01L20.87,1.114c-0.025,0.039-0.046,0.082-0.068,0.124L0.299,36.851l0.013,0.004"));
  ptr += (F("C0.117,37.215,0,37.62,0,38.059c0,1.412,1.147,2.565,2.565,2.565v0.015h16.989c-0.091-0.256-0.149-0.526-0.149-0.813"));
  ptr += (F("C19.405,39.407,19.518,39.019,19.704,38.674z'fill=#955BA5 /></g></svg>"));
  ptr += (F("</div>"));
  ptr += (F("<div class='side-by-side text'>Altitude</div>"));
  ptr += (F("<div class='side-by-side reading'>"));

  if (Mode == 0) {
    ptr += (int)feet;
    ptr += (F("<span class='superscript'>f</span></div>"));
  }
  else {
    ptr += (int)meter;
    ptr += (F("<span class='superscript'>m</span></div>"));
  }

  ptr += (F("</div>"));
  ptr += (F("</div>"));
  ptr += (F("</body>"));

  if (Mode == 1) {
    ptr += (F("<br><b>min:</b> "));
    ptr += (tMin);
    ptr += (F("&deg;C &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"));
    ptr += (F(" <b> max:</b> "));
    ptr += (tMax);
    ptr += (F("&deg;C "));
  } else {
    float tFmin = (tMin * 9 / 5) + 32;
    float tFmax = (tMax * 9 / 5) + 32;
    ptr += (F("<br><b>min:</b> "));
    ptr += (tFmin);
    ptr += (F("&deg;F &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"));
    ptr += (F(" <b> max:</b> "));
    ptr += (tFmax);
    ptr += (F("&deg;F "));
  }

  ptr += (F("<hr>"));

  ptr += (F("<p><b>Broadcast Status:</b> "));
  ptr += String(broadcastStatus);
  ptr += (F("</p><hr>"));

  if (Mode == 0) {
    ptr += (F("<br><p><a href='sw?sw=Metric' class='button'>Metric</a></p>"));
  } else {
    ptr += (F("<br><p><a href='sw?sw=Imperial' class='button'>Imperial</a></p>"));
  }

  ptr += (F("<br><hr>"));

  if (CallSign.length() > 2) {
    ptr += "<p><h3><a href='https://www.aprs.fi/" + String(CallSign) + "' target='_blank'>https://www.aprs.fi/" + String(CallSign) + "</a></h3></p>";
  }

  if (WUID.length() > 2) {
    ptr += "<p><h3><a href='https://www.wunderground.com/dashboard/pws/" + String(WUID) + "' target='_blank'>https://www.wunderground.com/dashboard/pws/" + String(WUID) + "</a></h3></p>";
  }

  //////////////////////////////////////////////////////////////////
  ptr += (F("<hr>"));
  ptr += (F("<p><b><h2>Blynk GPS Stream</h2></b></p>"));
  if (blat.length() < 1 or blon.length() < 1) {
    ptr += (F("Lat: QRX<br>"));
    ptr += (F("Lon: QRX<br>"));
    ptr += (F("Alt: QRX<br>"));
    ptr += (F("<br><b>To get GPS info,<br>add Blynk V20-(GPS Stream) Widget!</b>"));
  }
  else {
    ptr += (F("Lat: "));
    ptr += (blat);
    ptr += (F("<br>"));
    ptr += (F("Lon: "));
    ptr += (blon);
    ptr += (F("<br>"));
    ptr += (F("Alt: "));
    ptr += (gpsalt);
    ptr += (F(" meter"));

    ptr += (F("<p><a href ='/BlynkGpsStream?lat="));
    ptr += (blat);
    ptr += (F("&lon="));
    ptr += (blon);
    ptr += (F("&alt="));
    ptr += int(gpsalt);

    ptr += (F("'class='button'>Save!</a></p>"));
    ptr += (F("Click Save for use this Location on Weather Station."));
  }
  ptr += (F("<hr>"));

  //////////////////////////////////////////////////////////////////

  ptr += (F("<p><h2>Bosh BME280 Sensor</h2></p>"));
  ptr += (F("<p><a href ='/wifisetting' class='button'>Admin</a></p><br>"));

  if (lowBattery == true) {
    ptr += (F("<p><font color='red'>LOW Battery!: </font>"));
  } else {
    ptr += (F("<p><font color='blue'>Battery: </font>"));
  }
  ptr += (Tvoltage);
  ptr += (F(" v</p>"));

  ptr += (F("<p><font color = 'blue'><i>Signal Strength: </i></font> "));
  ptr += String(percentQ);
  ptr += (F(" %</p>"));

  // System Uptime
  sprintf(timestring, "%d days %02d:%02d:%02d", dddd, hh, mi, ss);
  ptr += (F("<p>System Uptime : "));
  ptr += String(timestring);
  ptr += (F("</p>"));
  ////////////////////////////////////////////////

  ptr += (F("<p><small>mini Weather Station "));
  ptr += (ver);
  ptr += (F("</p><p><small>Made by VE2CUZ</small></p>"));
  ptr += (F("</html>\n"));
  return ptr;
}

//////////////////
// BUILD HEADER //
////////////////////////////////////////////////////////////
void buildHeader() {
  header = "";
  header += (F("<!doctype html>\n"));
  header += (F("<html lang='en'>"));
  header += (F("<head>"));
  //  <!-- Required meta tags -->
  header += (F("<title>mini Weather Station</title>"));
  header += (F("<meta charset='utf-8'>"));
  header += (F("<meta name='viewport' content='width=device-width, initial-scale=1.0' shrink-to-fit=no'>"));
  header += (F("<meta name='description' content='APRS Weather Station'>"));
  header += (F("<meta name='author' content='Made by Real Drouin'>"));
  header += (F("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/4.7.0/css/font-awesome.min.css'>"));
  header += (F("<style>body {margin: 0;text-align: center;font-family: Arial, Helvetica, sans-serif;}"));
  header += (F(".topnav {overflow: hidden;background-color: #333;}"));
  header += (F(".topnav a {float: left;display: block;color: #f2f2f2;text-align: center;padding: 14px 16px;text-decoration: none;font-size: 17px;}"));
  header += (F(".topnav a:hover {background-color: #0066CC;color: black;}"));
  header += (F(".topnav a.active {background-color: blue;color: white;}"));
  header += (F(".topnav .icon {display: none;}"));
  header += (F("@media screen and (max-width: 600px) {.topnav a:not(:first-child) {display: none;}.topnav a.icon {float: right;display: block;}}"));
  header += (F("@media screen and (max-width: 600px) {.topnav.responsive {position: relative;}.topnav.responsive .icon {position: absolute;right: 0;top: 0;}"));
  header += (F(".topnav.responsive a {float: none;display: block;text-align: left;}}"));
  header += String(button);
  header += (F("</style>\n"));
  header += (F("</head>\n"));
  header += (F("<body>\n"));
}

//////////////////
// BUILD FOOTER //
////////////////////////////////////////////////////////////
void buildFooter() {
  footer = (F("<br>"));
  footer += (F("<address> Contact: <a href='mailto:ve2cuz@gmail.com'>VE2CUZ</a>"));
  footer += (F("</address>"));
  footer += (F("<p><small>mini Weater Station "));
  footer += String(ver);
  footer += (F(" made by VE2CUZ</p>"));
  footer += (F("</small>"));
  footer += (F("</footer>"));
  footer += (F("<script>function myFunction() {var x = document.getElementById('myTopnav');if (x.className === 'topnav') {x.className += ' responsive';} else {x.className = 'topnav';}}</script>"));
  footer += (F("</body>\n"));
  footer += (F("</html>\n"));
}

///////////////////
// HANDLE REBOOT //
////////////////////////////////////////////////////////////
void handleREBOOT() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();
  }

  String Spinner = (F("<html>"));
  Spinner += (F("<head><center><meta http-equiv=\"refresh\" content=\"30;URL='http://cwop-wx.local/'\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><style>"));

  Spinner += (F(".loader {border: 16px solid #f3f3f3;border-radius: 50%;border-top: 16px solid #3498db;"));
  Spinner += (F("width: 120px;height: 120px;-webkit-animation: spin 2s linear infinite;animation: spin 2s linear infinite;}"));

  Spinner += (F("@-webkit-keyframes spin {0% { -webkit-transform: rotate(0deg); }100% { -webkit-transform: rotate(360deg); }}"));

  Spinner += (F("@keyframes spin {0% { transform: rotate(0deg); }100% { transform: rotate(360deg); }}"));

  Spinner += (F("</style></head>"));

  Spinner += (F("<body>"));
  Spinner += (F("<br><b><h1 style='font-family:verdana; color:blue; font-size:300%; text-align:center;'>Weather Station</font></h1></b><hr><hr>"));
  Spinner += (F("<p><h2>Rebooting Please Wait...</h2></p>"));
  Spinner += (F("<div class=\"loader\"></div>"));
  Spinner += (F("</body></center></html>"));

  server.send(200, "text/html",  Spinner);

  warning(); // notification

  delay(1000);
  WiFi.disconnect();
  delay(3000);
  ESP.restart();
}

//////////////////
// WIFI SETTING //
////////////////////////////////////////////////////////////
void handleWIFISETTING() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();
  }

  buildHeader();
  webSite = header;
  webSite += (F("<div class='topnav' id='myTopnav'>"));
  webSite += (F("<a href='/'>Exit</a>"));
  webSite += (F("<a href='/wifisetting' class='active'>Wifi</a>"));
  webSite += (F("<a href='/station'>Station</a>"));
  webSite += (F("<a href='/blynk'>Blynk</a>"));
  webSite += (F("<a href='/wu'>WU</a>"));
  webSite += (F("<a href='/reboot' onclick=\"return confirm('Are you sure ? ');\">Reboot</a>"));
  webSite += (F("<a href='/firmware'>Firmware Update</a>"));
  webSite += (F("<a href='javascript:void(0);' class='icon' onclick='myFunction()'>"));
  webSite += (F("<i class='fa fa-bars'></i></a>"));
  webSite += (F("</div>"));
  webSite += (F("<br><b><h1 style='font-family:verdana; color:blue; font-size:300%; text-align:center;'>Weather Station</font></h1></b><hr><hr>"));
  webSite += (F("<h1 style='font-family:verdana; color:blue;'><u>Wireless Network</u></h1>\n"));

  if (WiFi.status() == WL_CONNECTED) {
    String IP = (WiFi.localIP().toString());
    webSite += (F("<p>Network Connected! to <mark>"));
    webSite += WiFi.SSID();
    webSite += (F("</mark></p>"));
    webSite += (F("<p>Ip: "));
    webSite += IP;
    webSite += (F("</p>"));
    webSite += (F("<p>http://cwop-wx.local</p>"));
    webSite += (F("<p><font color=red><b>API:</b></font> http://cwop-wx.local/api</p>"));
  }
  else {
    webSite += (F("<p><font color=red>Network Not Connected!</font></p>"));
  }

  webSite += (F("<hr><p><font color=blue><u>Wifi Scan</u></font></p>"));
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks(false, true);
  // sort by RSSI
  int indices[n];
  for (int i = 0; i < n; i++) {
    indices[i] = i;
  }
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
        std::swap(indices[i], indices[j]);
      }
    }
  }
  String st = "";
  if (n > 5) n = 5;
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<small><li>";
    st += WiFi.RSSI(indices[i]);
    st += " dBm, ";
    st += WiFi.SSID(indices[i]);
    st += "</small></li>";
  }
  webSite += (F("<p>"));
  webSite += st;
  webSite += (F("</p>"));
  //// WiFi SSID & PASSWORD
  webSite += (F("<hr><hr><h1 style='font-family:verdana; color:blue;'><u>Wifi Ssid & Pass</u></h1>\n"));
  webSite += (F("<form method='get' action='Wifi'><label>SSID: </label><input name='ssid' type='text' maxlength=32><br><br><label>PASS: </label><input name='pass' type='text' maxlength=32><br><br><input type='submit'></form>"));
  webSite += (F("<br><p><font color=red><b>Reset:</b></font> Push on Button for 10sec to active Setup Mode,</p>"));
  webSite += (F("<p>Weather Station reboot in Setup Mode, SSID cwop-wx-setup.</p>"));
  webSite += (F("<p>Ip: 192.168.4.1</p>"));
  webSite += (F("<hr><hr>"));
  buildFooter();
  webSite += footer;
  server.send(200, "text/html",  webSite);

  warning(); //notification
}

//////////////////
// HANDLE BLYNK //
////////////////////////////////////////////////////////////
void handleBLYNK() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();
  }

  buildHeader();
  webSite = header;
  webSite += (F("<div class='topnav' id='myTopnav'>"));
  webSite += (F("<a href=/ >Exit</a>"));
  webSite += (F("<a href='/wifisetting'>Wifi</a>"));
  webSite += (F("<a href='/station'>Station</a>"));
  webSite += (F("<a href='/blynk' class='active'>Blynk</a>"));
  webSite += (F("<a href='/wu'>WU</a>"));
  webSite += (F("<a href=/reboot onclick=\"return confirm('Are you sure?');\">Reboot</a>"));
  webSite += (F("<a href=/firmware >Firmware Update</a>"));
  webSite += (F("<a href='javascript:void(0);' class='icon' onclick='myFunction()'>"));
  webSite += (F("<i class='fa fa-bars'></i></a>"));
  webSite += (F("</div>"));
  webSite += (F("<br><b><h1 style='font-family:verdana; color:blue; font-size:300%; text-align:center;'>Weather Station</font></h1></b><hr><hr>"));

  // BLYNK CONFIG

  webSite += (F("<h1 style='font-family:verdana;color:blue;'><u>Blynk AuthToken</u></h1>\n"));
  webSite += (F("<p><form method='get' action='Blynk'><label>AuthToken: </label><input type='password' name='key' maxlength='32' value="));
  webSite += String(AuthToken);
  webSite += (F("><input type='submit'></form></p>\n"));

  webSite += (F("<hr><h1 style='font-family:verdana;color:blue;'><u>Blynk Server</u></h1>\n"));
  webSite += (F("<p><form method='get' action='BlynkServer'><label>Server: </label><input type='text' name='server' maxlength='32' value="));

  if (BlynkServer.length() > 5) {
    webSite += String(BlynkServer);
  }
  else {
    webSite += (F("blynk-cloud.com"));
  }
  webSite += (F("><label> Port: </label><input type='number' name='port' maxlength='4' value="));
  if (BlynkServer.length() > 5) {
    webSite += String(BlynkPort);
  }
  else {
    webSite += (F("8442"));
  }

  webSite += (F("><p><input type='submit'></form></p>\n"));
  webSite += (F("<br><p><font color=red><b>Note:</b></font> Enter your Blynk server address or leave empty to use default Blynk server.</p>"));
  webSite += (F("<p><font color=red><b>Virtual Variable:</b></font> V0:(Temp C), V1:(Temp F), V2:(Hum), V3:(mbar), V4:(Alt-Feet), V5:(Alt-Meter), V6:(Vbatt), V7:(Wifi-Strength),</p>"));
  webSite += (F("<p>V10:(Terminal), V20:(GPS Stream), (Notification), (Email).</p>"));
  webSite += (F("<hr><hr>"));

  buildFooter();
  webSite += footer;
  server.send(200, "text/html",  webSite);

  warning(); //notification
}

///////////////
// HANDLE WU //
////////////////////////////////////////////////////////////
void handleWU() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();
  }

  buildHeader();
  webSite = header;
  webSite += (F("<div class='topnav' id='myTopnav'>"));
  webSite += (F("<a href=/ >Exit</a>"));
  webSite += (F("<a href='/wifisetting'>Wifi</a>"));
  webSite += (F("<a href='/station'>Station</a>"));
  webSite += (F("<a href='/blynk'>Blynk</a>"));
  webSite += (F("<a href=/wu class='active'>WU</a>"));
  webSite += (F("<a href=/reboot onclick=\"return confirm('Are you sure?');\">Reboot</a>"));
  webSite += (F("<a href=/firmware >Firmware Update</a>"));
  webSite += (F("<a href='javascript:void(0);' class='icon' onclick='myFunction()'>"));
  webSite += (F("<i class='fa fa-bars'></i></a>"));
  webSite += (F("</div>"));
  webSite += (F("<br><b><h1 style='font-family:verdana; color:blue; font-size:300%; text-align:center;'>Weather Station</font></h1></b><hr><hr>"));

  // WU CONFIG
  webSite += (F("<h1 style='font-family:verdana;color:blue;'><u>Weather Underground PWS</u></h1>\n"));
  webSite += (F("<p><form method='get' action='WU'><label>ID: </label><input type='text' name='id' maxlength='16' value="));
  webSite += (WUID);
  webSite += (F("><label> KEY: </label><input type='password' name='key' maxlength='16' value="));
  webSite += (WUKEY);
  webSite += (F("><p><input type='submit'></form></p>\n"));

  webSite += (F("<p><font color=red><b>Note:</b></font> Enter your WU <b>ID</b> and <b>KEY</b> or leave empty to disable WU.</p>"));

  if (WUID.length() > 2 and WUKEY.length() > 2) {
    webSite += (F("<p>Update value every 6min.</p>"));
    webSite += (F("<hr><p><a href='/broadcast?WU=1' class='button'>Broadcast Test!</a></p>"));
  }
  webSite += (F("<hr><hr>"));

  buildFooter();
  webSite += footer;
  server.send(200, "text/html",  webSite);

  warning(); //notification
}

////////////////////
// HANDLE STATION //
////////////////////////////////////////////////////////////
void handleSTATION() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!server.authenticate("admin", password.c_str()))
      return server.requestAuthentication();
  }

  buildHeader();
  webSite = header;
  webSite += (F("<div class='topnav' id='myTopnav'>"));
  webSite += (F("<a href=/ >Exit</a>"));
  webSite += (F("<a href=/wifisetting>Wifi</a>"));
  webSite += (F("<a href=/station class='active'>Station</a>"));
  webSite += (F("<a href=/blynk>Blynk</a>"));
  webSite += (F("<a href=/wu>WU</a>"));
  webSite += (F("<a href=/reboot onclick=\"return confirm('Are you sure?');\">Reboot</a>"));
  webSite += (F("<a href=/firmware >Firmware Update</a>"));
  webSite += (F("<a href='javascript:void(0);' class='icon' onclick='myFunction()'>"));
  webSite += (F("<i class='fa fa-bars'></i></a>"));
  webSite += (F("</div>"));
  webSite += (F("<br><b><h1 style='font-family:verdana; color:blue; font-size:300%; text-align:center;'>Weather Station</font></h1></b><hr><hr>"));

  webSite += (F("<h1 style='font-family:verdana;color:blue;'><u>CWOP-WX Station</u></h1>\n"));
  webSite += (F("<form method='get' action='Station'><label>CWOP ID: </label><input oninput='this.value = this.value.toUpperCase()' name='callsign' type='text' maxlength='6' value='"));
  webSite += String(CallSign);
  webSite += (F("'> <a href='http://www.findu.com/citizenweather/cw_form.html' target='_blank'> REGISTRATION FORM.</a>"));
  webSite += (F("<br><br>"));
  webSite += (F("<label>Password:  </label><input name='password' type='number' maxlength='6' value='"));
  webSite += String(Password);
  webSite += (F("'> <a href='https://apps.magicbug.co.uk/passcode/index.php' target='_blank'> CWOP Passcode Generator.</a><br><br>"));
  webSite += (F("<br><br>"));
  webSite += (F("<h2 style='font-family:verdana;color:blue;'><u>Altitude Info</u></h2>\n"));
  webSite += (F("<p><font color=red><b>Note:</b></font> Set '0' to get <b>approx</b> altitude from bosch sensor,</p>"));
  webSite += (F("<p>for more <b>accurency</b> set manually.</p>"));
  webSite += (F("<p>Altitude affects the value of the barometer.</p>"));
  webSite += (F("<label>Alt in meter: </label><input name='alt' type='number' min='0' maxlength='4' value='"));
  webSite += int(epromALTITUDE);
  webSite += (F("'><br><br>"));
  webSite += (F("<label>Lat: </label><input oninput='this.value = this.value.toUpperCase()' name='lat' type='text' maxlength='10' value='"));
  webSite += String(Lat);
  webSite += (F("'> ex: 4545.99N<br><br>"));
  webSite += (F("<label>Lon: </label><input oninput='this.value = this.value.toUpperCase()' name='lon' type='text' maxlength='10' value='"));
  webSite += String(Lon);
  webSite += (F("'> ex: 07400.79W<br><br><br>"));
  webSite += (F("<label>Comment: </label><input name='comment' type='text' maxlength='32' value='"));
  webSite += String(Comment);
  webSite += (F("'><br><br><br>"));
  webSite += (F("<input type='submit'></form>"));
  webSite += (F("<br><hr><br><h2><p><a href='https://www.geoplaner.com/' target='_blank'> www.geoplaner.com</a></p></h2><br>"));

  webSite += (F("<hr><p><a href='/broadcast?APRS=1' class='button'>Broadcast Test!</a></p>"));
  webSite += (F("<hr>"));

  webSite += (F("<h1 style='font-family:verdana;color:blue;'><u>Battery Cal</u></h1>\n"));

  webSite += (F("<p><font color=red><b>Note: </b></font>Read the Battery Voltage with a Multimeter,</p>"));
  webSite += (F("<p>After, Input value in box.</p>"));
  webSite += (F("<p>MAX 13volts !</p>"));

  webSite += (F("<p><form method='get' action='Battery'><label>Battery Voltage Reading value</label><br><input type=number min='0' max='13.00' step='.01' name='BattVoltage' maxlength='5' ><br><br><input type='submit'></form></p>\n"));
  webSite += (F("<hr><hr>"));

  buildFooter();
  webSite += footer;
  server.send(200, "text/html",  webSite);

  warning(); //notification
}

//////////////////////////////////////////
// Broadcast to APRS Server Every 10min //
/////////////////////////////////////////////////
void APRS() {
  WiFiClient client;
  String readString;
  client.setTimeout(3000); // reduce delay executing get response

  ///////// Broadcast to APRS ////////////
  if (CallSign.length() > 2) {

    broadcastStatus = (F("Connecting to APRS Server..."));

    if (client.connect("cwop.aprs.net" , 14580)) {

      String response = "";

      int timeout = millis() + 5000;
      while (client.available() == 0) {
        if (timeout - millis() < 0) {
          delay(1);
          client.flush();
          client.stop();

          terminal.println();
          terminal.println(F("======================================"));
          terminal.println(F("FAIL! TimeOut to Connect APRS Server!"));
          terminal.println(F("======================================"));
          terminal.flush();

          broadcastStatus = (F("FAIL! TimeOut to Connect to APRS Server!"));

          broadcastWX = false;
          return;
        }
      }

      while (client.available()) {
        response = client.readStringUntil('\r');

        if (response.indexOf("APRS") > 0 or response.indexOf("aprs") > 0)
        {
          broadcastStatus = (F("Connected!"));

          if (broadcastWX == true) {
            String Login = ("user " + (CallSign) + " pass " + (Password.c_str()) + " vers ESP8266Weather 1");
            client.println(Login);
            delay(10);
          }

        }

        if (response.indexOf("unverified") > 0)
        {
          delay(1);
          client.flush();
          client.stop();

          terminal.println();
          terminal.println(F("======================================"));
          terminal.println(F("FAIL! , Check Password!"));
          terminal.println(F("======================================"));
          terminal.flush();

          broadcastStatus = (F("FAIL! , Check Password!"));

          broadcastWX = false;
          return;
        }

        if (response.indexOf("verified") > 0)
        {
          if (broadcastWX == true) {

            int Far = round(tFar);

            String response = "";
            readString = "";

            //////// Temperature Process ///////

            String temp;
            temp = "";

            if (Far <= 9 && Far >= 1)
            {
              temp = String("00");
              temp += (Far);
            }

            else if (Far >= 10)
            {
              temp = String("0");
              temp += (Far);
            }

            else if (Far == 0)
            {
              temp = String("000");
            }

            else if (Far >= -9 && Far <= -1)
            {
              int pos = (Far * (-1));
              temp = String("-0");
              temp += (pos);
            }

            else if (Far <= -10)
            {
              int pos = (Far * (-1));
              temp = String("-");
              temp += (pos);
            }
            ///////// Humidity Process //////////
            String hum;
            hum = "";

            if (Hum <= 9 && Hum >= 1)
            {
              hum = String("0");
              hum += (Hum);
            }

            if (Hum >= 10 && Hum <= 99)
            {
              hum = (Hum);
            }

            if (Hum >= 100)
            {
              hum = String("00");
            }

            ////////// Bar Process /////////
            String bar;
            bar = "";

            if (Bar >= 10000)
            {
              bar = (Bar);
            }
            else {
              bar = String("0");
              bar += (Bar);
            }

            char utc[21];
            sprintf(utc, "%02d%02d%02dz", timeClient.getHours(), timeClient.getMinutes(), timeClient.getSeconds());

            String aprsString = "";
            aprsString += (CallSign);
            aprsString += ">APRS,TCPIP:@";
            aprsString += utc;
            aprsString += (Lat.c_str());
            aprsString += "/";
            aprsString += (Lon.c_str());
            aprsString += "_.../...g...t";
            aprsString += temp;
            aprsString += "r...p...P...h";
            aprsString += hum;
            aprsString += "b";
            aprsString += bar;
            aprsString += (Comment);

            client.println(aprsString);
            delay(10);

            terminal.println();
            terminal.println(F("======================================"));
            terminal.println(F("CWOP Updated! :-)"));
            terminal.println(F("======================================"));
            terminal.flush();

            broadcastStatus = (F("CWOP Updated! :-)"));
          }
        }
      }
    }
    else {
      terminal.println();
      terminal.println(F("======================================"));
      terminal.println(F("FAIL!"));
      terminal.println(F("======================================"));
      terminal.flush();

      broadcastStatus = (F("FAIL!"));
    }
  }
  else {
    terminal.println();
    terminal.println(F("======================================"));
    terminal.println(F("FAIL! CWOP ID not Set!"));
    terminal.println(F("======================================"));
    terminal.flush();

    broadcastStatus = (F("FAIL! CWOP ID not Set!"));
  }

  delay(1);
  client.flush();
  client.stop();

  broadcastWX = false;
}

//////////////////////
// Bosh BME280 Read //
////////////////////////////////////////////////
void BME280Read() {
  timeClient.update();
  percentQ = 0; // Maximum signal strength, you are probably standing right next to the access point. -50 dBm.
  // Anything down to this level can be considered excellent signal strength.

  if (WiFi.RSSI() <= -100) {
    percentQ = 0;
  } else if (WiFi.RSSI() >= -50) {
    percentQ = 100;
  } else {
    percentQ = 2 * (WiFi.RSSI() + 100);
  }

  //////// APRS VARIABLE /////////////
  bme.takeForcedMeasurement(); // has no effect in normal mode

  temperature = bme.readTemperature(); // in Celcius
  tFar = (temperature * 9 / 5) + 32; //Convert Celcius to Fahrenheit

  // Min,Max Temp Value
  if (temperature > tMax) {
    tMax = temperature;
  }

  if (temperature < tMin) {
    tMin = temperature;
  }

  humidity = bme.readHumidity();
  Hum = humidity;

  if (epromALTITUDE < 1) {
    ALTITUDE = bme.readAltitude(SEALEVELPRESSURE_HPA);
  } else {
    ALTITUDE = epromALTITUDE;
  }

  pressure = bme.readPressure();
  pressure = bme.seaLevelForAltitude(ALTITUDE, pressure);
  pressure = pressure / 100.0F;
  Bar = pressure * 10;

  ////////////////////////////////////
  // Variable Display on WebPage

  feet = 0;
  feet = round(ALTITUDE * 3.28084); //Convert Meter to Feet
  meter = 0;
  meter = round(ALTITUDE);

  if (Connected2Blynk) {

    Blynk.virtualWrite(V0, temperature);
    Blynk.virtualWrite(V5, int(meter));

    Blynk.virtualWrite(V1, tFar);
    Blynk.virtualWrite(V4, int(feet));

    Blynk.virtualWrite(V2, humidity);
    Blynk.virtualWrite(V3, pressure);

    Blynk.virtualWrite(V7, percentQ);
  }
}

//////////////////////
// Check Connection //
//////////////////////////////////////////////////////////////
void checkConnection() {

  if (Setup == false and WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    delay(10);
    WiFi.mode(WIFI_STA);
    delay(10);
    WiFi.hostname("APRS-WX");
    delay(10);
    WiFi.begin(ssid.c_str(), password.c_str());
  }

  if (Setup == false) {
    if (!Connected2Blynk) {
      if (BlynkServer.length() > 0) {
        Blynk.config(AuthToken.c_str(), BlynkServer.c_str(), BlynkPort);
      }
      else {
        Blynk.config(AuthToken.c_str(), "blynk-cloud.com", 8442);
      }
      Blynk.connect(3333);  // timeout set to 10 seconds and then continue without Blynk
    }
  }
}

/////////////
// Warning //
//////////////////////////////////////////////////////////////
void warning() {
  String addy = server.client().remoteIP().toString();

  terminal.println();
  terminal.println(F("***** Warning !!! *****"));
  terminal.println(F("Client connect to ADMIN!"));
  terminal.print(F("client IP: "));
  terminal.println(addy);
  terminal.println(F("***** Warning !!! *****"));
  terminal.flush();
}

/////////////////////////
// Weather Underground //
//////////////////////////////////////////////////////////////
void WU()
{
  WiFiClient client;
  client.setTimeout(3000); // reduce delay executing get response

  broadcastStatus = (F("Connecting to WU Server..."));
  terminal.flush();

  if (!client.connect(wuhost, 80)) {
    terminal.println(F("======================================"));
    terminal.println(F("Fail! not Connected to WU Server!"));
    terminal.println(F("======================================"));
    terminal.flush();

    broadcastStatus = (F("Fail! not Connected to WU Server!"));
    return;
  }

  float binches = (Bar / 10 * 0.02953); //convert mbar to inHg

  char WUBAR[8];
  sprintf(WUBAR, "%.4f", binches);

  char WUTFAR[6];
  sprintf(WUTFAR, "%.1f", tFar);

  float DewPoint = (temperature - (100 - Hum) / 5);   //  dewpoint calculation using Celsius value

  float DP = (DewPoint * 9 / 5) + 32;     //  converts dewPoint calculation to fahrenheit


  if ((WUID.length() > 2) and (WUKEY.length() > 2)) {

    wurl = "/weatherstation/updateweatherstation.php?ID=";
    wurl += String(WUID);
    wurl += (F("&PASSWORD="));
    wurl += String(WUKEY);
    wurl += (F("&dateutc=now"));
    wurl += (F("&humidity="));
    wurl += String(Hum);
    wurl += (F("&tempf="));
    wurl += String(WUTFAR);
    wurl += (F("&baromin="));
    wurl += String(WUBAR);
    wurl += (F("&dewptf="));
    wurl += String(DP);
    wurl += (F("&realtime=1&rtfreq=60&action=updateraw"));

    client.print(String("GET ") + wurl + " HTTP/1.1\r\n" +
                 "Host: " + wuhost + "\r\n" +
                 "User-Agent: G6EJDFailureDetectionFunction\r\n" +
                 "Connection: close\r\n\r\n");

    delay(1);
    client.flush();
    client.stop();

    terminal.println();
    terminal.println(F("======================================"));
    terminal.println(F("WU Updated! :-)"));
    terminal.println(F("======================================"));
    terminal.flush();

    broadcastStatus = (F("WU Updated! :-)"));
  }
  else {
    terminal.println();
    terminal.println(F("======================================"));
    terminal.println(F("Fail, check! WUID and/or WUKEY."));
    terminal.println(F("======================================"));
    terminal.flush();

    broadcastStatus = (F("Fail, check! WUID or WUKEY."));
  }
}

void Batt() {
  /////////////////////////////////////Battery Voltage//////////////////////////////////

  for (unsigned int i = 0; i < 10; i++) {
    Vvalue = Vvalue + analogRead(BAT);     // Read analog Voltage
    delay(5);                              // ADC stable
  }
  Vvalue = (float)Vvalue / 10.0;           // Find average of 10 values
  Rvalue = (float)(Vvalue / 1024.0) * 5;   // Convert Voltage in 5v factor
  Tvoltage = Rvalue * RatioFactor;         // Find original voltage by multiplying with factor

  Blynk.virtualWrite(V6, Tvoltage);

  if ((Tvoltage < lowNotification) and (lowBattery == false)) {
    Blynk.notify("mini Weather Station, Alert! Low Battery!");
    Blynk.email("mini Weather Station", "Alert! Low Battery!");
    lowBattery = true;
  }
  else if (Tvoltage >= lowNotification) {
    lowBattery = false;
  }
}

void info() {
  terminal.println();
  terminal.println(F("======================================"));
  terminal.print(F("mini Weather Station "));
  terminal.println(ver);
  terminal.println(F("======================================"));
  terminal.println(F("type ? to display status"));
}

