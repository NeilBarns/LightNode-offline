#include <EEPROM.h>
#include <FS.h>
#include <WiFiManager.h>

WiFiManager wifiManager;

void setup() {
  Serial.begin(115200);

  // Initialize EEPROM with a size of 512 bytes
  EEPROM.begin(512);

  // Clear EEPROM by writing zeros
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }

  // Commit the changes to EEPROM
  EEPROM.commit();
if (!SPIFFS.begin()) {
    Serial.println(F("Failed to mount file system"));
    return;
  }

  SPIFFS.format();
SPIFFS.end();

  Serial.println("EEPROM cleared.");
  wifiManager.resetSettings();
}

void loop() {
  // Nothing to do here
}
