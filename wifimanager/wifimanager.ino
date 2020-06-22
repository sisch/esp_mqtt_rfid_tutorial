#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
//
// ...
//
// Wifimanager variables
uint64_t chip_id = ESP.getEfuseMac();
uint16_t chip = (uint16_t)(chip_id >> 32);
const char* CONFIG_PASSWORD = "correct-horse-battery-staple";
char ssid[17];

void setup(){
  Serial.begin(115200);
  
  // Set ssid to ESP-123456789ABC
  snprintf(ssid, 17, "ESP-%04X%08X", chip, (uint32_t) chip_id);

  WiFiManager wifimanager;
  
  if (!wifimanager.autoConnect(ssid, CONFIG_PASSWORD)) {
    Serial.println("Could not start AP");
    Serial.println(ssid);
    delay(3000);
    ESP.restart();
    delay(5000);
  }
}

void loop(){
  
}
