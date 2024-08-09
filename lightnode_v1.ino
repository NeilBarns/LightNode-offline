#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <ArduinoOTA.h>

// Configuration parameters
char ssid[32] = "";
char password[32] = ""; 
char serverAppDomain[32] = ""; 
char serverAppPort[6] = "";
const char* registerDeviceURL = "/api/device/insert";
const char* stopDeviceTimeURL = "/api/device-time/end";
char deviceName[32] = "";
int deviceId = 0;

// Network configuration
IPAddress local_IP; 
IPAddress gateway;  
IPAddress subnet;   

// Store network configuration as strings
char ipString[16] = "";
char gatewayString[16] = "";
char subnetString[16] = "";

String manufacturerDeviceName = "Lightnode";
String versionNumber = "1.0.0-0";
String APSSID = manufacturerDeviceName + "-" + versionNumber + "-AP";
String APPass = "L1ghtN0d3@2024";
const char* mDNSHostname = "110lightnode"; 
const char* otaPassword = "L1ghtN0d3@2024";
char hostURL[128];

// SPIFFS
const char* timeFilePath = "/time.txt";

ESP8266WebServer server(80);

bool isRegistered = false; 
bool isLEDOn = false; // State of the LED
bool isTesting = false; // State of test light
bool isDisabled = false; // State of disabled device
bool isPaused = false; //State of paused time
bool isFree = false; //State of free light
int storedTimeInSeconds = 0;

// Time tracking
unsigned long lastMillis = 0; // Last recorded time
unsigned long offDuration = 0; // Time duration the device was off

// Watchdog timer setup
Ticker ticker;

void setup() {
  Serial.begin(115200);

  IPAddress local_IP(192, 168, 10, 3); // Desired IP address
  IPAddress gateway(192, 168, 10, 1);  // Gateway
  IPAddress subnet(255, 255, 255, 0);  // Subnet mask
  
  EEPROM.begin(512);
  if (!SPIFFS.begin()) {
    Serial.println(F("Failed to mount file system"));
    return;
  }
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D1, OUTPUT);
  digitalWrite(D1, LOW);
  loadConfig();
  connectToWiFi();
  startMDNS();
  setupServer();

  // Configure OTA
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = F("sketch");
    } else { // U_SPIFFS
      type = F("filesystem");
    }
    Serial.println(F("Start updating ") + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nEnd"));
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println(F("Auth Failed"));
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println(F("Begin Failed"));
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println(F("Connect Failed"));
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println(F("Receive Failed"));
    } else if (error == OTA_END_ERROR) {
      Serial.println(F("End Failed"));
    }
  });

  ArduinoOTA.begin();

  if (!isRegistered) {
    registerDevice();
  }

  loadState();

  storedTimeInSeconds = readTimeFromSPIFFS();
  unsigned long offDuration = calculateOffDuration();
  storedTimeInSeconds -= offDuration;
  if (storedTimeInSeconds < 0) {
    storedTimeInSeconds = 0;
  }
  writeTimeToSPIFFS(storedTimeInSeconds);
  Serial.println(F("Adjusted storedTimeInSeconds: ") + String(storedTimeInSeconds));

  // Set up a Ticker to call a function that resets the watchdog timer every 30 minutes
  ticker.attach(1800, []() {
    saveState();
    Serial.println(F("Restarting"));
    ESP.restart();
  });

  lastMillis = millis();
}

void loop() {
  server.handleClient();
  manageLEDTiming();
  ArduinoOTA.handle(); // Handle OTA updates
//  checkWiFiConnection(); // Ensure WiFi is connected
}


void connectToWiFi() {
  local_IP.fromString(ipString);
  gateway.fromString(gatewayString);
  subnet.fromString(subnetString);

  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  Serial.print(F("Connecting to WiFi "));
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.print(F("."));
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    attempt++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("Connected to WiFi"));
    Serial.print(F("IP Address: "));
    Serial.println(WiFi.localIP());
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    Serial.println(F("Failed to connect to WiFi. Starting AP mode."));
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(APSSID, APPass);
    Serial.print(F("AP IP address: "));
    Serial.println(WiFi.softAPIP());
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void startMDNS() {
  if (MDNS.begin(mDNSHostname)) {
    Serial.println("MDNS responder started");
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("Error setting up MDNS responder!");
  }
}

void loadConfig() {
  EEPROM.get(0, ssid);
  EEPROM.get(32, password);
  EEPROM.get(64, serverAppDomain);
  EEPROM.get(96, serverAppPort);
  EEPROM.get(128, deviceName); 
  EEPROM.get(160, isRegistered); 
  EEPROM.get(161, ipString);
  EEPROM.get(177, gatewayString);
  EEPROM.get(193, subnetString);
  EEPROM.get(209, isDisabled);
  EEPROM.get(210, isPaused);
  EEPROM.get(211, isFree);
  EEPROM.get(224, deviceId); // Load deviceId from address 224

  snprintf(hostURL, sizeof(hostURL), "http://%s:%s", serverAppDomain, serverAppPort);
}

void saveConfig() {
  EEPROM.put(0, ssid);
  EEPROM.put(32, password);
  EEPROM.put(64, serverAppDomain);
  EEPROM.put(96, serverAppPort);
  EEPROM.put(128, deviceName); 
  EEPROM.put(160, isRegistered); 
  EEPROM.put(161, ipString);
  EEPROM.put(177, gatewayString);
  EEPROM.put(193, subnetString);
  EEPROM.put(209, isDisabled);
  EEPROM.put(210, isPaused);
  EEPROM.put(211, isFree);
  EEPROM.put(224, deviceId); // Save deviceId at address 224
  EEPROM.commit();
}

void setupServer() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", generateHTML());
  });

  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("ssid") && server.hasArg("password") && server.hasArg("hostname") && server.hasArg("device_name")
        && server.hasArg("ip") && server.hasArg("gateway") && server.hasArg("subnet")) {
      strncpy(ssid, server.arg("ssid").c_str(), sizeof(ssid));
      strncpy(password, server.arg("password").c_str(), sizeof(password));
      strncpy(serverAppDomain, server.arg("hostname").c_str(), sizeof(serverAppDomain));
      strncpy(serverAppPort, server.arg("port").c_str(), sizeof(serverAppPort));
      strncpy(deviceName, server.arg("device_name").c_str(), sizeof(deviceName));
      strncpy(ipString, server.arg("ip").c_str(), sizeof(ipString));
      strncpy(gatewayString, server.arg("gateway").c_str(), sizeof(gatewayString));
      strncpy(subnetString, server.arg("subnet").c_str(), sizeof(subnetString));
      
      isRegistered = false; // Reset registration status on config save
      saveConfig();
      server.send(200, "text/html", "<html><body><h1>Configuration Saved!</h1><p>Device will reboot now.</p></body></html>");
      delay(2000);
      ESP.restart();
    } else {
      server.send(400, "text/html", "<html><body><h1>Invalid Input!</h1></body></html>");
    }
  });

  server.on("/api/reset", HTTP_DELETE, []() {
    digitalWrite(D1, LOW);
    resetEEPROMSPIFFS();
    server.send(200, "text/html", "<html><body><h1>Device Reset</h1><p>Device has been reset successfully.</p></body></html>");
    delay(2000);
    ESP.restart();
  });

  server.on("/api/span", HTTP_GET, []() {
    if (server.hasArg("time")) {
      int timeInSeconds = server.arg("time").toInt();
      int temp_storedTimeInSeconds = readTimeFromSPIFFS();
      Serial.println("timeInSeconds:" + String(timeInSeconds));
      Serial.println("storedTimeInSeconds:" + String(temp_storedTimeInSeconds));
      temp_storedTimeInSeconds += timeInSeconds;
      Serial.println("extended-storedTimeInSeconds:" + String(temp_storedTimeInSeconds));
      writeTimeToSPIFFS(temp_storedTimeInSeconds);

      storedTimeInSeconds = temp_storedTimeInSeconds;

      server.send(200, "text/plain", "Time set to " + String(temp_storedTimeInSeconds) + " seconds");
    } else {
      server.send(400, "text/plain", "Missing time parameter");
    }
  });

  server.on("/api/stop", HTTP_GET, []() {
    Serial.println("Forced stop");
    writeTimeToSPIFFS(0);
    storedTimeInSeconds = 0;
    isPaused = false;
    EEPROM.put(177, isPaused);
    EEPROM.commit();
    digitalWrite(D1, LOW);
    isLEDOn = false;
    EEPROM.put(300, isLEDOn);
    EEPROM.commit();
    server.send(200, "text/plain", "Time has been reset to 0 seconds");
  });

  server.on("/api/test", HTTP_GET, []() {
    Serial.println("Testing");
    isTesting = true;
    storedTimeInSeconds = 10;
    server.send(200, "text/plain", "Device test initiated. Time set to 10000 seconds");
  });

  server.on("/api/disable", HTTP_GET, []() {
    isDisabled = true;
    EEPROM.put(209, isDisabled);
    EEPROM.commit();
    server.send(200, "text/plain", "Device disabled");
  });

  server.on("/api/enable", HTTP_GET, []() {
    isDisabled = false;
    EEPROM.put(209, isDisabled);
    EEPROM.commit();
    server.send(200, "text/plain", "Device enabled");
  });

  server.on("/api/pause", HTTP_GET, []() {
    isPaused = true;
    EEPROM.put(210, isPaused);
    EEPROM.commit();
    digitalWrite(D1, LOW);
    isLEDOn = false;
    EEPROM.put(300, isLEDOn);
    EEPROM.commit();
    server.send(200, "text/plain", "Device paused time");
  });

  server.on("/api/resume", HTTP_GET, []() {
    isPaused = false;
    EEPROM.put(210, isPaused);
    EEPROM.commit();
    digitalWrite(D1, HIGH);
    isLEDOn = true;
    EEPROM.put(300, isLEDOn);
    EEPROM.commit();
    server.send(200, "text/plain", "Device resumed time");
  });

  server.on("/api/startfree", HTTP_GET, []() {
    Serial.println("Free light");
    isFree = true;
    EEPROM.put(211, isFree);
    EEPROM.commit();
    digitalWrite(D1, HIGH);
    server.send(200, "text/plain", "Device free light");
  });

  server.on("/api/stopfree", HTTP_GET, []() {
    Serial.println("Stop Free light");
    isFree = false;
    EEPROM.put(211, isFree);
    EEPROM.commit();
    digitalWrite(D1, LOW);
    server.send(200, "text/plain", "Device free light");
  });

  server.begin();
  Serial.println("HTTP server started");
}

void manageLEDTiming() {

  if (isFree)
  {
    digitalWrite(D1, HIGH);
  }
  else
  {
   if (!isDisabled && !isPaused)
    {
       if (storedTimeInSeconds > 0) {
        if (isTesting)
        {
          digitalWrite(D1, HIGH);
          delay(1000);
          storedTimeInSeconds--;
          digitalWrite(D1, LOW);
          delay(1000);
          storedTimeInSeconds--;
        }
        else
        {
          if (!isLEDOn) {
            digitalWrite(D1, HIGH);
            isLEDOn = true;
            saveState(); // Save state whenever the LED state changes
          }
          
          delay(1000);
          storedTimeInSeconds--;
          Serial.println("Remaining time: " + String(storedTimeInSeconds));
          writeTimeToSPIFFS(storedTimeInSeconds);
        }
      } else {
        if (isLEDOn || isTesting) 
        {
          digitalWrite(D1, LOW);
          isTesting = false;
          isLEDOn = false;
          saveState(); // Save state whenever the LED state changes
          notifyServerOfTimeEnd();
        }
      } 
    } 
  }
}


int readTimeFromSPIFFS() {
  File file = SPIFFS.open(timeFilePath, "r");
  if (!file) {
    Serial.println("Failed to open time file for reading");
    return 0;
  }

  String timeString = file.readString();
  file.close();
  Serial.println("Read time from SPIFFS: " + timeString);
  return timeString.toInt();
}

void writeTimeToSPIFFS(int time) {
  File file = SPIFFS.open(timeFilePath, "w");
  if (!file) {
    Serial.println("Failed to open time file for writing");
    return;
  }

  file.println(time);
  file.close();
  Serial.println("Written time to SPIFFS: " + String(time));
}

void resetEEPROMSPIFFS() {
  SPIFFS.format();
  for (int i = 0; i < 512; ++i) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();

  createSPIFFSFile();
  
  Serial.println("EEPROM reset complete");
}

void createSPIFFSFile() {
  File file = SPIFFS.open(timeFilePath, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  int initialTime = 0; // Set your initial time value here
  file.println(initialTime);
  file.close();

  Serial.println(F("Time file created with initial value: ") + String(initialTime));
}

String generateHTML() {
  String page = "<!DOCTYPE html><html lang=\"en\"><head>";
    page += "<meta charset=\"UTF-8\">";
    page += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    page += "<title>Device Configuration</title>";
    page += "<style>";
    page += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f4f4f9; text-align: center; }";
    page += "h1 { color: #333; }";
    page += "form { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); text-align: left; }";
    page += "input[type='text'] { box-sizing: border-box; width: 100%; padding: 8px; margin: 10px 0; border: 1px solid #ccc; border-radius: 4px; }";
    page += "input[type='submit'] { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; }";
    page += "input[type='submit']:hover { background-color: #45a049; }";
    page += ".version { font-size: 0.9em; color: #777; margin-bottom: 20px; }";
    page += ".divider { border: 1px solid lightgray; margin: 15px 0px; }";
    page += "</style>";
    page += "</head><body>";
    page += "<h1>Device Configuration</h1>";
    page += "<div class='version'>" + manufacturerDeviceName +  "  <span id='versionNumber'>" + versionNumber + "</span></div>";
    page += "<form action='/save' method='POST'>";
    page += "<b>SSID:</b> <input type='text' name='ssid' placeholder='WiFi SSID' value='" + String(ssid) + "'><br>";
    page += "<b>Password:</b> <input type='text' name='password' placeholder='WiFi password' value='" + String(password) + "'><br>";
    page += "<b>Server IP:</b> <input type='text' name='hostname' placeholder='Server IP' value='" + String(serverAppDomain) + "'><br>";
    page += "<b>Server Port:</b> <input type='text' name='port' placeholder='Server port' value='" + String(serverAppPort) + "'><br>";
    page += "<b>Device name:</b> <input type='text' name='device_name' placeholder='Device name' value='" + String(deviceName) + "'><br>";
    page += "<div class='divider'></div>";
    page += "<b>Static IP:</b> <input type='text' name='ip' placeholder='192.168.18.184' value='" + String(ipString) + "'><br>";
    page += "<b>Gateway:</b> <input type='text' name='gateway' placeholder='192.168.18.1' value='" + String(gatewayString) + "'><br>";
    page += "<b>Subnet:</b> <input type='text' name='subnet' placeholder='255.255.255.0' value='" + String(subnetString) + "'><br>";
    page += "<input type='submit' value='Save'>";
    page += "</form>";
    page += "</body></html>";
    return page;
}

void registerDevice() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    Serial.println(String(hostURL) + registerDeviceURL);
    http.begin(client, String(hostURL) + registerDeviceURL); 
    http.addHeader(F("Content-Type"), F("application/json"));

    String payload = F("{");
    payload += F("\"DeviceName\":\"") + String(deviceName) + F("\",");
    payload += F("\"IPAddress\":\"") + WiFi.localIP().toString() + F("\",");
    payload += F("\"DeviceStatusID\":1"); // Set initial status as 1/Pending Configuration
    payload += F("}");

    Serial.println(payload);
    
    int httpResponseCode = http.POST(payload);
    Serial.println(httpResponseCode);
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(F("Device registered successfully: ") + response);

      // Parse the JSON response to extract device_id
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, response);

      if (!error) {
        int tmp_deviceId = doc["device_id"];
        Serial.println("Response Device ID: " + String(tmp_deviceId));

        // Save the device ID to EEPROM
        EEPROM.put(224, tmp_deviceId); // Save deviceId at address 224
        EEPROM.commit();

        EEPROM.get(224, deviceId);
        Serial.println("Stored Device ID: " + String(deviceId));
      } else {
        Serial.println(F("Failed to parse JSON response"));
      }

      isRegistered = true; 
      saveConfig(); 
    } else {
      Serial.print(F("Error on sending POST: "));
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println(F("Error in WiFi connection"));
  }
}

void notifyServerOfTimeEnd() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClient client;
        HTTPClient http;

        int id;
        EEPROM.get(224, id); // Load deviceId from address 224
        Serial.println(F("deviceId:") + String(id));
        
        String fullURL = String(hostURL) + stopDeviceTimeURL;
        Serial.println("Full URL: " + fullURL);
        
        http.begin(client, fullURL); 
        http.addHeader(F("Content-Type"), F("application/json"));

        String payload = F("{\"device_id\": \"") + String(id) + F("\"}");
        
        int httpResponseCode = http.POST(payload);
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Server notified successfully: " + response);
        } else {
            Serial.println("Error notifying server: " + httpResponseCode);
        }

        http.end();
    } else {
        Serial.println("Error in WiFi connection");
    }
}

void saveState() {
  EEPROM.put(300, isLEDOn); // Save LED state
  EEPROM.put(304, millis()); // Save current time in millis
  EEPROM.put(308, storedTimeInSeconds); // Save remaining time
  EEPROM.commit();
  Serial.println("Saved state: millis = " + String(millis()) + ", storedTimeInSeconds = " + String(storedTimeInSeconds));
}

void loadState() {
  EEPROM.get(300, isLEDOn);
  EEPROM.get(304, lastMillis); // Get the last recorded time in millis
  EEPROM.get(308, storedTimeInSeconds); // Load remaining time
  Serial.println("Loaded state: millis = " + String(lastMillis) + ", storedTimeInSeconds = " + String(storedTimeInSeconds));

  if (isLEDOn) {
    digitalWrite(D1, HIGH); // Restore LED state
  } else {
    digitalWrite(D1, LOW);
  }
}

unsigned long calculateOffDuration() {
  unsigned long currentMillis = millis();
  if (lastMillis > currentMillis) {
    // Handle the case where millis() overflowed
    lastMillis = currentMillis; 
    return 0; 
  }
  unsigned long offDurationMillis = currentMillis - lastMillis; // Calculate duration in milliseconds
  Serial.println(F("Calculated offDurationMillis: ") + String(offDurationMillis));
  return offDurationMillis / 1000; // Return the duration in seconds
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi connection lost. Reconnecting..."));
    connectToWiFi();
  }
}
