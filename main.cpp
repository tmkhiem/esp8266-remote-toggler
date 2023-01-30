#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>

WebSocketsServer webSocket(81);    // create a websocket server on port 81
ESP8266WebServer server(80);

unsigned long lastOn = 0;
uint8_t pongResponse[6] = {'p', 'o', 'n', 'g', '|', 'x'};
bool toggle = false;

void startWebSocket();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t payload_size);
void handleRoot();
void handleJQuery();
String getContentType(String filename);
bool handleFileRead(String path);

void setup() {
  digitalWrite(D1, LOW);
  pinMode(D1, OUTPUT);
  digitalWrite(D1, LOW);

  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);         // Start the Serial communication to send messages to the computer
  delay(10);

  WiFi.mode(WIFI_AP);
  WiFi.softAP("Gutmann D", "assembler");
  Serial.print("AP Started: ");
  Serial.println(WiFi.softAPIP());

  int i = 0;

  digitalWrite(LED_BUILTIN, LOW);

  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer

  startWebSocket();

  SPIFFS.begin();

  server.onNotFound([]() {
    if (!handleFileRead(server.uri()))
      server.send(404, "text/plain", "404: Not Found");
  });
  server.begin();

  if (MDNS.begin("esp8266.local")) {              // Start the mDNS responder for esp8266.local
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  webSocket.loop();
  server.handleClient();

  unsigned long current = millis();
  if (digitalRead(D1) && (current - lastOn > 2000)) {
    digitalWrite(D1, LOW);
    Serial.println("Auto OFF");
  }
}

void startWebSocket() { // Start a WebSocket server
  webSocket.begin();                          // start the websocket server
  webSocket.onEvent(webSocketEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t payload_size) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }
      break;
    case WStype_TEXT:                     // if new text data is received
      Serial.printf("[%u] rcvd (%d): %s\n", num, payload_size, payload);
      if (payload_size != 4)
      {
        Serial.println("Invalid format!");
        return;
      }

      if      ((payload[0] == 'p') &&
               (payload[1] == 'i') &&
               (payload[2] == 'n') &&
               (payload[3] == 'g')) {
        pongResponse[5] = digitalRead(D1) ? '1' : '0';
        webSocket.sendTXT(num, pongResponse, 6, false);
        Serial.printf("[%u] PONG\n", num);
        break;
      }

      if      ((payload[0] == 'o') &&
               (payload[1] == 'n') &&
               (payload[2] == 'o') &&
               (payload[3] == 'n')) {
        Serial.printf("[%u] ON \n", num);
        lastOn = millis();
        digitalWrite(D1, HIGH);
        break;
      }

      if      ((payload[0] == 'o') &&
               (payload[1] == 'f') &&
               (payload[2] == 'f') &&
               (payload[3] == 'f')) {
        Serial.printf("[%u] OFF \n", num);
        digitalWrite(D1, LOW);
        break;
      }
  }
}

String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  return "text/plain";
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)

  Serial.println("handleFileRead: " + path);

  if (path.endsWith("/"))
    path += "index.html";         // If a folder is requested, send the index file

  String contentType = getContentType(path);            // Get the MIME type

  if (SPIFFS.exists(path)) {                            // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    size_t sent = server.streamFile(file, contentType); // And send it to the client
    file.close();                                       // Then close the file again
    return true;
  }
  Serial.println("\tFile Not Found");
  return false;                                         // If the file doesn't exist, return false
}
