#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>
//
// ...
//
// Wifimanager variables
uint64_t chip_id = ESP.getEfuseMac();
uint16_t chip = (uint16_t)(chip_id >> 32);
const char* CONFIG_PASSWORD = "correct-horse-battery-staple";
char ssid[17];

// MQTT variables
char mqtt_host[] = "10.0.0.59";
char mqtt_port[] = "1883";
char mqtt_topic[] = "devices/esp/status";

// Initialize MQTT client
WiFiClient wifi;
IPStack ipstack(wifi);
const int MMMS = 50; // MAX_MQTT_MESSAGE_SIZE
const int MMPS = 1;  // MAX_MQTT_PARALLEL_SESSIONS
MQTT::Client<IPStack, Countdown, MMMS, MMPS> client = MQTT::Client<IPStack, Countdown, MMMS, MMPS>(ipstack);
MQTT::Message message; 

void connect_mqtt(){
  Serial.print("Connecting to: ");
  Serial.print(mqtt_host);
  Serial.print(":");
  Serial.println(mqtt_port);
  int returncode = ipstack.connect(mqtt_host, atoi(mqtt_port));
  if (returncode != 1)
  {
    Serial.print("Returncode from TCP connect is ");
    Serial.println(returncode);
    return;
  }
  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
  data.MQTTVersion = 3;
  data.clientID.cstring = ssid;
  Serial.print("Connecting as: ");
  Serial.println(ssid);
  returncode = client.connect(data);
  if (returncode != 0)
  {
    Serial.print("Returncode from MQTT connect is ");
    Serial.println(returncode);
    return;
  }
  Serial.println("MQTT connected"); 
}

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

  message.qos = MQTT::QOS1;
  message.retained = false;
}

void loop(){
  if(!client.isConnected()){
      connect_mqtt();
  }
  char payload[] = "alive";
  message.payload = payload;
  message.payloadlen = strlen(payload);
  client.publish(mqtt_topic, message);
  delay(5000);
}
