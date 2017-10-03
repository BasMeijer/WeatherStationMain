// LIBRARIES IMPORTS
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MQ135.h"

// PIN DEFINITIONS

// GAS SENSOR DEFINITIONS
#define ANALOGPIN A0    //  Define Analog PIN on Arduino Board
#define RZERO 169.18
float ppm;
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
#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif


HTTPClient http;

//const char *ssid = "#";
//const char *password = "#";
byte ledPin = 2;
char ssid[] = "#";               // SSID of your home WiFi
char password[] = "#";               // password of your home WiFi

String httpPostUrl = "http://iot-open-server.herokuapp.com/data";
String apiToken = "#";



WiFiServer server(80);                    
IPAddress ip(192, 168, 0, 80);            // IP address of the server
IPAddress gateway(192,168,2,254);           // gateway of your network
IPAddress subnet(255,255,255,0);          // subnet mask of your network


void setup() {
  Serial.begin(115200);
  pinMode(IR, INPUT); 
  setupWebServer();
    
//  connectToWifiNetwork();
}

void loop() {

  receiveClientMessages();
  
  float rzero = gasSensor.getRZero();
  Serial.print("MQ135 RZERO Calibration Value : ");
  Serial.println(rzero);

  ppm = gasSensor.getPPM();
  digitalWrite(13,HIGH);
  Serial.print("CO2 ppm value : ");
  Serial.println(ppm);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& jsonRoot = jsonBuffer.createObject();
  
  jsonRoot["token"] = apiToken;
  
  JsonArray& data = jsonRoot.createNestedArray("data");

  JsonObject& airqualityObject = jsonBuffer.createObject();
  airqualityObject["key"] = "ppm";
  airqualityObject["value"] = ppm;
  
  data.add(airqualityObject);

  String dataToSend;
  jsonRoot.printTo(dataToSend);

  Serial.println(dataToSend);
  
  detection = digitalRead(IR);
  if(detection == LOW){
    showDisplay(ppm);
  }
  
//  postData(dataToSend);
  delay(500);
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

//  if ( MDNS.begin ( "esp8266" ) ) {
//    Serial.println ( "MDNS responder started" );
//  }
}

void postData(String stringToPost) {
  if(WiFi.status()== WL_CONNECTED){
    http.begin(httpPostUrl);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(stringToPost);
    String payload = http.getString();
  
    Serial.println(httpCode);
    Serial.println(payload);
    http.end();
  } else {
    Serial.println("Wifi connection failed, retrying.");   
    connectToWifiNetwork();
  }
}

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
  display.println(ppm);

  display.setTextColor(WHITE);
  display.println("-------------");

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Humidity:");

  display.setTextSize(1);
  display.setTextColor(BLACK, WHITE); // 'inverted' text
  display.println(String(ppm));
  
  
  display.display();
  delay(10000);
  display.clearDisplay();
  display.display();
}

void setupWebServer(){
  WiFi.config(ip, gateway, subnet);       // forces to use the fix IP
  WiFi.begin(ssid, password);                 // connects to the WiFi router
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  server.begin();                         // starts the server
  Serial.println("Connected to wifi");
  Serial.print("Status: "); Serial.println(WiFi.status());  // some parameters from the network
  Serial.print("IP: ");     Serial.println(WiFi.localIP());
  Serial.print("Subnet: "); Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: "); Serial.println(WiFi.gatewayIP());
  Serial.print("SSID: "); Serial.println(WiFi.SSID());
  Serial.print("Signal: "); Serial.println(WiFi.RSSI());
  Serial.print("Networks: "); Serial.println(WiFi.scanNetworks());
  pinMode(ledPin, OUTPUT);
}

void receiveClientMessages(){
  WiFiClient client = server.available();
  if (client) {
    if (client.connected()) {
      digitalWrite(ledPin, LOW);  // to show the communication only (inverted logic)
      Serial.println(".");
      String request = client.readStringUntil('\r');    // receives the message from the client
      Serial.print("From client: "); Serial.println(request);
      client.flush();
      client.println("Hi client! No, I am listening.\r"); // sends the answer to the client
      digitalWrite(ledPin, HIGH);
    }
    client.stop();                // tarminates the connection with the client
  }
}

