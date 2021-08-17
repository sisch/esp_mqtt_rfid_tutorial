# ESP32, MQTT, RFID tutorial
I recently wrote a German tutorial (https://plantprogrammer.de/esp32-rfid-and-mqtt/) that describes on how to use an ESP to publish UIDs of RFID-tags into an MQTT topic. I wanted to have the ESP configurable in new environments, so I could easily ship pre-flashed boards to the users.
The code in the tutorial sequentially builds the functionality and inserts glue code in the following order:
* wifimanager
* wifimanager_mqtt
* wifimanager_mqtt_mfrc522
* wifimanager_mqtt_mfrc522_jsonconfig
* autoconnect_rfid_mqtt_esp (my initial target project)

The final project not only detects RFID tags it also tells when it changes or a certain time has passed (requirement on my project).

# ESP32-Template sketch
## Purpose
This is a generic - batteries included - template that takes care of 
* Connection handling using autoconnect
* Setting a basic set of MQTT parameters in the captive portal
* Persisting configuration to a file
* Establishing MQTT connection (reconnecting if necessary)
* Examples for publish and subscribe 

Use it whenever you want to start a new ESP32 MQTT project, but want to leave out the boring parts.

## Requirements
You will need ESP32 board definitions in your preferences (this url needs to be present: https://dl.espressif.com/dl/package_esp32_index.json)
Download and install the following libraries in your Arduino IDE using the menu "Sketch->Include->Add .ZIP library"
* paho MQTT "Arduino client library" https://projects.eclipse.org/projects/iot.paho/downloads
* WifiManager https://github.com/TheBrunez/WIFIMANAGER-ESP32 and its dependencies
    * https://github.com/zhouhan0126/WebServer-esp32
    * https://github.com/zhouhan0126/DNSServer---esp32
* https://github.com/bblanchon/ArduinoJson

## Getting Started
After installing all requirements, just download a copy of the ESP32-template folder, open the .ino file in Arduino IDE and check out lines 16, 152, and 264.

1. In line 16 you can decide whether to use encrypted communication through TLS (default) or not (comment this line then). If you want to use TLS, you need to add your certificates in the separate certs_available.h header file.

2. In line 152 you can add subscribers to certain topics (defaults to the topic set in captive portal)

3. In line 264 you can publish messages (needs to be of type char*, if in doubt prepare a `char buffer[length]` and fill it with `snprintf(buffer, length, format, args...)`)

4. Upload the sketch to your ESP32

5. Connect to ESP32-AABBCCDDEE access point and configure your WiFi credentials and MQTT parameters.

6. The captive portal launches everytime the ESP does not find the last configured WiFi. Should you need to reconfigure while in the same WiFi, open Arduino IDE's Serial Monitor and send the letter "c". On the next restart your ESP starts as access point again.

