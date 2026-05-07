#define REMOTEXY_MODE__WIFI_CLOUD

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>

// RemoteXY connection settings 
#define REMOTEXY_CLOUD_SERVER "cloud.remotexy.com"
#define REMOTEXY_CLOUD_PORT 6376
#define REMOTEXY_CLOUD_TOKEN "xxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

// EEPROM settings
#define EEPROM_SIZE 1024
#define EEPROM_CONFIG_FLAG 0
#define EEPROM_SSID_START 1
#define EEPROM_PASS_START 33
#define EEPROM_RELAY_NAMES_START 100
#define EEPROM_AP_MODE_START 200

// WiFi Manager variables
WiFiManager wifiManager;

// Web Server
ESP8266WebServer server(80);

// Simpan SSID dan password untuk RemoteXY
char remoteXY_ssid[32] = "";
char remoteXY_password[64] = "";

// Nama relay (default)
char relayNames[4][21] = {"Relay 1", "Relay 2", "Relay 3", "Relay 4"};

// Variabel untuk mode AP
bool apModeEnabled = false;

// Include RemoteXY SETELAH kita set variabel
#define REMOTEXY_WIFI_SSID remoteXY_ssid
#define REMOTEXY_WIFI_PASSWORD remoteXY_password

#include <RemoteXY.h>




// RemoteXY GUI configuration  
#pragma pack(push, 1)  
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =   // 124 bytes V19 
  { 255,4,0,21,0,117,0,19,0,0,0,0,28,1,106,200,1,1,5,0,
  2,18,46,71,17,1,2,25,31,31,68,101,112,97,110,32,32,32,32,32,
  0,79,70,70,0,2,18,80,71,17,1,2,25,31,31,84,101,110,103,97,
  104,32,32,32,0,79,70,70,0,2,18,114,71,17,1,2,25,31,31,66,
  101,108,97,107,97,110,103,0,79,70,70,0,2,18,148,71,17,1,2,25,
  31,31,83,97,109,112,105,110,103,32,0,79,70,70,0,67,9,19,90,11,
  5,2,25,21 };
  
  
  
struct {
  uint8_t switch_01;
  uint8_t switch_02; 
  uint8_t switch_03;
  uint8_t switch_04;
  
    // output variables
  char value_01[21]; // string UTF8 end zero

    // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0

} RemoteXY;   
#pragma pack(pop)

// PIN relay - gunakan pin yang tidak conflict dengan Serial
#define PIN_SWITCH_01 0  // GPIO5
#define PIN_SWITCH_02 1  // GPIO4
#define PIN_SWITCH_03 2  // GPIO14
#define PIN_SWITCH_04 3  // GPIO12

bool relayState[4] = {false, false, false, false};

// Tambahkan variabel global di awal, misalnya setelah struct RemoteXY
bool ipShown = false;

// Fungsi EEPROM
void saveWiFiCredentials(const char* ssid, const char* password) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_CONFIG_FLAG, 1);
  
  for (int i = 0; i < strlen(ssid); i++) {
    EEPROM.write(EEPROM_SSID_START + i, ssid[i]);
  }
  EEPROM.write(EEPROM_SSID_START + strlen(ssid), '\0');
  
  for (int i = 0; i < strlen(password); i++) {
    EEPROM.write(EEPROM_PASS_START + i, password[i]);
  }
  EEPROM.write(EEPROM_PASS_START + strlen(password), '\0');
  
  EEPROM.commit();
  EEPROM.end();
  
  strncpy(remoteXY_ssid, ssid, sizeof(remoteXY_ssid));
  strncpy(remoteXY_password, password, sizeof(remoteXY_password));
  
  Serial.println("WiFi credentials saved");
}

bool loadWiFiCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  
  if (EEPROM.read(EEPROM_CONFIG_FLAG) != 1) {
    EEPROM.end();
    return false;
  }
  
  int i = 0;
  for (i = 0; i < 31; i++) {
    char c = EEPROM.read(EEPROM_SSID_START + i);
    if (c == '\0') break;
    remoteXY_ssid[i] = c;
  }
  remoteXY_ssid[i] = '\0';
  
  for (i = 0; i < 63; i++) {
    char c = EEPROM.read(EEPROM_PASS_START + i);
    if (c == '\0') break;
    remoteXY_password[i] = c;
  }
  remoteXY_password[i] = '\0';
  
  EEPROM.end();
  
  if (strlen(remoteXY_ssid) > 0) {
    Serial.print("Loaded WiFi SSID: ");
    Serial.println(remoteXY_ssid);
    return true;
  }
  
  return false;
}

void saveRelayNames() {
  EEPROM.begin(EEPROM_SIZE);
  int address = EEPROM_RELAY_NAMES_START;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < strlen(relayNames[i]); j++) {
      EEPROM.write(address + j, relayNames[i][j]);
    }
    EEPROM.write(address + strlen(relayNames[i]), '\0');
    address += 21;
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println("Relay names saved");
}

void loadRelayNames() {
  EEPROM.begin(EEPROM_SIZE);
  int address = EEPROM_RELAY_NAMES_START;
  for (int i = 0; i < 4; i++) {
    int j = 0;
    char c;
    while ((c = EEPROM.read(address + j)) != '\0' && j < 20) {
      relayNames[i][j] = c;
      j++;
    }
    relayNames[i][j] = '\0';
    address += 21;
  }
  EEPROM.end();
  Serial.println("Relay names loaded");
}

void saveAPModeStatus() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_AP_MODE_START, apModeEnabled ? 1 : 0);
  EEPROM.commit();
  EEPROM.end();
  Serial.println("AP mode status saved");
}

void loadAPModeStatus() {
  EEPROM.begin(EEPROM_SIZE);
  apModeEnabled = (EEPROM.read(EEPROM_AP_MODE_START) == 1);
  EEPROM.end();
  Serial.print("AP mode status: ");
  Serial.println(apModeEnabled ? "Enabled" : "Disabled");
}

// HTML Page yang lebih sederhana untuk testing
const char* htmlPage = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <title>Relay Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 0; padding: 20px; background: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
        .relay { margin: 15px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; text-align: center; }
        .switch { position: relative; display: inline-block; width: 60px; height: 34px; margin: 0 10px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background: #ccc; transition: .4s; border-radius: 34px; }
        .slider:before { position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background: white; transition: .4s; border-radius: 50%; }
        input:checked + .slider { background: #2196F3; }
        input:checked + .slider:before { transform: translateX(26px); }
        .status { font-weight: bold; margin-left: 10px; }
        .on { color: green; }
        .off { color: red; }
        .info { background: #e3f2fd; padding: 10px; border-radius: 5px; margin: 10px 0; }
    </style>
</head>
<body>
    <div class="container">
        <h1>4 Channel Relay Control</h1>
        <div class="info">
            <strong>Status:</strong> <span id="connectionStatus">Connected</span> | 
            <strong>IP:</strong> <span id="ipAddress">%IP%</span> | 
            <strong>WiFi:</strong> <span id="wifiSSID">%SSID%</span>
        </div>

        <div class="relay">
            <h3 id="relay1Name">Relay 1</h3>
            <label class="switch">
                <input type="checkbox" id="relay1" onchange="toggleRelay(1)">
                <span class="slider"></span>
            </label>
            <span class="status" id="status1">OFF</span>
        </div>

        <div class="relay">
            <h3 id="relay2Name">Relay 2</h3>
            <label class="switch">
                <input type="checkbox" id="relay2" onchange="toggleRelay(2)">
                <span class="slider"></span>
            </label>
            <span class="status" id="status2">OFF</span>
        </div>

        <div class="relay">
            <h3 id="relay3Name">Relay 3</h3>
            <label class="switch">
                <input type="checkbox" id="relay3" onchange="toggleRelay(3)">
                <span class="slider"></span>
            </label>
            <span class="status" id="status3">OFF</span>
        </div>

        <div class="relay">
            <h3 id="relay4Name">Relay 4</h3>
            <label class="switch">
                <input type="checkbox" id="relay4" onchange="toggleRelay(4)">
                <span class="slider"></span>
            </label>
            <span class="status" id="status4">OFF</span>
        </div>

        <div style="text-align: center; margin-top: 20px;">
            <button onclick="loadStatus()">Refresh Status</button>
            <button onclick="location.reload()">Reload Page</button>
        </div>
    </div>

    <script>
        function toggleRelay(relayNum) {
            var checkbox = document.getElementById('relay' + relayNum);
            var state = checkbox.checked ? 1 : 0;
            
            fetch('/control?channel=' + relayNum + '&state=' + state)
                .then(response => response.text())
                .then(data => {
                    updateStatusDisplay(relayNum, checkbox.checked);
                })
                .catch(error => {
                    console.error('Error:', error);
                    checkbox.checked = !checkbox.checked;
                });
        }

        function updateStatusDisplay(relayNum, isOn) {
            var status = document.getElementById('status' + relayNum);
            status.textContent = isOn ? 'ON' : 'OFF';
            status.className = 'status ' + (isOn ? 'on' : 'off');
        }

        function loadStatus() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    for (var i = 1; i <= 4; i++) {
                        var checkbox = document.getElementById('relay' + i);
                        checkbox.checked = data['relay' + i];
                        updateStatusDisplay(i, data['relay' + i]);
                    }
                })
                .catch(error => console.error('Error loading status:', error));
        }

        // Load initial status
        window.onload = function() {
            loadStatus();
            // Update status every 3 seconds
            setInterval(loadStatus, 3000);
        }
    </script>
</body>
</html>
)rawliteral";

// Handler functions
void handleRoot() {
  String html = htmlPage;
  if (apModeEnabled) {
    html.replace("%IP%", WiFi.softAPIP().toString());
    html.replace("%SSID%", "AP Mode");
  } else {
    html.replace("%IP%", WiFi.localIP().toString());
    html.replace("%SSID%", WiFi.SSID());
  }
  server.send(200, "text/html", html);
  Serial.println("Web page served");
}

void handleControl() {
  Serial.println("Control request received");
  
  if (server.hasArg("channel") && server.hasArg("state")) {
    int channel = server.arg("channel").toInt() - 1;
    int state = server.arg("state").toInt();
    
    Serial.print("Channel: ");
    Serial.print(channel);
    Serial.print(", State: ");
    Serial.println(state);
    
    if (channel >= 0 && channel < 4) {
      relayState[channel] = (state == 1);
      
      // Update relay outputs
      digitalWrite(PIN_SWITCH_01, relayState[0] ? LOW : HIGH);
      digitalWrite(PIN_SWITCH_02, relayState[1] ? LOW : HIGH);
      digitalWrite(PIN_SWITCH_03, relayState[2] ? LOW : HIGH);
      digitalWrite(PIN_SWITCH_04, relayState[3] ? LOW : HIGH);
      
      // Update RemoteXY switches
      RemoteXY.switch_01 = relayState[0];
      RemoteXY.switch_02 = relayState[1];
      RemoteXY.switch_03 = relayState[2];
      RemoteXY.switch_04 = relayState[3];
      
      server.send(200, "text/plain", "OK");
      Serial.println("Relay control successful");
      return;
    }
  }
  server.send(400, "text/plain", "ERROR");
  Serial.println("Relay control error");
}

void handleStatus() {
  String json = "{";
  json += "\"relay1\":" + String(relayState[0] ? "true" : "false");
  json += ",\"relay2\":" + String(relayState[1] ? "true" : "false");
  json += ",\"relay3\":" + String(relayState[2] ? "true" : "false");
  json += ",\"relay4\":" + String(relayState[3] ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
  Serial.println("Status sent: " + json);
}

void handleGetNames() {
  String json = "{";
  json += "\"name1\":\"" + String(relayNames[0]) + "\"";
  json += ",\"name2\":\"" + String(relayNames[1]) + "\"";
  json += ",\"name3\":\"" + String(relayNames[2]) + "\"";
  json += ",\"name4\":\"" + String(relayNames[3]) + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleSaveNames() {
  if (server.hasArg("name1") && server.hasArg("name2") && server.hasArg("name3") && server.hasArg("name4")) {
    strncpy(relayNames[0], server.arg("name1").c_str(), 20);
    strncpy(relayNames[1], server.arg("name2").c_str(), 20);
    strncpy(relayNames[2], server.arg("name3").c_str(), 20);
    strncpy(relayNames[3], server.arg("name4").c_str(), 20);
    saveRelayNames();
    server.send(200, "text/plain", "Settings saved!");
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleSystemInfo() {
  String json = "{";
  json += "\"ip\":\"" + (apModeEnabled ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\",";
  json += "\"mac\":\"" + WiFi.macAddress() + "\",";
  json += "\"ssid\":\"" + (apModeEnabled ? "AP Mode" : WiFi.SSID()) + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"remotexyStatus\":\"" + String(RemoteXY.connect_flag ? "Connected" : "Disconnected") + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleRestart() {
  server.send(200, "text/plain", "Restarting...");
  delay(1000);
  ESP.restart();
}

void handleResetWiFi() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_CONFIG_FLAG, 0);
  EEPROM.commit();
  EEPROM.end();
  server.send(200, "text/plain", "WiFi reset. Restarting...");
  delay(1000);
  ESP.restart();
}

void handleAPMode() {
  if (server.hasArg("enable")) {
    apModeEnabled = (server.arg("enable") == "true");
    saveAPModeStatus();
    server.send(200, "text/plain", "AP mode updated. Restarting...");
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Invalid parameters");
  }
}

void handleAPStatus() {
  String json = "{\"enabled\":" + String(apModeEnabled ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
  Serial.println("404: " + server.uri());
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting Relay Controller...");
  
  // Initialize EEPROM and load settings
  EEPROM.begin(EEPROM_SIZE);
  loadRelayNames();
  loadAPModeStatus();
  
  // Initialize relay pins
  pinMode(PIN_SWITCH_01, OUTPUT);
  pinMode(PIN_SWITCH_02, OUTPUT);
  pinMode(PIN_SWITCH_03, OUTPUT);
  pinMode(PIN_SWITCH_04, OUTPUT);
  
  // Turn off all relays initially
  digitalWrite(PIN_SWITCH_01, HIGH);
  digitalWrite(PIN_SWITCH_02, HIGH);
  digitalWrite(PIN_SWITCH_03, HIGH);
  digitalWrite(PIN_SWITCH_04, HIGH);
  
  Serial.println("Relay pins initialized");
  
  // Connect to WiFi or start AP mode
  if (apModeEnabled) {
    Serial.println("Starting in AP mode...");
    WiFi.softAP("RelayController", "");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    if (loadWiFiCredentials()) {
      Serial.println("Connecting to WiFi...");
      WiFi.begin(remoteXY_ssid, remoteXY_password);
      
      int timeout = 0;
      while (WiFi.status() != WL_CONNECTED && timeout < 30) {
        delay(500);
        Serial.print(".");
        timeout++;
        if (timeout >= 30) {
          Serial.println("\nWiFi connection failed, starting AP mode...");
          apModeEnabled = true;
          saveAPModeStatus();
          WiFi.softAP("RelayController", "");
          break;
        }
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
      }
    } else {
      Serial.println("No saved credentials, starting config portal...");
      wifiManager.setConfigPortalTimeout(180);
      
      if (!wifiManager.startConfigPortal("RelayController")) {
        Serial.println("Config portal timeout, restarting...");
        delay(3000);
        ESP.restart();
      } else {
        saveWiFiCredentials(wifiManager.getWiFiSSID().c_str(), wifiManager.getWiFiPass().c_str());
        Serial.println("Credentials saved, restarting...");
        delay(2000);
        ESP.restart();
      }
    }
  }
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.on("/status", handleStatus);
  server.on("/getnames", handleGetNames);
  server.on("/savenames", HTTP_POST, handleSaveNames);
  server.on("/systeminfo", handleSystemInfo);
  server.on("/restart", handleRestart);
  server.on("/resetwifi", handleResetWiFi);
  server.on("/apmode", HTTP_POST, handleAPMode);
  server.on("/apstatus", handleAPStatus);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("HTTP server started");
  
  // Initialize RemoteXY only if not in AP mode
  if (!apModeEnabled) {
    RemoteXY_Init();
    Serial.println("RemoteXY initialized");
  } else {
    Serial.println("RemoteXY disabled (AP mode)");
  }
  
  Serial.println("Setup completed");
}

void loop() {
  server.handleClient();
  
  if (!apModeEnabled) {
    RemoteXY_Handler();
    
    // Update relay states from RemoteXY
    relayState[0] = (RemoteXY.switch_01 == 1);
    relayState[1] = (RemoteXY.switch_02 == 1);
    relayState[2] = (RemoteXY.switch_03 == 1);
    relayState[3] = (RemoteXY.switch_04 == 1);
  }
  
  // Update physical relays
  digitalWrite(PIN_SWITCH_01, relayState[0] ? LOW : HIGH);
  digitalWrite(PIN_SWITCH_02, relayState[1] ? LOW : HIGH);
  digitalWrite(PIN_SWITCH_03, relayState[2] ? LOW : HIGH);
  digitalWrite(PIN_SWITCH_04, relayState[3] ? LOW : HIGH);
  
  delay(50);
    
  // Tampilkan IP hanya sekali setelah WiFi terhubung
  if (!ipShown && WiFi.status() == WL_CONNECTED) {
    ipShown = true;
    char ipBuffer[21];
    sprintf(ipBuffer, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
    strcpy(RemoteXY.value_01, ipBuffer);
  }
}
