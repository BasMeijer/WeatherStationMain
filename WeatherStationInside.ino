// ----------------------------------------------------
// LIB IMPORTS
// ----------------------------------------------------
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "SH1106.h"
#include "SH1106Ui.h"
#include "MQ135.h"
#include <Adafruit_NeoPixel.h>

// ----------------------------------------------------
// PIN & SENSOR DEFINITIONS 
// ----------------------------------------------------

// GAS SENSOR
#define ANALOGPIN A0    //  Define Analog PIN on Arduino Board
#define RZERO 169.18
MQ135 gasSensor = MQ135(ANALOGPIN);

// OBJECT DETECTION
#define IR D2 
int detection = HIGH;    // no obstacle

// LCD DISPLAY
#define OLED_DC     D4
#define OLED_CS     D8
#define OLED_RESET  D0

SH1106 display(true, OLED_RESET, OLED_DC, OLED_CS); // FOR SPI
SH1106Ui ui     ( &display );

// LED STRIP 
#define PIN D6
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, PIN, NEO_GRB + NEO_KHZ800);

// ----------------------------------------------------
// DATA VARIABLES
// ----------------------------------------------------

// SENSOR READINGS
float humidity;
float temperature;
float airpressure;
float pressureAltitude;
String raining;
float ppm;
float airspeed;

//String actuatorTrigger;

// ----------------------------------------------------
// LCD DISPLAY FUNCTIONS & VARIABLES
// ----------------------------------------------------

bool drawFrame1(SH1106 *display, SH1106UiState* state, int x, int y) {

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 5 + y, "DHT - Temp/Hum");

  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 15 + y, "------------------------------");

  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 25 + y, ("Temp: " + String(temperature) + " °C"));

  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 40 + y, ("Hum: " + String(humidity) + " %"));

  return false;
}

bool drawFrame2(SH1106 *display, SH1106UiState* state, int x, int y) {
  
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 5 + y, "Air - Speed/Pressure");

  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 15 + y, "------------------------------");

  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 25 + y, ("Airspeed: " + String(airspeed) + " km/h"));

  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 40 + y, ("Pressure: " + String(airpressure) + " Pa"));

  return false;
}

bool drawFrame3(SH1106 *display, SH1106UiState* state, int x, int y) {
  
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 5 + y, "Weather Outside");

  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 15 + y, "------------------------------");

  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 25 + y, ("Condition: " + raining));

  return false;
}

bool drawFrame4(SH1106 *display, SH1106UiState* state, int x, int y) {
  
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 5 + y, "Airquality Inside");

  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 15 + y, "------------------------------");

  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 25 + y, ("CO2 PPM: " + String(ppm)));

  return false;
}

int frameCount = 4;
bool (*frames[])(SH1106 *display, SH1106UiState* state, int x, int y) = { drawFrame1,drawFrame2,drawFrame3,drawFrame4 };

// ----------------------------------------------------
// WIFI & NETWORK SETTINGS
// ----------------------------------------------------

HTTPClient http;
byte ledPin = 2;
char ssid[] = "PinguHotspot";               // SSID of your home WiFi
char password[] = "basbas1337";               // password of your home WiFi

String httpPostUrl = "http://iot-open-server.herokuapp.com/data";
String actuatorUrl = "http://iot-open-server.herokuapp.com/actuator/59b26059f728010004dd60a7";
String apiToken = "b69d453c30c82a48619472ac";
String deviceId = "59b26059f728010004dd60a7";

WiFiServer server(80);
IPAddress ip(192, 168, 43, 198);            // IP address of the server
IPAddress gateway(192, 168, 0, 1);        // gateway of your network
IPAddress subnet(255, 255, 255, 0);       // subnet mask of your network

unsigned long clientTimer = 0;
unsigned long actuatorTimer = 0;

// ----------------------------------------------------
// HOMESTATION STARTUP FUNCTIONS
// ----------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("Running Setup...");

  connectToWifiNetwork();
    
  pinMode(IR, INPUT); 
  
  ui.setTargetFPS(30);
  ui.setTimePerFrame(2000);
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setFrames(frames, frameCount);
  ui.init();

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  
  delay(5000);
}


// ----------------------------------------------------
// MAIN LOOP FUNCTIONALITY
// ----------------------------------------------------

void loop() {

  receiveClientMessages();

  ui.update();      // Update UI of the oled screen
    
  if (millis() - actuatorTimer > 10000) {    // stops and restarts the WiFi server after 30 sec
    getActuator();
  }
  
  if (millis() - clientTimer > 30000) {    // stops and restarts the WiFi server after 30 sec
    Serial.println("Restarting Server");
    WiFi.disconnect();                     // idle time
    delay(5000);
    server_start(1);
  }
  
  delay(1000);
}


// ----------------------------------------------------
// MAIN FUNCTIONS
// ----------------------------------------------------

void receiveClientMessages(){
  WiFiClient client = server.available();
  if (client) {
    if (client.connected()) {
      Serial.println(".");            // Dot Dot try connecting
      String request = client.readStringUntil('\r');    // receives the message from the client

      ppm = gasSensor.getPPM();
      String humidityval = getValue(request, ';', 0);
      String temperatureval = getValue(request, ';', 1);
      String airpressureval = getValue(request, ';', 2);
      String rainingval = getValue(request, ';', 3);
      String airspeedval = getValue(request, ';', 4);

      humidity = humidityval.toFloat();
      temperature = temperatureval.toFloat();
      airpressure = airpressureval.toFloat();
      airspeed = airspeedval.toFloat();

      if(rainingval.equals("1")){
        raining = "Raining";
      }else if(rainingval.equals("0")){
        raining = "Dry";
      }

      Serial.println("------ RECEIVED DATA -----");
      Serial.print("From client: "); Serial.println(request);
      Serial.println(humidityval);
      Serial.println(temperatureval);
      Serial.println(airpressureval);
      Serial.println(rainingval);
      Serial.println(airspeedval);
      Serial.println("---------------------------");

      ui.update();      // Update UI of the oled screen
      
      client.flush();
    }
    client.stop();                // tarminates the connection with the client
    clientTimer = millis();
    
    String dataToSend = createDataString();
    Serial.println(dataToSend);
  
    postData(dataToSend);
  }  
}


String createDataString(){
  StaticJsonBuffer<600> jsonBuffer;
  JsonObject& jsonRoot = jsonBuffer.createObject();
  
  jsonRoot["token"] = apiToken;
  
  JsonArray& data = jsonRoot.createNestedArray("data");

  JsonObject& airqualityObject = jsonBuffer.createObject();
  airqualityObject["key"] = "ppm";
  airqualityObject["value"] = ppm;

  JsonObject& humidityObject = jsonBuffer.createObject();
  humidityObject["key"] = "humidity";
  humidityObject["value"] = humidity;

  JsonObject& temperatureObject = jsonBuffer.createObject();
  temperatureObject["key"] = "temperature";
  temperatureObject["value"] = temperature;

  JsonObject& airpressureObject = jsonBuffer.createObject();
  airpressureObject["key"] = "airpressure";
  airpressureObject["value"] = airpressure;

  JsonObject& rainingObject = jsonBuffer.createObject();
  rainingObject["key"] = "raining";
  rainingObject["value"] = raining;

  JsonObject& airspeedObject = jsonBuffer.createObject();
  airspeedObject["key"] = "airspeed";
  airspeedObject["value"] = airspeed;
  
  data.add(airqualityObject);
  data.add(humidityObject);
  data.add(temperatureObject);
  data.add(airpressureObject);
  data.add(rainingObject);
  data.add(airspeedObject);

  String dataToSend;
  jsonRoot.printTo(dataToSend);

  return dataToSend;
}

// ----------------------------------------------------
// WIFI & NETWORK FUNCTIONS
// ----------------------------------------------------

void server_start(byte restart) {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  server.begin();
  delay(500);
  clientTimer = millis();
}

void connectToWifiNetwork() {
  Serial.println ( "Trying to establish WiFi connection" );
  WiFi.begin ( ssid, password );
  Serial.println ( "" );
  
  // Wait for connection
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }

  Serial.println ( "" );
  Serial.print ( "Connected to " );
  Serial.println ( ssid );
  Serial.print ( "IP address: " );
  Serial.println ( WiFi.localIP() );
}

void postData(String stringToPost) {
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(httpPostUrl);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(stringToPost);
    String payload = http.getString();

    Serial.println(httpCode);
    Serial.println(payload);
    Serial.println("POST DATA SUCCESFUL!");

    http.end();
  } else {
    Serial.println("Wifi connection failed, retrying.");
    connectToWifiNetwork();
  }
}

void getActuator(){
  actuatorTimer = millis();
  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
    http.begin(actuatorUrl);  //Specify request destination
    int httpCode = http.GET();                                                                  //Send the request
 
    if (httpCode > 0) { //Check the returning code
      String payload = http.getString();   //Get the request response payload
      Serial.println("Actuator payload: ");
      Serial.println(payload);

      StaticJsonBuffer<300> JSONBuffer;
      JsonObject&  parsed= JSONBuffer.parseObject(payload);
      
      const char * actuatorTrigger = parsed["trigger"];
      
      if(String(actuatorTrigger) == "true"){
        Serial.println("trigger is true");  
        setActuatorInactive();

        delay(1000);
        //   Ledstrip init
        strip.begin();
        strip.show(); // Initialize all pixels to 'off'
      
        colorWipe(strip.Color(0, 0, 100), 100); // Blue
      
        colorWipe(strip.Color(0, 0, 0), 100); // Off
        strip.show(); // Initialize all pixels to 'off'
    
      }
    }
    
    http.end();   //Close connection
  }  
}

void setActuatorInactive(){

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& jsonRoot = jsonBuffer.createObject();
  jsonRoot["trigger"] = "false";
  
  String dataToSend;
  jsonRoot.printTo(dataToSend);
  
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(actuatorUrl);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(dataToSend);
    String payload = http.getString();

    Serial.println(httpCode);
    Serial.println(payload);
    Serial.println("POST ACTUATOR SUCCESFUL!");

    http.end();
  } else {
    Serial.println("Wifi connection failed, retrying.");
    connectToWifiNetwork();
  }
}

// ----------------------------------------------------
// HELPER FUNCTIONS
// ----------------------------------------------------

// Gets the values from a String with seperated data
String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}


