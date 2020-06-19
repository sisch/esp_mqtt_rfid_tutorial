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

// MQTT variables
char mqtt_host[] = "10.0.0.60";
char mqtt_port[] = "1883";
char mqtt_topic[] = "devices/esp/status";

// Initialize MQTT client
WiFiClient wifi;
IPStack ipstack(wifi);
int MMMS = 50; // MAX_MQTT_MESSAGE_SIZE
int MMPS = 1;  // MAX_MQTT_PARALLEL_SESSIONS
MQTT::Client<IPStack, Countdown, MMMS, MMPS> client = MQTT::Client<IPStack, Countdown, MMMS, MMPS>(ipstack);
MQTT::Message message; 

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
  Serial.println("Connected");
  delay(4000);
}
