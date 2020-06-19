#include <Countdown.h>
#include <FP.h>
#include <IPStack.h>
#include <MQTTClient.h>
#include <StackTrace.h>
#include <WifiIPStack.h>

#include <FS.h> // includes for using filesystem
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>

#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 5
#define RST_PIN 22

const char* CONFIG_PASSWORD = "correct-horse-battery-staple";
bool debug = false;

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;

// Wifimanager variables
uint64_t chip_id = ESP.getEfuseMac();
uint16_t chip = (uint16_t)(chip_id >> 32);
bool save_config = false;
char mqtt_host[40];
char mqtt_port[6];
char mqtt_user[40];
char mqtt_pw[64];
char mqtt_topic[128] = "RFID/Shield1/lastitem";
char mqtt_default_value[20] = "none";
char mqtt_clientID[40];
bool is_connected = false;
bool rfid_tag_present_prev = false;
bool rfid_tag_present = false;
int _rfid_error_counter = 0;
bool _tag_found = false;
unsigned long previousMillis = 0;
unsigned long interval = 500;


void save_config_callback() {
  save_config = true;
}

void load_json() {
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
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

        Serial.println("\nparsed json");
        serializeJson(jsondoc, Serial);
        strcpy(mqtt_host, jsondoc["mqtt_host"]);
        strcpy(mqtt_port, jsondoc["mqtt_port"]);
        strcpy(mqtt_user, jsondoc["mqtt_user"]);
        strcpy(mqtt_pw, jsondoc["mqtt_pw"]);
        strcpy(mqtt_topic, jsondoc["mqtt_topic"]);
        strcpy(mqtt_default_value, jsondoc["mqtt_default_value"]);
        strcpy(mqtt_clientID, jsondoc["mqtt_clientID"]);
        configFile.close();
      }
    }
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
  Serial.println("############");
  Serial.println(measureJson(jsondoc));
  Serial.println("############");
  SPIFFS.begin();
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }
  serializeJson(jsondoc, Serial);
  delay(1000);
  serializeJson(jsondoc, configFile);
  configFile.close();
  //end save
}

WiFiClient wifi;
IPStack ipstack(wifi);
MQTT::Client<IPStack, Countdown, 50, 1> client = MQTT::Client<IPStack, Countdown, 50, 1>(ipstack);
MQTT::Message message;

void connect()
{ 
  char buffer[128];
  sprintf(buffer, "Connecting to: %s:%d", mqtt_host, atoi(mqtt_port));
  Serial.println(buffer);
  int rc = ipstack.connect(mqtt_host, atoi(mqtt_port));
  if (rc != 1)
  {
    Serial.print("rc from TCP connect is ");
    Serial.println(rc);
  }
  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
  data.MQTTVersion = 3;
  data.clientID.cstring = mqtt_clientID;
  sprintf(buffer, "Connecting as: ", mqtt_clientID);
  Serial.println(buffer);
  rc = client.connect(data);
  if (rc != 0)
  {
    Serial.print("rc from MQTT connect is ");
    Serial.println(rc);
  }
  Serial.println("MQTT connected");

  // subscribe to new topic
  /*rc = client.subscribe(topic, MQTT::QOS2, messageArrived);   
  if (rc != 0)
  {
    Serial.print("rc from MQTT subscribe is ");
    Serial.println(rc);
  }
  Serial.println("MQTT subscribed");
  */
}

void setup()
{
  Serial.begin(115200);
  if (debug) {
    if (!SPIFFS.begin(true)) {
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }

    File root = SPIFFS.open("/");

    File file = root.openNextFile();

    while (file) {

      Serial.print("FILE: ");
      Serial.println(file.name());

      file = root.openNextFile();
    }
    Serial.println("No more files");
  }
  Serial.println("Starting");

  // Try to read existing config
  // from config.json
  load_json();

  //Wifi Manager initialization
  WiFiManager wifimanager;
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

  wifimanager.setSaveConfigCallback(save_config_callback);
  
  char bssid[17];
  snprintf(bssid, 17, "ESP-%04X%08X", chip, (uint32_t) chip_id);
  if (!wifimanager.autoConnect(bssid, CONFIG_PASSWORD)) {
    Serial.println("Could not connect");
    Serial.println(bssid);
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  // we are connected
  // so copy back values and check if config needs saving
  strcpy(mqtt_host, custom_mqtt_host.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pw, custom_mqtt_pw.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());
  strcpy(mqtt_default_value, custom_mqtt_default_value.getValue());
  strcpy(mqtt_clientID, custom_mqtt_clientID.getValue());


  if (save_config) {
    save_json();
    Serial.println("configuration saved");
  }

  // Start RFID reader
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  message.qos = MQTT::QOS1;
  message.retained = false;
  message.dup = false;
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

void loop()
{
  if (!client.isConnected())
    connect();
  rfid_tag_present_prev = rfid_tag_present;

  _rfid_error_counter += 1;
  if (_rfid_error_counter > 2) {
    _tag_found = false;
  }

  // Detect Tag without looking for collisions
  byte bufferATQA[2];
  byte bufferSize = sizeof(bufferATQA);

  // Reset baud rates
  rfid.PCD_WriteRegister(rfid.TxModeReg, 0x00);
  rfid.PCD_WriteRegister(rfid.RxModeReg, 0x00);
  // Reset ModWidthReg
  rfid.PCD_WriteRegister(rfid.ModWidthReg, 0x26);

  MFRC522::StatusCode result = rfid.PICC_RequestA(bufferATQA, &bufferSize);

  if (result == rfid.STATUS_OK) {
    if ( ! rfid.PICC_ReadCardSerial()) { //Since a PICC placed get Serial and continue
      return;
    }
    _rfid_error_counter = 0;
    _tag_found = true;
  }

  rfid_tag_present = _tag_found;
  unsigned long currentMillis = millis();
  // rising edge
  if (rfid_tag_present && (!rfid_tag_present_prev || (currentMillis - previousMillis) > interval)) {
    Serial.println("Tag present");
    Serial.print(F("Card UID:"));
    char str[32] = "";
    array_to_string(rfid.uid.uidByte, rfid.uid.size, str);
    Serial.println(str);
    message.retained = false;
    message.payload = str;
    message.payloadlen = strlen(str);
    client.publish(mqtt_topic, message);
    previousMillis = currentMillis;
  }

  // falling edge
  if (!rfid_tag_present && rfid_tag_present_prev) {
    Serial.println("Tag gone");
    message.payload = mqtt_default_value;
    message.payloadlen = strlen(mqtt_default_value);
    message.retained = true;
    client.publish(mqtt_topic, message);
    previousMillis = 0;
  }
}
