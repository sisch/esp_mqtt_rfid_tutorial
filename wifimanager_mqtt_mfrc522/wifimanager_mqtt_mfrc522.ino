#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>
#include <SPI.h>
#include <MFRC522.h>


// Set Secondary Select and Reset pin to the pins connected
#define SS_PIN 5
#define RST_PIN 22

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;

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

void array_to_string(byte array[], unsigned int len, char buffer[])
{
  for (unsigned int i = 0; i < len; i++)
  {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[i * 2 + 0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[i * 2 + 1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
  }
  buffer[len * 2] = '\0';
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

  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
}

void loop(){
  if(!client.isConnected()){
      connect_mqtt();
  }
  
  if ( ! rfid.PICC_IsNewCardPresent())
    {delay(500); return;}
  if ( ! rfid.PICC_ReadCardSerial())
    {delay(1000); return;}
  
  // prepare string buffer 2 chars per byte + '\0'
  char str[rfid.uid.size * 2 + 1]; 
  array_to_string(rfid.uid.uidByte, rfid.uid.size, str);
  Serial.println(str);
  message.retained = false;
  message.payload = str;
  message.payloadlen = strlen(str);
  client.publish(mqtt_topic, message);
  rfid.PICC_HaltA();  // stop card from being read while near reader
  delay(2000);
}
