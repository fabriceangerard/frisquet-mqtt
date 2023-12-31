#include <Arduino.h>
#include <RadioLib.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <heltec.h>
#include <config.h>

SX1262 radio = new Module(SS, DIO0, RST_LoRa, BUSY_LoRa); 

int b;  // Boiler counter

// FA: Variables permettant d'envoyer une requête

byte toID = 0x80; // 01 - 80 (boiler)
byte fromID = 0x08;   // 02 - 08 (satellite) ou 7E (connect)
byte reqID = 0xCA;  // 03 - CA !! A modifier pour chaque chaudière / périphérique - à récupérer dans une trame 23 ou 49 
byte msgNum = 0x96; // 04 - 96 !! Potentiellement à modifier à chaque transmission (incrémetation à mettre en place)
byte DemRep = 0x01; // 05 - 01 Demande ou 81 Réponse
byte reqMsg[] = {0x79, 0xE0, 0x00, 0x1C}; // 07-10 - Chaîne de demande de températures chandière [0x79, 0xe0, 0x00, 0x1c]

// -- Première initialisation de la chaîne de requête a la chaudière
uint8_t BoilerTx[] = {toID, fromID, reqID, msgNum, DemRep, 0x03, reqMsg[0], reqMsg[1], reqMsg[2], reqMsg[3]};

WiFiClient espClient;
PubSubClient client(espClient);

void connectToMqtt() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32 Frisquet", mqttUsername, mqttPassword)) {
      Serial.println("Connected to MQTT");
    } else {
      Serial.print("Failed to connect to MQTT, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
      }
  }
}

void publishDeviceEntities() {

// Initialisation de la connexion MQTT
client.setServer(mqttServer, mqttPort);
client.setBufferSize(2048);

// Configuration du capteur de température ambiante 1
char tempAmbiante1ConfigTopic[] = "homeassistant/sensor/frisquet/tempAmbiante1/config";
char tempAmbiante1ConfigPayload[] = R"(
{
  "uniq_id": "frisquet_tempAmbiante1",
  "name": "Frisquet - Temperature ambiante 1",
  "state_topic": "homeassistant/sensor/frisquet/tempAmbiante1/state",
  "unit_of_measurement": "°C",
  "device":{"ids":["FrisquetBoiler"],"mf":"Frisquet","name":"Frisquet Boiler","mdl":"Frisquet Boiler"}
}
)";
client.publish(tempAmbiante1ConfigTopic, tempAmbiante1ConfigPayload);

  // Configuration du capteur de température de consigne
char tempconsigneConfigTopic[] = "homeassistant/sensor/frisquet/consigne/config";
char tempconsigneConfigPayload[] = R"(
{
  "uniq_id": "frisquet_tempConsigne",
  "name": "Frisquet - Temperature consigne",
  "state_topic": "homeassistant/sensor/frisquet/consigne/state",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "device":{"ids":["FrisquetBoiler"],"mf":"Frisquet","name":"Frisquet Boiler","mdl":"Frisquet Boiler"}
}
)";
client.publish(tempconsigneConfigTopic, tempconsigneConfigPayload);

// Configuration récupération Payload
char payloadConfigTopic[] = "homeassistant/sensor/frisquet/payload/config";
char payloadConfigPayload[] = R"(
{
  "name": "Frisquet - Payload",
  "state_topic": "homeassistant/sensor/frisquet/payload/state",
  "device":{"ids":["FrisquetBoiler"],"mf":"Frisquet","name":"Frisquet Boiler","mdl":"Frisquet Boiler"}
}
)";
client.publish(payloadConfigTopic, payloadConfigPayload);

// FA: Configuration du capteur CDC
char tempCDCConfigTopic[] = "homeassistant/sensor/frisquet/CDC/config";
char tempCDCConfigPayload[] = R"(
{
  "uniq_id": "frisquet_tempCDC",
  "name": "Frisquet - Temperature CDC",
  "state_topic": "homeassistant/sensor/frisquet/CDC/state",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "device":{"ids":["FrisquetBoiler"],"mf":"Frisquet","name":"Frisquet Boiler","mdl":"Frisquet Boiler"}
}
)";
client.publish(tempCDCConfigTopic, tempCDCConfigPayload);

// FA: Configuration du capteur ECS
char tempECSConfigTopic[] = "homeassistant/sensor/frisquet/ECS/config";
char tempECSConfigPayload[] = R"(
{
  "uniq_id": "frisquet_tempECS",
  "name": "Frisquet - Temperature ECS",
  "state_topic": "homeassistant/sensor/frisquet/ECS/state",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "device":{"ids":["FrisquetBoiler"],"mf":"Frisquet","name":"Frisquet Boiler","mdl":"Frisquet Boiler"}
}
)";
client.publish(tempECSConfigTopic, tempECSConfigPayload);

// FA: Configuration du capteur Depart
char tempDepartConfigTopic[] = "homeassistant/sensor/frisquet/Depart/config";
char tempDepartConfigPayload[] = R"(
{
  "uniq_id": "frisquet_tempDepart",
  "name": "Frisquet - Temperature Depart",
  "state_topic": "homeassistant/sensor/frisquet/Depart/state",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "device":{"ids":["FrisquetBoiler"],"mf":"Frisquet","name":"Frisquet Boiler","mdl":"Frisquet Boiler"}
}
)";
client.publish(tempDepartConfigTopic, tempDepartConfigPayload);

}

void initOTA();

// FA: Demande de températures à la chaudière 
void requestBoiler(){
  Serial.printf("-- Appel Request Boiler ");
  int stateTx = radio.transmit(BoilerTx, 10);
  Serial.printf("- Transmit status : ");
  Serial.println(stateTx);
}

// FA: Fonction Publish to MQTT
void publishToMQTT(char* MQTT_Topic, float MQTT_Value){
  char MQTT_Payload[10];
  snprintf(MQTT_Payload, sizeof(MQTT_Payload), "%.2f", MQTT_Value);
  Serial.println(MQTT_Payload);
  if (!client.publish(MQTT_Topic, MQTT_Payload)) {
    Serial.println("Failed to publish temperature to MQTT");
  }    
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  
  // Initialize OLED display
  Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Disable*/, true /*Serial Enable*/);
  Heltec.display->init();
  Heltec.display->flipScreenVertically();
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->clear();

  int state = radio.beginFSK();
  state = radio.setFrequency(868.96);
  state = radio.setBitRate(25.0);
  state = radio.setFrequencyDeviation(50.0);
  state = radio.setRxBandwidth(250.0);
  state = radio.setPreambleLength(4);
  state = radio.setSyncWord(network_id, sizeof(network_id));

  initOTA();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialisation de la connexion MQTT
  client.setServer(mqttServer, mqttPort);
  connectToMqtt();
  publishDeviceEntities();
  requestBoiler();
}

// FA: Programme princpal
void loop() {
  if (!client.connected()) {
    connectToMqtt();
  }
  ArduinoOTA.handle();

  char message[255];
  
  // FA: Demande toutes les 500 écoutes
  if (b == 100) { // délais avant rappel à mettre en variable
    publishDeviceEntities();
    requestBoiler();
    b = 0;
  }
  // FA: Incrément compteur 
    b++;
  
  byte byteArr[RADIOLIB_SX126X_MAX_PACKET_LENGTH];
  int state = radio.receive(byteArr, 0);
  
  if (state == RADIOLIB_ERR_NONE) {

    // Envoi de la configuration du device Frisquet Boiler et des ses sensors
    publishDeviceEntities();
    
    int len = radio.getPacketLength();
    Serial.printf("RECEIVED [%2d] : ", len);
    message[0] = '\0';
    for (int i = 0; i < len; i++) {
      sprintf(message + strlen(message), "%02X ", byteArr[i]);
      Serial.printf("%02X ", byteArr[i]);}
      if (!client.publish("homeassistant/sensor/frisquet/payload/state", message)) {
        Serial.println("Failed to publish Payload to MQTT");
    }
    Serial.println("");

    // FA: Décodage chaîne de réponse suite à demande de températures
    if (len == 63) {
      Serial.println("- Trame chaudière reçue");
      
      // Extract bytes 9 and 9
      int decimalValueECS = byteArr[7] << 8 | byteArr[8];
      float ECSValue = decimalValueECS / 10.0;
      Serial.printf("Temperature ECS : ");

      // Publish temperature to the "frisquet_ECS" MQTT topic
      publishToMQTT("homeassistant/sensor/frisquet/ECS/state", ECSValue);
          
      // Extract bytes 10 and 11 - CDC
      int decimalValueCDC = byteArr[9] << 8 | byteArr[10];
      float CDCValue = decimalValueCDC / 10.0;
      Serial.printf("Temperature CDC : ");
     
      // Publish temperature to the "frisquet_CDC" MQTT topic
      publishToMQTT("homeassistant/sensor/frisquet/CDC/state", CDCValue);
      
      // Extract bytes 12 and 13
      int decimalValueDepart = byteArr[11] << 8 | byteArr[12];
      float DepartValue = decimalValueDepart / 10.0;
      Serial.printf("Temperature Depart : ");

      // Publish temperature to the "frisquet_Depart" MQTT topic
      publishToMQTT("homeassistant/sensor/frisquet/Depart/state", DepartValue);
      
      Heltec.display->drawString(0, 24, "Temperature CDC: " + String(CDCValue) + "°C");
      Heltec.display->drawString(0, 36, "Temperature ECS: " + String(ECSValue) + "°C");
      Heltec.display->drawString(0, 48, "Temperature Rad: " + String(DepartValue) + "°C");
      Heltec.display->display();
    }

    if (len == 49) {
      Serial.println("Trame satellite reçue");
      Serial.printf("Séquence : "); Serial.println(byteArr[5]);
    }

    if (len == 23) {  // Check if the length is 23 bytes

      Serial.println("- Trame satellite reçue");

      // Extract bytes 16 and 17
      int decimalValueTemp = byteArr[15] << 8 | byteArr[16];
      float tempAmbiante1Value = decimalValueTemp / 10.0;
      Serial.printf("Temperature : ");

      // Publish temperature to the "frisquet/temperature" MQTT topic
      publishToMQTT("homeassistant/sensor/frisquet/tempAmbiante1/state", tempAmbiante1Value);

      // Extract bytes 18 and 19
      int decimalValueCons = byteArr[17] << 8 | byteArr[18];
      float temperatureconsValue = decimalValueCons / 10.0;
      Serial.printf("Temperature consigne : ");

      // Publish temperature to the "frisquet/consigne" MQTT topic
      publishToMQTT("homeassistant/sensor/frisquet/consigne/state", temperatureconsValue);

      Heltec.display->drawString(0, 0, "Consigne Sat1: " + String(temperatureconsValue) + "°C");
      Heltec.display->drawString(0, 12, "Temperature Sat1: " + String(tempAmbiante1Value) + "°C");
      Heltec.display->display();

    }
   }
  client.loop();
}

void initOTA() {
 
  ArduinoOTA.setHostname("ESP32 Frisquetconnect");
  ArduinoOTA.setTimeout(25);  // Augmenter le délai d'attente à 25 secondes

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
}

