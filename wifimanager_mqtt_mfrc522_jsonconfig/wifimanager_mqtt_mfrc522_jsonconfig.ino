#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <IPStack.h>
#include <WifiIPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <FS.h> // includes for using filesystem
#include <SPIFFS.h>
#include <ArduinoJson.h>

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

// create local variables for configuration
char mqtt_host[40];
char mqtt_port[6];
char mqtt_user[40];
char mqtt_pw[64];
char mqtt_topic[128] = "RFID/Shield1/lastitem";
char mqtt_default_value[40] = "none";
char mqtt_clientID[40];

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

void load_json(){
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        DynamicJsonDocument jsondoc(1024);
        DeserializationError error = deserializeJson(jsondoc, configFile);
        if (error) {
          Serial.println("failed to load json config");
          Serial.println(error.c_str());
          return;
        }

        //Serial.println("\nparsed json");
        //serializeJson(jsondoc, Serial);  //Testoutput to Serial Monitor
        // copy json values into local variables
        strncpy(mqtt_host, jsondoc["mqtt_host"], 39);
        strncpy(mqtt_port, jsondoc["mqtt_port"], 5);
        strncpy(mqtt_user, jsondoc["mqtt_user"], 40);
        strncpy(mqtt_pw, jsondoc["mqtt_pw"], 63);
        strncpy(mqtt_topic, jsondoc["mqtt_topic"], 127);
        strncpy(mqtt_default_value, jsondoc["mqtt_default_value"], 39);
        strncpy(mqtt_clientID, jsondoc["mqtt_clientID"], 39);
        configFile.close();
      }
    } // else { // would be nicer to have file not exists error here}
  } else {
    Serial.println("failed to mount FS");
    delay(1000);
    return;
  }
}

void save_json() {
  Serial.println("saving config");
  DynamicJsonDocument jsondoc(1024);
  jsondoc["mqtt_host"] = mqtt_host;
  jsondoc["mqtt_port"] = mqtt_port;
  jsondoc["mqtt_user"] = mqtt_user;
  jsondoc["mqtt_pw"] = mqtt_pw;
  jsondoc["mqtt_topic"] = mqtt_topic;
  jsondoc["mqtt_default_value"] = mqtt_default_value;
  jsondoc["mqtt_clientID"] = mqtt_clientID;
  SPIFFS.begin(true); // true meaning formatOnFail
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }
  serializeJson(jsondoc, configFile);
  configFile.close();
  //end save
}

bool needs_saving = false;
void save_callback(){
  needs_saving = true;
}



void setup(){
  SPIFFS.begin();
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while(file){
    Serial.print("file: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }
  
  Serial.begin(115200);
  
  // Set ssid to ESP-123456789ABC
  snprintf(ssid, 17, "ESP-%04X%08X", chip, (uint32_t) chip_id);

  WiFiManager wifimanager;
  load_json();
  
  WiFiManagerParameter custom_mqtt_host("server", "mqtt server", mqtt_host, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 40);
  WiFiManagerParameter custom_mqtt_pw("pw", "mqtt password", mqtt_pw, 64);
  WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", mqtt_topic, 128);
  WiFiManagerParameter custom_mqtt_default_value("defaultPayload", "mqtt default_payload", mqtt_default_value, 20);
  WiFiManagerParameter custom_mqtt_clientID("clientID", "Client ID", mqtt_clientID, 40);
  wifimanager.addParameter(&custom_mqtt_host);
  wifimanager.addParameter(&custom_mqtt_port);
  wifimanager.addParameter(&custom_mqtt_user);
  wifimanager.addParameter(&custom_mqtt_pw);
  wifimanager.addParameter(&custom_mqtt_topic);
  wifimanager.addParameter(&custom_mqtt_default_value);
  wifimanager.addParameter(&custom_mqtt_clientID);

  wifimanager.setSaveConfigCallback(save_callback);
  if (!wifimanager.autoConnect(ssid, CONFIG_PASSWORD)) {
    Serial.println("Could not start AP");
    Serial.println(ssid);
    delay(3000);
    ESP.restart();
    delay(5000);
  }
  // we are connected
  // so copy back values and check if config needs saving
  strncpy(mqtt_host, custom_mqtt_host.getValue(), 39);
  strncpy(mqtt_port, custom_mqtt_port.getValue(), 5);
  strncpy(mqtt_user, custom_mqtt_user.getValue(), 39);
  strncpy(mqtt_pw, custom_mqtt_pw.getValue(), 63);
  strncpy(mqtt_topic, custom_mqtt_topic.getValue(), 127);
  strncpy(mqtt_default_value, custom_mqtt_default_value.getValue(), 39);
  strncpy(mqtt_clientID, custom_mqtt_clientID.getValue(), 39);
  
  if (needs_saving) {
    save_json();
    Serial.println("configuration saved");
  }
  needs_saving = false;
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
    {
      delay(500);
      message.payload = mqtt_default_value;
      message.payloadlen = strlen(mqtt_default_value);
      client.publish(mqtt_topic, message);
      return;
      }
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
