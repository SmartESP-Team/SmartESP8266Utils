#ifndef SMART_ESP8266_UTILS_H
#define SMART_ESP8266_UTILS_H


#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

// WebSocket
AsyncWebSocket ws("/ws");

// ---------------- STRUCT ----------------
struct AutoUpdatePayload {
  String ecran1 = "";
  String ecran2 = "";
  String ecran3 = "";
  String ecran4 = "";
  String ecran5 = "";
  String ecran6 = "";
  float indicateur1 = 0.0;
  float indicateur2 = 0.0;
  float indicateur3 = 0.0;
  float indicateur4 = 0.0;
  String bulb1 = "";
  String bulb2 = "";
  String bulb3 = "";
  String bulb4 = "";
  String notif = "false";
  String ia = "false";
  String ac = "";
  String statusMessage = "";
};

// ---------------- WIFI ----------------
void connectToWiFi(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  Serial.print("Connexion au WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

// ---------------- WEBSOCKET ----------------
void onWebSocketEvent(
  AsyncWebSocket* server,
  AsyncWebSocketClient* client,
  AwsEventType type,
  void* arg,
  uint8_t* data,
  size_t len,
  String* lastCommandPtr) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Client WebSocket #%u connecté depuis %s\n",
        client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Client WebSocket #%u déconnecté\n", client->id());
      break;
    case WS_EVT_DATA: {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        *lastCommandPtr = (char*)data;
        Serial.printf("Commande reçue : %s\n", (char*)data);
      }
      break;
    }
  }
}

void setupWebSocket(AsyncWebServer& server, String* lastCommandPtr) {
  ws.onEvent([lastCommandPtr](AsyncWebSocket* server,
    AsyncWebSocketClient* client,
    AwsEventType type,
    void* arg,
    uint8_t* data,
    size_t len) {
    onWebSocketEvent(server, client, type, arg, data, len, lastCommandPtr);
  });
  server.addHandler(&ws);
  server.begin();
}

// ---------------- AUTO UPDATE ----------------
void sendAutoUpdate(const AutoUpdatePayload& payload) {
  StaticJsonDocument<512> doc;

  doc["Ecran1"] = payload.ecran1.isEmpty() ? String(millis()) : payload.ecran1;
  doc["Ecran2"] = payload.ecran2;
  doc["Ecran3"] = payload.ecran3;
  doc["Ecran4"] = payload.ecran4;
  doc["Ecran5"] = payload.ecran5;
  doc["Ecran6"] = payload.ecran6;

  doc["Indicateur1"] = payload.indicateur1;
  doc["Indicateur2"] = payload.indicateur2;
  doc["Indicateur3"] = payload.indicateur3;
  doc["Indicateur4"] = payload.indicateur4;

  doc["Bulb1"] = payload.bulb1;
  doc["Bulb2"] = payload.bulb2;
  doc["Bulb3"] = payload.bulb3;
  doc["Bulb4"] = payload.bulb4;

  doc["notif"] = payload.notif;
  doc["AI"] = payload.ia;
  doc["Audio"] = payload.ac;
  doc["message"] = payload.statusMessage;

  doc["status"] = "auto-update";
  doc["Wifi"] = WiFi.SSID();
  doc["ip_address"] = WiFi.localIP().toString();
  doc["mac_address"] = WiFi.macAddress();
  doc["signal_strength"] = WiFi.RSSI();
  doc["gateway_ip"] = WiFi.gatewayIP().toString();
  doc["free_memory"] = ESP.getFreeHeap();
  doc["flash_memory"] = ESP.getFlashChipRealSize();

  String output;
  serializeJson(doc, output);
  ws.textAll(output);
}

void processCommandIfNeeded(String& lastCommand, AutoUpdatePayload& payload) {
  if (!lastCommand.isEmpty()) {
    sendAutoUpdate(payload);
    payload.notif = "false";
    payload.ia = "false";
    payload.ac = "";
    lastCommand = "";
  }
}

// ---------------- EMAIL ----------------



//...

void sendEmail(String email, String subject, String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi non connecté");
    return;
  }

  HTTPClient http;
  BearSSL::WiFiClientSecure client;
  client.setInsecure(); // Ignore les certificats SSL (non sécurisé mais simple)

  String url = "https://script.google.com/macros/s/AKfycbwYkg3mOneVbI7umotIWl5-_AJNOMRte1UMSzCZ5758GzLc74cltNjeORuedDbfRNXh/exec";
  String postData = "email=" + email + "&subject=" + subject + "&message=" + message;

  http.begin(client, url);  
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpCode = http.POST(postData);
  String response = http.getString();

  Serial.print("Code HTTP : ");
  Serial.println(httpCode);
  Serial.println("Réponse du serveur : " + response);

  http.end();
}


// ---------------- NOTIFY ONCE ----------------
struct NotifyFlag {
  String msg;
  bool state;
};
static NotifyFlag notifyFlags[10];
static int notifyCount = 0;

void notifyOnce(AutoUpdatePayload &payload, const String &msg, bool condition) {
  int idx = -1;
  for (int i = 0; i < notifyCount; i++) {
    if (notifyFlags[i].msg == msg) { idx = i; break; }
  }
  if (idx == -1 && notifyCount < 10) {
    idx = notifyCount++;
    notifyFlags[idx].msg = msg;
    notifyFlags[idx].state = false;
  }

  if (idx >= 0) {
    if (condition && !notifyFlags[idx].state) {
      payload.notif = msg;
      sendAutoUpdate(payload);
      notifyFlags[idx].state = true;
    } else if (!condition) {
      notifyFlags[idx].state = false;
    }
  }
}

// ---------------- EMAIL ONCE ----------------
struct EmailFlag {
  String msg;
  bool state;
};
static EmailFlag emailFlags[10];
static int emailCount = 0;

void sendEmailOnce(String toEmail, String subject, String message, const String &flagName, bool condition) {
  int idx = -1;
  for (int i = 0; i < emailCount; i++) {
    if (emailFlags[i].msg == flagName) { idx = i; break; }
  }
  if (idx == -1 && emailCount < 10) {
    idx = emailCount++;
    emailFlags[idx].msg = flagName;
    emailFlags[idx].state = false;
  }

  if (idx >= 0) {
    if (condition && !emailFlags[idx].state) {
      sendEmail(toEmail, subject, message);
      emailFlags[idx].state = true;
    } else if (!condition) {
      emailFlags[idx].state = false;
    }
  }
}

#endif



// notifyOnce(payload, "nono", smokeValue > 500);
// sendEmailOnce("ermes1643@gmail.com", "Fumée détectée", "La valeur a dépassé 500", "smoke_alert", smokeValue > 500);
 
