#include "SmartESP8266Utils.h"
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

const char* ssid = "N1";
const char* password = "yannyann";

AsyncWebServer server(80);
String lastCommand = "";
AutoUpdatePayload payload;
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 500;

// ------- GPIO ESP8266 (adapter si besoin) --------

bool smokeAlert = false ;

void setup() {
  Serial.begin(115200);
  connectToWiFi(ssid, password);
  setupWebSocket(server, &lastCommand);

 
}

void loop() {
 


  Autoupdate();
}

void Autoupdate() {
  processCommandIfNeeded(lastCommand, payload);

  if (millis() - lastUpdateTime > updateInterval) {
    detect();

    sendAutoUpdate(payload);
    payload.notif = "false";
    payload.ia = "false";
    payload.ac = "";
    lastUpdateTime = millis();
  }
}


void detect(){
  
   int yann = analogRead(A0);
   float i = yann/ 1024.0 ;
  payload.indicateur1 = i ;
  payload.ecran1 = yann ;


}
