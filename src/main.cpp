#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <FS.h>
#include "ArduinoJson.h"  //Version 6.15.2
#include "stm32ota.h"

#define ARDUINOJSON_USE_LONG_LONG 1

stm32ota STM32(5, 4, 2);  //For use with libray STM32OTA

const char* ssid = "DEPPED";  //you ssid
const char* password = "123maria";  //you password
const char* link_Updt = link_Updt = "https://raw.githubusercontent.com/TiagoMagSilva/TesteOTA_STM/main/ESP32UpdateInstruction.txt";
char link_bin[100];
boolean MandatoryUpdate = false;
//----------------------------------------------------------------------------------
const int buttonPin = 9;
const int ledPin = 2;
boolean aux = false;
unsigned long lastTime;
int button = true;

//----------------------------------------------------------------------------------
void wifiConnect() {
  SerialPC.println("");
  WiFi.disconnect(true);  
  WiFi.mode(WIFI_STA);
  delay(2000);  //Aguarda a estabilizaçao do modulo.
  WiFi.begin(ssid, password);
  byte b = 0;
  while (WiFi.status() != WL_CONNECTED && b < 60) {  //Tempo de tentativa de conecxao - 60 segundos
    b++;
    SerialPC.print(".");
    delay(500);
  }
  SerialPC.println("");
  SerialPC.print("IP:");
  SerialPC.println(WiFi.localIP());
}
//----------------------------------------------------------------------------------
void checkupdt(boolean all = true) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, link_Updt);
  int httpCode = http.GET();
  String s = "";
  s = http.getString();
  http.end();
  s.trim();

  if (all) {
    SerialPC.println(s);  //usar apenas no debug
  }


  if (httpCode != HTTP_CODE_OK) {
    return;
  }

  StaticJsonDocument<800> doc;
  deserializeJson(doc, s);
  strlcpy(link_bin, doc["link"] | "", sizeof(link_bin));
  MandatoryUpdate = doc["mandatory"] | false;
  SerialPC.println(link_bin);  //For debug only
  //Debug.println(MandatoryUpdate);                   //For debug only
}

//----------------------------------------------------------------------------------
void setup() 
{
  SerialPC.begin(9600);  //For debug only
  SerialESP.begin(9600);
  SerialPC.println("DEBUG SOFTWARESERIAL");
  
  if(!SPIFFS.begin(true))
  {
    SerialPC.println("Erro ao montar sistema de arquivo SPIFFS");
  }

  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT);
  delay(200);
  wifiConnect(); 
  delay(200);
  //STM32.RunMode();
  
  checkupdt();
  SerialPC.println("END OF INITIALIZATION");
}

void loop() {

  button = digitalRead(buttonPin);
  if (!button) {
    digitalWrite(ledPin, HIGH);
    SerialPC.println("START UPDATE");
    delay(2000);
    checkupdt(false);
    String myString = String(link_bin);
    SerialPC.println(STM32.otaUpdate(myString));  //For debug only
    SerialPC.println("END OF UPDT");              //For debug only
  }
  //_______________________________________________________________________

  if (millis() - lastTime > 500) {             //BLINK LED BULTIN
    if (aux) {
      aux = false;
    } else aux = true;
    lastTime = millis();
    digitalWrite(ledPin, aux);
  }
}