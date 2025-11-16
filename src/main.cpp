/* Code From:
 * https://randomnerdtutorials.com/esp32-access-point-ap-web-server/
 * https://randomnerdtutorials.com/esp32-websocket-server-arduino/
 * https://RandomNerdTutorials.com/esp32-websocket-server-sensor/  // Using LittleFS
*/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h> 

// Change the ssid to your preferred WiFi network name
// Password not needed if you want the AP (Access Point) to be open
const char* ssid = "ESP32-Access-Point";
const char* password = "12345678";

// Define LED GPIO pins
const int ledPin1 = 2;
const int ledPin2 = 4;
const int ledPin3 = 16;

// LED states
bool ledState1 = LOW;
bool ledState2 = LOW;
bool ledState3 = LOW;

// Configure an IP address for the Access Point
IPAddress ap_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress  subnet(255, 255, 255, 0);

AsyncWebServer server(80); // AsyncWebServer object on port 80
AsyncWebSocket ws("/ws"); // WebSocket server at /ws
JsonDocument receivedJson; // Json object to hold received data from clients

// HTML content for the web page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32 LED Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 0px auto; padding-top: 20px;}
    .button { background-color: #4CAF50; border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 4px 2px; cursor: pointer; }
    .button.off { background-color: #f44336; }
  </style>
</head>
<body>
  <h1>ESP32 LED Control</h1>
  <p>LED 1 State: <span id="ledState1">OFF</span></p>
  <button class="button" id="button1" onclick="toggleLED(1)">Toggle LED 1</button>
  <p>LED 2 State: <span id="ledState2">OFF</span></p>
  <button class="button" id="button2" onclick="toggleLED(2)">Toggle LED 2</button>
  <p>LED 3 State: <span id="ledState3">OFF</span></p>
  <button class="button" id="button3" onclick="toggleLED(3)">Toggle LED 3</button>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;

    window.addEventListener('load', onLoad);

    function onLoad(event) {
      initWebSocket();
    }

    function initWebSocket() {
      console.log('Trying to open a WebSocket connection...');
      websocket = new WebSocket(gateway);
      websocket.onopen    = onOpen;
      websocket.onclose   = onClose;
      websocket.onmessage = onMessage; // <-- add this line
    }

    function onOpen(event) {
      console.log('Connection opened');
    }

    function onClose(event) {
      console.log('Connection closed');
      setTimeout(initWebSocket, 2000); // Reconnect after 2 seconds
    }

    function onMessage(event) {
      var message = JSON.parse(event.data);
      if (message.id == 1) {
        document.getElementById('ledState1').innerHTML = message.state == 1 ? "ON" : "OFF";
        document.getElementById('button1').classList.toggle('off', message.state == 0);
      } else if (message.id == 2) {
        document.getElementById('ledState2').innerHTML = message.state == 1 ? "ON" : "OFF";
        document.getElementById('button2').classList.toggle('off', message.state == 0);
      } else if (message.id == 3) {
        document.getElementById('ledState3').innerHTML = message.state == 1 ? "ON" : "OFF";
        document.getElementById('button3').classList.toggle('off', message.state == 0);
      }
    }

    function toggleLED(ledId) {
      websocket.send(JSON.stringify({ id: ledId }));
    }
  </script>
</body>
</html>
)rawliteral";

void notifyClients() {
  String jsonMessage1 = "{\"id\":1,\"state\":" + String(ledState1) + "}";
  String jsonMessage2 = "{\"id\":2,\"state\":" + String(ledState2) + "}";
  String jsonMessage3 = "{\"id\":3,\"state\":" + String(ledState3) + "}";
  ws.textAll(jsonMessage1);
  ws.textAll(jsonMessage2);
  ws.textAll(jsonMessage3);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    Serial.println(message);

    deserializeJson(receivedJson, message);

    int ledId = receivedJson["id"];

    if (ledId == 1) {
      ledState1 = !ledState1;
    } else if (ledId == 2) {
      ledState2 = !ledState2;
    } else if (ledId == 3) {
      ledState3 = !ledState3;
    }
    notifyClients();
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      notifyClients(); // Send initial states to newly connected client
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.begin();
}

// Initialize Wi-Fi Access Point and Web Server
void initAP() {
  Serial.print("Setting AP (Access Point)…");
  WiFi.mode(WIFI_AP);
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);
  if (!WiFi.softAPConfig(ap_ip, gateway, subnet)) {
    Serial.println("AP configuration failed");
  } else {
    // Print ESP IP Address
    IPAddress IP = WiFi.softAPIP();
    Serial.println("AP configuration successful");
    Serial.print("AP IP address: ");
    Serial.println(IP);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(ledPin3, OUTPUT);


  initAP();
  initWebSocket();
}

void loop() {
  ws.cleanupClients();
  digitalWrite(ledPin1, ledState1);
  digitalWrite(ledPin2, ledState2);
  digitalWrite(ledPin3, ledState3);
}