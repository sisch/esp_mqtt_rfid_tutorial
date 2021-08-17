#include <Countdown.h>
#include <FP.h>
#include <IPStack.h>
#include <MQTTClient.h>
#include <StackTrace.h>
#include <WifiIPStack.h>

#include <FS.h> // includes for using filesystem
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>

// Un/comment next line if you want to use certificates for MQTTS
#include "certs_available.h"
#ifdef CERTS_AVAILABLE
  #include <WiFiClientSecure.h>
#else
  #include <WiFi.h>
#endif

const char* CONFIG_PASSWORD = "correct-horse-battery-staple";
bool debug = false;

// Wifimanager variables
uint64_t chip_id = ESP.getEfuseMac();
uint16_t chip = (uint16_t)(chip_id >> 32);
bool save_config = false;
char mqtt_host[40];
char mqtt_port[6];
char mqtt_user[40];
char mqtt_pw[64];
char mqtt_topic[128] = "sensors/test";
char mqtt_default_value[20] = "none";
char mqtt_clientID[40];
bool is_connected = false;

#ifdef CERTS_AVAILABLE
WiFiClientSecure wifi;
#else
WiFiClient wifi;
#endif
IPStack ipstack(wifi);
MQTT::Client<IPStack, Countdown, 250, 1> client = MQTT::Client<IPStack, Countdown, 250, 1>(ipstack);
MQTT::Message message;
WiFiManager wifimanager;

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
  SPIFFS.begin(true);
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


void connect()
{ 
  #ifdef CERTS_AVAILABLE
  wifi.setCACert(CA_CERT);
  wifi.setCertificate(CLIENT_CERT);
  wifi.setPrivateKey(CLIENT_KEY);
  #endif
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
  data.username.cstring = mqtt_user;
  data.password.cstring = mqtt_pw;
  
  sprintf(buffer, "Connecting as: %s", mqtt_clientID);
  Serial.println(buffer);
  rc = client.connect(data);
  if (rc != 0)
  {
    Serial.print("rc from MQTT connect is ");
    Serial.println(rc);
    return;
    
  }
  Serial.println("MQTT connected");

  // insert MQTT subscription code here
  /*
  rc = client.subscribe(mqtt_topic, MQTT::QOS1, messageArrived);   
  if (rc != 0)
  {
    Serial.print("rc from MQTT subscribe is ");
    Serial.println(rc);
  }
  Serial.print("MQTT subscribed to ");
  Serial.println(mqtt_topic); 
  // */
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

void setup()
{
  Serial.begin(115200);

  SPIFFS.begin();
  bool force_config = SPIFFS.exists("/force_config") || !SPIFFS.exists("/config.json");
  // Try to read existing config
  // from config.json
  load_json();

  //Wifi Manager initialization
  
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

  char bssid[17];
  snprintf(bssid, 17, "ESP-%04X%08X", chip, (uint32_t) chip_id);
  wifimanager.setSaveConfigCallback(save_config_callback);
  if(force_config){
    if(wifimanager.startConfigPortal(bssid, CONFIG_PASSWORD)){

      SPIFFS.remove("/force_config");
      delay(2000);
    }
  }
  else if (!wifimanager.autoConnect(bssid, CONFIG_PASSWORD)) {
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
}


void loop()
{
  if(char c = Serial.read()){
    if(c == 'c'){
      SPIFFS.begin();
      File force_cfg = SPIFFS.open("/force_config", "w");
      force_cfg.print("y");
      force_cfg.close();
      delay(1000);
      ESP.restart();
    }
    else if(c == 'x'){
      SPIFFS.begin();
      SPIFFS.remove("/config.json");
      delay(1000);
      ESP.restart();
    }
  }
  if (!client.isConnected())
    {
      connect();
    }

  // Insert your MQTT publishing code here
  /*
    char* str = "Hello World!";
    message.retained = false;
    message.payload = str;
    message.payloadlen = strlen(str);
    client.publish(mqtt_topic, message);
    delay(2000); 
  //*/
  client.yield();
}
