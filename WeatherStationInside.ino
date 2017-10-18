// ----------------------------------------------------
// LIB IMPORTS
// ----------------------------------------------------
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MQ135.h"

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
Adafruit_SSD1306 display(OLED_DC, OLED_RESET, OLED_CS);
#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2

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

// ----------------------------------------------------
// WIFI & NETWORK SETTINGS
// ----------------------------------------------------

HTTPClient http;
byte ledPin = 2;
char ssid[] = "PinguHotspot";               // SSID of your home WiFi
char password[] = "basbas1337";               // password of your home WiFi

String httpPostUrl = "http://iot-open-server.herokuapp.com/data";
String apiToken = "b69d453c30c82a48619472ac";

WiFiServer server(80);
IPAddress ip(192, 168, 43, 198);            // IP address of the server
IPAddress gateway(192, 168, 0, 1);        // gateway of your network
IPAddress subnet(255, 255, 255, 0);       // subnet mask of your network

unsigned long clientTimer = 0;


// ----------------------------------------------------
// HOMESTATION STARTUP FUNCTIONS
// ----------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("Running Setup...");
  
  pinMode(IR, INPUT); 
  
  connectToWifiNetwork();
  
  delay(5000);
}


// ----------------------------------------------------
// MAIN LOOP FUNCTIONALITY
// ----------------------------------------------------

void loop() {

  receiveClientMessages();
  
  detection = digitalRead(IR);
  
  if(detection == LOW){
    showDisplay(ppm);
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
      digitalWrite(LED_BUILTIN, LOW);  // to show the communication only (inverted logic)
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
// LCD DISPLAY FUNCTIONS
// ----------------------------------------------------

void showDisplay(float ppm){
    // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC);

  // Clear the buffer.
  display.clearDisplay();

  // text display tests
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Hello, Bas!");
  display.println("-------------");

  display.setTextColor(WHITE);
  display.println("Temperature:");

  display.setTextSize(1.5);
  display.setTextColor(BLACK, WHITE); // 'inverted' text
  display.println(temperature);

  display.setCursor(0,15);
  display.setTextColor(WHITE);
  display.println("-------------");

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Humidity:");

  display.setTextSize(1);
  display.setTextColor(BLACK, WHITE); // 'inverted' text
  display.println(String(humidity));
  
  display.display();
  delay(5000);
  display.clearDisplay();
  display.display();
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

