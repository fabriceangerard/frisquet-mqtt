#include <Arduino.h>
#include <RadioLib.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

SX1262 radio = new Module(SS, DIO0, RST_LoRa, BUSY_LoRa); 

// Configuration Wifi
const char* ssid = "";  // Mettre votre SSID Wifi
const char* password = "";  // Mettre votre mot de passe Wifi

// Définition de l'adresse du broket MQTT
const char* mqttServer = ""; // Mettre l'ip du serveur mqtt
const int mqttPort = 1883;

// MQTT secrets
const char* mqttUsername = ""; // Mettre le user mqtt
const char* mqttPassword = ""; // Mettre votre mot de passe mqtt

// NeetworkID secret
uint8_t network_id[] = {0xNN, 0xNN, 0xNN, 0xNN}; // remplacer NN par le network id de la chaudiere

int b;  // Boiler counter

// FA: Variables communes
// -- variables à utiliser plus tard
byte fromID = 0x80; // 01 - 80 (boiler)
byte toID = 0x08;   // 02 - 08 (satellite)
byte reqID = 0xCA;  // 03 - CA
byte msgNum = 0x96; // 04 - 95
byte DemRep = 0x01; // 05 - 01 Demande ou 81 Réponse
byte reqMsg[] = {0x79, 0xE0, 0x00, 0x1C}; // 07-10 - Chaîne de demande de températures chandière [0x79, 0xe0, 0x00, 0x1c]


// -- Première initialisation de la chaîne de requête a la chaudière
uint8_t BoilerTx[] = {fromID, toID, reqID, msgNum, DemRep, 0x03, reqMsg[0], reqMsg[1], reqMsg[2], reqMsg[3]};

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

void connectToTopic() {
  // Configuration du capteur de température
  char temperatureConfigTopic[] = "homeassistant/sensor/frisquet_temperature/config";
  char temperatureConfigPayload[] = R"(
  {
    "name": "Frisquet - Temperature interieur",
    "state_topic": "homeassistant/sensor/frisquet_temperature/state",
    "unit_of_measurement": "°C",
    "device_class": "temperature"
  }
  )";
  client.publish(temperatureConfigTopic, temperatureConfigPayload);
  
    // Configuration du capteur de température de consigne
  char tempconsigneConfigTopic[] = "homeassistant/sensor/frisquet_consigne/config";
  char tempconsigneConfigPayload[] = R"(
  {
    "name": "Frisquet - Temperature consigne",
    "state_topic": "homeassistant/sensor/frisquet_consigne/state",
    "unit_of_measurement": "°C",
    "device_class": "temperature"
  }
  )";
  client.publish(tempconsigneConfigTopic, tempconsigneConfigPayload);
  
  // Configuration récupération Payload
  char payloadConfigTopic[] = "homeassistant/sensor/frisquet_payload/config";
  char payloadConfigPayload[] = R"(
  {
    "name": "Frisquet - Payload",
    "state_topic": "homeassistant/sensor/frisquet_payload/state"
  }
  )";
  client.publish(payloadConfigTopic, payloadConfigPayload);
  
  // FA: Configuration du capteur CDC
  char tempCDCConfigTopic[] = "homeassistant/sensor/frisquet_CDC/config";
  char tempCDCConfigPayload[] = R"(
  {
    "name": "Frisquet - Temperature CDC",
    "state_topic": "homeassistant/sensor/frisquet_CDC/state",
    "unit_of_measurement": "°C",
    "device_class": "temperature"
  }
  )";
  client.publish(tempCDCConfigTopic, tempCDCConfigPayload);
  
  // FA: Configuration du capteur ECS
  char tempECSConfigTopic[] = "homeassistant/sensor/frisquet_ECS/config";
  char tempECSConfigPayload[] = R"(
  {
    "name": "Frisquet - Temperature ECS",
    "state_topic": "homeassistant/sensor/frisquet_ECS/state",
    "unit_of_measurement": "°C",
    "device_class": "temperature"
  }
  )";
  client.publish(tempECSConfigTopic, tempECSConfigPayload);
  
  // FA: Configuration du capteur Depart
  char tempDepartConfigTopic[] = "homeassistant/sensor/frisquet_Depart/config";
  char tempDepartConfigPayload[] = R"(
  {
    "name": "Frisquet - Temperature Depart",
    "state_topic": "homeassistant/sensor/frisquet_Depart/state",
    "unit_of_measurement": "°C",
    "device_class": "temperature"
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
  connectToTopic();
  requestBoiler();
}

// FA: Programme princpal
void loop() {
  if (!client.connected()) {
    connectToMqtt();
  }
  ArduinoOTA.handle();
  connectToTopic();

  char message[255];
  
  // FA: Demande toutes les 500 écoutes
  if (b == 100) { // délais avant rappel à mettre en variable
    requestBoiler();
    b = 0;
  }
  // FA: Incrément compteur 
  b++;
  
  byte byteArr[RADIOLIB_SX126X_MAX_PACKET_LENGTH];
  int state = radio.receive(byteArr, 0);
  
  if (state == RADIOLIB_ERR_NONE) {
    int len = radio.getPacketLength();
    Serial.printf("RECEIVED [%2d] : ", len);
    message[0] = '\0';
    for (int i = 0; i < len; i++) {
      sprintf(message + strlen(message), "%02X ", byteArr[i]);
      Serial.printf("%02X ", byteArr[i]);
    }
    
    if (!client.publish("homeassistant/sensor/frisquet_payload/state", message)) {
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
      publishToMQTT("homeassistant/sensor/frisquet_ECS/state", ECSValue);
          
      // Extract bytes 10 and 11 - CDC
      int decimalValueCDC = byteArr[9] << 8 | byteArr[10];
      float CDCValue = decimalValueCDC / 10.0;
      Serial.printf("Temperature CDC : ");
     
      // Publish temperature to the "frisquet_CDC" MQTT topic
      publishToMQTT("homeassistant/sensor/frisquet_CDC/state", CDCValue);
      
      // Extract bytes 12 and 13
      int decimalValueDepart = byteArr[11] << 8 | byteArr[12];
      float DepartValue = decimalValueDepart / 10.0;
      Serial.printf("Temperature Depart : ");

      // Publish temperature to the "frisquet_Depart" MQTT topic
      publishToMQTT("homeassistant/sensor/frisquet_Depart/state", DepartValue);
    }

    if (len == 49) {
      Serial.println("Trame satellite reçue");
      Serial.printf("Séquence : "); Serial.println(byteArr[5]);
    }

    if (len == 23) {  // Check if the length is 23 bytes
      Serial.println("- Trame satellite reçue");

      // Extract bytes 16 and 17
      int decimalValueTemp = byteArr[15] << 8 | byteArr[16];
      float temperatureValue = decimalValueTemp / 10.0;
      Serial.printf("Temperature : "); Serial.println(temperatureValue);

      // Extract bytes 18 and 19
      int decimalValueCons = byteArr[17] << 8 | byteArr[18];
      float temperatureconsValue = decimalValueCons / 10.0;
      Serial.printf("Temperature consigne : "); Serial.println(temperatureconsValue);

      // Publish temperature to the "frisquet_temperature" MQTT topic
      char temperatureTopic[] = "homeassistant/sensor/frisquet_temperature/state";
      char temperaturePayload[10];
      snprintf(temperaturePayload, sizeof(temperaturePayload), "%.2f", temperatureValue);
      if (!client.publish(temperatureTopic, temperaturePayload)) {
        Serial.println("Failed to publish temperature to MQTT");
      }
      
      // Publish temperature to the "tempconsigne" MQTT topic
      char tempconsigneTopic[] = "homeassistant/sensor/frisquet_consigne/state";
      char tempconsignePayload[10];
      snprintf(tempconsignePayload, sizeof(tempconsignePayload), "%.2f", temperatureconsValue);
      if (!client.publish(tempconsigneTopic, tempconsignePayload)) {
        Serial.println("Failed to publish consigne to MQTT");
      }
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
