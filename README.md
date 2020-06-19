# ESP32, MQTT, RFID tutorial
I recently wrote a German tutorial (link follows) that describes on how to use an ESP to publish UIDs of RFID-tags into an MQTT topic. I wanted to have the ESP configurable in new environments, so I could easily ship pre-flashed boards to the users.
The code in the tutorial sequentially builds the functionality and inserts glue code in the following order:
* wifimanager
* wifimanager_mqtt
* wifimanager_mqtt_mfrc522
* wifimanager_mqtt_mfrc522_jsonconfig
* autoconnect_rfid_mqtt_esp (my initial target project)

The final project not only detects RFID tags it also tells when it changes or a certain time has passed (requirement on my project).
