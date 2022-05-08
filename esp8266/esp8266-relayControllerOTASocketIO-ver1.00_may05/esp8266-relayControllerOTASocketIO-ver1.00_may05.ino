#include "arduino_secrets.h"

//---------------------------------------------------------------------------------
// Server & Sketch Update
//---------------------------------------------------------------------------------
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#define SKETCH_VERSION "1.00"

//---------------------------------------------------------------------------------
// SocketIO
//---------------------------------------------------------------------------------
//#if !defined(ESP8266)
//#error This code is intended to run only on the ESP8266 boards ! Please check your Tools->Board setting.
//#endif
//
//#define DEBUG_WEBSOCKETS_PORT  Serial
//#define _WEBSOCKETS_LOGLEVEL_  4

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ArduinoJson.h>

#include <WebSocketsClient_Generic.h>
#include <SocketIOclient_Generic.h>

ESP8266WiFiMulti WiFiMulti;
SocketIOclient socketIO;
WiFiClient client;

// -----------------------------------------------------------------------------------------------------
// WiFi Settings
// -----------------------------------------------------------------------------------------------------
char serverIP[] = WEBSOCKETS_SERVER_HOST;
uint16_t  serverPort = WEBSOCKETS_SERVER_PORT;
String update_host = UPDATE_SERVER_HOST;
int update_port = UPDATE_SERVER_PORT;
String update_path = UPDATE_SERVER_PATH;

int status = WL_IDLE_STATUS;

char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;         // your network password (use for WPA, or use as key for WEP), length must be 8+
unsigned long statusCheck_time;
const unsigned long statusCheckInterval = 30L * 60L * 1000L;

// -----------------------------------------------------------------------------------------------------
// Time & Reset Timer
// -----------------------------------------------------------------------------------------------------
int resetTime_hh = 3;
int resetTime_mm = 30;
int Time_BKK_hh;
int Time_BKK_mm;

// -----------------------------------------------------------------------------------------------------
// Relay Control
// -----------------------------------------------------------------------------------------------------

// Relay Name
const char* user_agent = "user-agent: esp8266-5V2R_power_control";
boolean relay_1;
boolean relay_2;
boolean isChange_Relay_1;
boolean isChange_Relay_2;

// 1st Relay
byte SendData_Open_1[] = {0xA0, 0x01, 0x01, 0xA2}; // A00101A2
byte SendData_Close_1[] = {0xA0, 0x01, 0x00, 0xA1}; // A00100A1

// 2nd Relay
byte SendData_Open_2[] = {0xA0, 0x02, 0x01, 0xA3}; // A00201A3
byte SendData_Close_2[] = {0xA0, 0x02, 0x00, 0xA2}; // A00200A2


// -----------------------------------------------------------------------------------------------------
// Event Handler
// -----------------------------------------------------------------------------------------------------
String socket_ID;
String socket_username;
String socket_address;

// -----------------------------------------------------------------------------------------------------

void setup() {

  Serial.begin(115200);
  delay(200);

  //---------------------------------------------------------------------------------
  // Connect to WiFi
  //---------------------------------------------------------------------------------

  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);

  WiFiMulti.addAP(ssid, pass);

  Serial.print(F("Connecting to ")); Serial.println(ssid);

  //WiFi.disconnect();
  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(F("."));
    delay(500);
  }

  printWifiStatus();
  Serial.println();

  //---------------------------------------------------------------------------------
  // Checking Sketch Version
  //---------------------------------------------------------------------------------
  Serial.print(F("Sketch version: "));
  Serial.println(SKETCH_VERSION);
  handleSketchDownload();

  //---------------------------------------------------------------------------------
  // SocketIO
  //---------------------------------------------------------------------------------

  Serial.print(F("\nStart WebSocketClientSocketIO on ")); Serial.println(BOARD_NAME);
  Serial.println(WEBSOCKETS_GENERIC_VERSION);

  // Client address
  Serial.print(F("WebSockets Client started @ IP address: "));
  Serial.println(WiFi.localIP());

  // server address, port and URL
  Serial.print(F("Connecting to WebSockets Server @ IP address: "));
  Serial.print(serverIP);
  Serial.print(F(", port: "));
  Serial.println(serverPort);

  // setReconnectInterval to 10s, new from v2.5.1 to avoid flooding server. Default is 0.5s
  socketIO.setReconnectInterval(10000);

  socketIO.setExtraHeaders(user_agent);

  // server address, port and URL
  // void begin(IPAddress host, uint16_t port, String url = "/socket.io/?EIO=4", String protocol = "arduino");
  // To use default EIO=4 from v2.5.1
  socketIO.begin(serverIP, serverPort);

  // event handler
  socketIO.onEvent(socketIOEvent);
}


void loop() {

  //---------------------------------------------------------------------------------
  // SocketIO
  //---------------------------------------------------------------------------------
  socketIO.loop();

  //---------------------------------------------------------------------------------
  // Relay Control
  //---------------------------------------------------------------------------------
  if (isChange_Relay_1) {
    if (relay_1) {
      Serial.write(SendData_Open_1, sizeof(SendData_Open_1));
    }
    if (!relay_1) {
      Serial.write(SendData_Close_1, sizeof(SendData_Close_1));
    }
    isChange_Relay_1 = false;
    delay(1000);
  }

  if (isChange_Relay_2) {
    if (relay_2) {
      Serial.write(SendData_Open_2, sizeof(SendData_Open_2));
    }
    if (!relay_2) {
      Serial.write(SendData_Close_2, sizeof(SendData_Close_2));
    }
    isChange_Relay_2 = false;
    delay(1000);
  }

  //---------------------------------------------------------------------------------
  // Time & Reset timer
  //---------------------------------------------------------------------------------
  if ( Time_BKK_hh >= resetTime_hh && Time_BKK_mm >= resetTime_mm || ( millis()/(1000*3600) )>72 ) {
      ESP.restart();
  }

  //---------------------------------------------------------------------------------
  // WiFi Status Checking & Reconnect
  //---------------------------------------------------------------------------------
  if (millis() - statusCheck_time > statusCheckInterval) {
    statusCheck_time = millis();
    if (WiFiMulti.run() != WL_CONNECTED) {
      WiFi.disconnect();
      WiFi.begin(ssid, pass);
    }
  }

}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print(F("WebSockets Client IP Address: "));
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print(F("signal strength (RSSI):"));
  Serial.print(rssi);
  Serial.println(F(" dBm"));
}

void socketIOEvent(const socketIOmessageType_t& type, uint8_t * payload, const size_t& length) {

  // Extract Event
  DynamicJsonDocument doc2(1024);
  String eventName;


  switch (type) {

    case sIOtype_DISCONNECT:
      Serial.println(F("[IOc] Disconnected"));

      break;

    case sIOtype_CONNECT:
      Serial.print(F("[IOc] Connected to url: "));
      Serial.println((char*) payload);

      // join default namespace (no auto join in Socket.IO V3)
      socketIO.send(sIOtype_CONNECT, "/");

      break;

    case sIOtype_EVENT:
      Serial.print(F("[IOc] Get event: "));
      Serial.println((char*) payload);


      // -----------------------------------------------------------------------------------------------------
      // Get Event name
      // -----------------------------------------------------------------------------------------------------

      // Deserialize the JSON document
      deserializeJson(doc2, payload);

      eventName = doc2[0].as<String>();
      //      Serial.print("__Debug eventName: "); Serial.println(eventName);


      // -----------------------------------------------------------------------------------------------------
      // On joined event
      // -----------------------------------------------------------------------------------------------------

      if ( eventName == "joined" ) {

        JsonObject Array_1;
        Array_1 = doc2[1];
        socket_ID = Array_1["id"].as<String>();
        socket_username = Array_1["username"].as<String>();
        socket_address = Array_1["address"].as<String>();

        //        Serial.print("socket_ID: "); Serial.println(socket_ID);
        //        Serial.print("socket_username: "); Serial.println(socket_username);
        //        Serial.print("socket_address: "); Serial.println(socket_address);
      }

      // -----------------------------------------------------------------------------------------------------
      // On control_relay event
      // -----------------------------------------------------------------------------------------------------

      // Relay 1
      if ( eventName == "control_relay_1" ) {

        JsonObject Array_1;
        Array_1 = doc2[1];
        String socket_ID_req = Array_1["id"];
        String socket_username_req = Array_1["username"];

        Serial.println(F("Look up for Device ID & Username..."));
        if ( String(socket_ID).equals(String(socket_ID_req)) && String(socket_username).equals(String(socket_username_req)) ) {

          Serial.println(F("ID & Username Found!"));

          if (strcmp(Array_1["relay1"].as<const char*>(), "true") == 0) {
            relay_1 = true;
            isChange_Relay_1 = true;
            Serial.println(F("Relay_1: ON"));
            delay(1000);
          }

          if (strcmp(Array_1["relay1"].as<const char*>(), "false") == 0) {
            relay_1 = false;
            isChange_Relay_1 = true;
            Serial.println(F("Relay_1: OFF"));
            delay(1000);
          }

        } else {
          Serial.println(F("ID & Username not found or error"));
        }
      }

      // Relay 2
      if ( eventName == "control_relay_2" ) {

        JsonObject Array_1;
        Array_1 = doc2[1];
        String socket_ID_req = Array_1["id"];
        String socket_username_req = Array_1["username"];

        Serial.println(F("Look up for Device ID & Username..."));
        if ( String(socket_ID).equals(String(socket_ID_req)) && String(socket_username).equals(String(socket_username_req)) ) {

          Serial.println(F("ID & Username Found!"));

          if (strcmp(Array_1["relay2"].as<const char*>(), "true") == 0) {
            relay_2 = true;
            isChange_Relay_2 = true;
            Serial.println(F("Relay_2: ON"));
            delay(1000);
          }

          if (strcmp(Array_1["relay2"].as<const char*>(), "false") == 0) {
            relay_2 = false;
            isChange_Relay_2 = true;
            Serial.println(F("Relay_2: OFF"));
            delay(1000);
          }

        } else {
          Serial.println(F("ID & Username not found or error"));
        }
      }

      // -----------------------------------------------------------------------------------------------------
      // On restart board
      // -----------------------------------------------------------------------------------------------------
      if ( eventName == "restart_board" ) {

        JsonObject Array_1;
        Array_1 = doc2[1];
        String socket_ID_req = Array_1["id"];
        String socket_username_req = Array_1["username"];

        Serial.println(F("Look up for Device ID & Username..."));
        if ( String(socket_ID).equals(String(socket_ID_req)) && String(socket_username).equals(String(socket_username_req)) ) {
          Serial.println(F("ID & Username Found!"));

          ESP.restart();
        } else {
          Serial.println(F("ID & Username not found or error"));
        }
      }

      // -----------------------------------------------------------------------------------------------------
      // On update sketch
      // -----------------------------------------------------------------------------------------------------
      if ( eventName == "update_sketch" ) {

        JsonObject Array_1;
        Array_1 = doc2[1];
        String socket_ID_req = Array_1["id"];
        String socket_username_req = Array_1["username"];

        Serial.println(F("Look up for Device ID & Username..."));
        if ( String(socket_ID).equals(String(socket_ID_req)) && String(socket_username).equals(String(socket_username_req)) ) {
          Serial.println(F("ID & Username Found!"));

          handleSketchDownload();
        } else {
          Serial.println(F("ID & Username not found or error"));
        }
      }

      // -----------------------------------------------------------------------------------------------------
      // On update sketch path
      // -----------------------------------------------------------------------------------------------------
      if ( eventName == "update_sketch_path" ) {

        JsonObject Array_1;
        Array_1 = doc2[1];
        String socket_ID_req = Array_1["id"];
        String socket_username_req = Array_1["username"];

        Serial.println(F("Look up for Device ID & Username..."));
        if ( String(socket_ID).equals(String(socket_ID_req)) && String(socket_username).equals(String(socket_username_req)) ) {
          Serial.println(F("ID & Username Found!"));

          update_host      = Array_1["UPDATE_SERVER_HOST"].as<String>();
          update_port      = Array_1["UPDATE_SERVER_PORT"].as<int>();
          update_path      = Array_1["UPDATE_SERVER_PATH"].as<String>();

        }
      }

      // -----------------------------------------------------------------------------------------------------
      // On time event
      // -----------------------------------------------------------------------------------------------------
      if ( eventName == "get_time" ) {

        JsonObject Array_1;
        Array_1 = doc2[1];
        String socket_ID_req = Array_1["id"];
        String socket_username_req = Array_1["username"];

        Serial.println(F("Look up for Device ID & Username..."));
        if ( String(socket_ID).equals(String(socket_ID_req)) && String(socket_username).equals(String(socket_username_req)) ) {
          Serial.println(F("ID & Username Found!"));

          Time_BKK_hh      = Array_1["Time_BKK_hh"];
          Time_BKK_mm      = Array_1["Time_BKK_mm"];

        }
      }


      // -----------------------------------------------------------------------------------------------------

      break;

    //    case sIOtype_ACK:
    //      Serial.print("[IOc] Get ack: ");
    //      Serial.println(length);
    //
    //      //hexdump(payload, length);
    //
    //      break;
    //
    //    case sIOtype_ERROR:
    //      Serial.print("[IOc] Get error: ");
    //      Serial.println(length);
    //
    //      //hexdump(payload, length);
    //
    //      break;
    //
    //    case sIOtype_BINARY_EVENT:
    //      Serial.print("[IOc] Get binary: ");
    //      Serial.println(length);
    //
    //      //hexdump(payload, length);
    //
    //      break;
    //
    //    case sIOtype_BINARY_ACK:
    //      Serial.print("[IOc] Get binary ack: ");
    //      Serial.println(length);
    //
    //      //hexdump(payload, length);
    //
    //      break;
    //
    //    case sIOtype_PING:
    //      Serial.println("[IOc] Get PING");
    //
    //
    //      break;
    //
    //    case sIOtype_PONG:
    //      Serial.println("[IOc] Get PONG");
    //
    //
    //      break;

    default:
      break;
  }
}

void handleSketchDownload() {

  if ((WiFiMulti.run() == WL_CONNECTED)) {

    // The line below is optional. It can be used to blink the LED on the board during flashing
    // The LED will be on during download of one buffer of data from the network. The LED will
    // be off during writing that buffer to flash
    // On a good connection the LED should flash regularly. On a bad connection the LED will be
    // on much longer than it will be off. Other pins than LED_BUILTIN may be used. The second
    // value is used to put the LED on. If the LED is on with HIGH, that value should be passed
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

    // Add optional callback notifiers
    ESPhttpUpdate.onStart(update_started);
    ESPhttpUpdate.onEnd(update_finished);
    ESPhttpUpdate.onProgress(update_progress);
    ESPhttpUpdate.onError(update_error);

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, update_host, update_port, update_path, SKETCH_VERSION);
    // Or:
    //t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");

    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println(F("HTTP_UPDATE_NO_UPDATES, ON RECENT VERSION!"));
        break;

      case HTTP_UPDATE_OK:
        Serial.println(F("HTTP_UPDATE_OK"));
        break;
    }
  }
}

void update_started() {
  Serial.println(F("CALLBACK:  HTTP update process started"));
}

void update_finished() {
  Serial.println(F("CALLBACK:  HTTP update process finished"));
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}
