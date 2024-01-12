#include <Arduino.h>
#include <RadioLib.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <heltec.h>
#include <config.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

SX1262 radio = new Module(SS, DIO0, RST_LoRa, BUSY_LoRa); 

// FA: Variables permettant d'envoyer une requête

byte toID = 0x80;     // 01 - 80 (boiler)
byte fromID = 0x08;   // 02 - 08 (satellite) ou 7E (connect)
byte reqID = 0xCA;    // 03 - CA !! A modifier pour chaque chaudière / périphérique - à récupérer dans une trame 23 ou 49 
byte msgNum = 0x01;   // 04 - 96 !! Potentiellement à modifier à chaque transmission (incrémetation à mettre en place)
byte DemRep = 0x01;   // 05 - 01 Demande ou 81 Réponse
byte reqMsg[] = {0x79, 0xE0, 0x00, 0x1C}; // 07-10 - Chaîne de demande de températures chandière [0x79, 0xe0, 0x00, 0x1c]

byte toDestID;       // 04 - 08 (satellite) ou 7E (connect)

// -- Première initialisation de la chaîne de requête a la chaudière
uint8_t BoilerTx[] = {toID, fromID, reqID, msgNum, DemRep, 0x03, reqMsg[0], reqMsg[1], reqMsg[2], reqMsg[3]};

// Déclaration des variables de valeurs des capteurs 
float DepartValue;
float CDCValue;
float ECSValue;
float tempAmbiante1Value;
float temperatureconsValue;
float ExtValue;
float Ambi1Value;
float Cons1Value;
float ModeValue;
int decimalValue;

String tempAmbiante;
String tempExterieure;
String tempConsigne;
String modeFrisquet;
String byteArrayToHexString(uint8_t *byteArray, int length);
String DateTimeRes;

unsigned long RefTime;
unsigned long ReqBoilerDelay = 600000;// requête à la chaudière toutes les 10mn
unsigned long DeltaTime = 600000;

char message[255];

// Drapeaux pour indiquer si les données ont changé
bool tempAmbianteChanged = false;
bool tempExterieureChanged = false;
bool tempConsigneChanged = false;
bool modeFrisquetChanged = false;
 
WiFiClient espClient;
PubSubClient client(espClient);

WiFiUDP ntpUDP;
/*
* Choix du serveur NTP pour récupérer l'heure, 3600 =1h est le fuseau horaire et 60000=60s est le * taux de rafraichissement
*/
NTPClient temps(ntpUDP, "fr.pool.ntp.org", 3600, 60000);

void initOTA();

// Fonction DateTime basée sur NTPClient, retourne YYYYMMDD-HHMMSS dans DateTimeRes
void DateTime()
{
  temps.update();
  time_t epochTime = temps.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  int currentYear = ptm->tm_year+1900;
  char buffer[15];
  String resfinal;
   
  sprintf(buffer, "%04d", currentYear);
    resfinal = String(buffer);
  sprintf(buffer, "%02d", currentMonth); 
    resfinal = resfinal + String(buffer);
  sprintf(buffer, "%02d", monthDay); 
    resfinal = resfinal + String(buffer) + "-";

  sprintf(buffer, "%02d", temps.getHours()); 
    resfinal = resfinal + String(buffer);
  sprintf(buffer, "%02d", temps.getMinutes()); 
    resfinal = resfinal + String(buffer);
  sprintf(buffer, "%02d", temps.getSeconds()); 
    resfinal = resfinal + String(buffer);

  DateTimeRes=resfinal;
}

void connectToMqtt() 
{
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32 Frisquet", mqttUsername, mqttPassword))
      {
      Serial.println("Connected to MQTT");
      } 
    else 
      {
      Serial.print("Failed to connect to MQTT, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
      }
  }
}

void publishDeviceEntities()
{
  // Initialisation de la connexion MQTT
  client.setServer(mqttServer, mqttPort);
  client.setBufferSize(2048);
  
  client.subscribe("homeassistant/select/frisquet/mode/set");
  client.subscribe("homeassistant/sensor/frisquet/tempAmbiante/state");
  client.subscribe("homeassistant/sensor/frisquet/tempExterieure/state");
  client.subscribe("homeassistant/sensor/wen_shi_du_chuan_gan_qi_wifi_2_temperature/state");

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

  // FA: Configuration du capteur Mode
  char modeConfigTopic[] = "homeassistant/sensor/frisquet/mode/config";
  char modeConfigPayload[] = R"(
  {
    "uniq_id": "frisquet_mode",
    "name": "Frisquet - Mode chauffage",
    "state_topic": "homeassistant/sensor/frisquet/mode/state",
    "unit_of_measurement": "",
    "device_class": "",
    "device":{"ids":["FrisquetBoiler"],"mf":"Frisquet","name":"Frisquet Boiler","mdl":"Frisquet Boiler"}
  }
  )";
  client.publish(modeConfigTopic, modeConfigPayload);
}

// FA: Requête de demande de températures à la chaudière sur la base de BoilerTx[]
void requestBoiler()
{
  // Serial.println("-- Appel Request Boiler ");
  DateTime();

  msgNum=msgNum+0x01;
  BoilerTx[3]=msgNum;

  int stateTx = radio.transmit(BoilerTx, 10);
  int len10 = 10;
    Serial.printf("SENT [%2d];", len10);
    Serial.print(DateTimeRes);

    message[0] = '\0';
    for (int ct = 0; ct < len10; ct++) 
    {
      sprintf(message + strlen(message), "%02X ", BoilerTx[ct]);
      Serial.printf(";%02X", BoilerTx[ct]);
    }
    Serial.println("");
}

// FA: Fonction Publish to MQTT
void publishToMQTT(char* MQTT_Topic, float MQTT_Value)
{
  char MQTT_Payload[10];
  snprintf(MQTT_Payload, sizeof(MQTT_Payload), "%.2f", MQTT_Value);
  if (!client.publish(MQTT_Topic, MQTT_Payload)) 
  {
    Serial.println("Failed to publish temperature to MQTT");
  }    
}

void callback(char *topic, byte *payload, unsigned int length)
{
  // Convertir le payload en une chaîne de caractères
  char message[length + 1];
  strncpy(message, (char *)payload, length);
  message[length] = '\0';

  // Vérifier le topic et agir en conséquence
  // Vérifier le topic et mettre à jour les variables globales
  if (strcmp(topic, "homeassistant/sensor/wen_shi_du_chuan_gan_qi_wifi_2_temperature/state") == 0)
  {
    if (tempAmbiante != String(message))
    {
      tempAmbiante = String(message);
      Serial.println(tempAmbiante);
      tempAmbianteChanged = true;
    }
  }
  else if (strcmp(topic, "homeassistant/sensor/frisquet/tempExterieure/state") == 0)
  {
    if (tempExterieure != String(message))
    {
      tempExterieure = String(message);
      tempExterieureChanged = true;
    }
  }
  else if (strcmp(topic, "homeassistant/sensor/frisquet/tempConsigne/state") == 0)
  {
    if (tempConsigne != String(message))
    {
      tempConsigne = String(message);
      tempConsigneChanged = true;
    }
  }
  else if (strcmp(topic, "homeassistant/select/frisquet/mode/set") == 0)
  {
    if (modeFrisquet != String(message))
    {
      modeFrisquet = String(message);
      modeFrisquetChanged = true;
      client.publish("homeassistant/select/frisquet/mode/state", message);
    }
  }
}

// Affichage sur l'écran Heltec
void AfficheHeltec()
{
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "Sat1: "  + String(Ambi1Value,1 ) + "°C / "+ String(Cons1Value,1) + "°C");
  Heltec.display->drawString(0, 12, "CDC/ECS : " + String(CDCValue, 1) + " / "  + String(ECSValue,1) + "°C");
  Heltec.display->drawString(0, 24, "Temperature Rad: " + String(DepartValue,1) + "°C");
  Heltec.display->drawString(0, 40, "MAJ: " + DateTimeRes);
  Heltec.display->display();
}
void setup() 
{
 
  // Initialize Wifi connection
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) 
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  
  temps.begin();

  // Initialisation de l'écran OLED
  Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Disable*/, true /*Serial Enable*/);
  Heltec.display->init();
  Heltec.display->flipScreenVertically();
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->clear();

  // Initialisation de la radio
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
  client.setCallback(callback);
  connectToMqtt();
  publishDeviceEntities();
}

// FA: Programme princpal
void loop() {
  if (!client.connected()) 
  {
    connectToMqtt();
    requestBoiler();
  }
  
  // initialise un msgNum aléatoire
  if (msgNum==1)
  {
    Serial.printf ("%2d", millis());
    randomSeed(long(temps.getSeconds()));
    msgNum=(random(1,90),HEX);
    // Serial.println(msgNum);
  }

  temps.update();
  ArduinoOTA.handle();

  // FA: Demande toutes les ReqBoilerDelay millisecondes
   if (DeltaTime >= ReqBoilerDelay)
    {
    requestBoiler();
    RefTime=millis();
    }
  DeltaTime = millis()-RefTime;

  byte byteArr[RADIOLIB_SX126X_MAX_PACKET_LENGTH];
  int state = radio.receive(byteArr, 0);
  DateTime();
  
  if (state == RADIOLIB_ERR_NONE) 
  {
    // Envoi de la configuration du device Frisquet Boiler et des ses sensors
    publishDeviceEntities();
    
    int len = radio.getPacketLength();

    Serial.printf("RECEIVED [%2d];", len);
    Serial.print(DateTimeRes);

    message[0] = '\0';
    for (int i = 0; i < len; i++) 
    {
      sprintf(message + strlen(message), "%02X", byteArr[i]);
      Serial.printf(";%02X", byteArr[i]);
    }
      if (!client.publish("homeassistant/sensor/frisquet/payload/state", message)) 
      {
        Serial.println("Failed to publish Payload to MQTT");
      }
      Serial.println("");
          
    fromID=byteArr[1];
    toID=byteArr[0];
    toDestID=byteArr[4];

    // Serial.printf("> ");
    // Serial.printf("%02X ",toID); 

    // FA: Décodage chaîne de réponse suite à demande de températures
    if ((len == 63) and (toID==0x08) and (toDestID==0x81))
    {
      // Serial.println("- Réponse chaudière 63 reçue");
          
      // Extract bytes 9 and 9 and publish
      decimalValue = byteArr[7] << 8 | byteArr[8];
      ECSValue = decimalValue / 10.0;
      publishToMQTT("homeassistant/sensor/frisquet/ECS/state", ECSValue);
          
      // Extract bytes 10 and 11 - CDC
      decimalValue = byteArr[9] << 8 | byteArr[10];
      CDCValue = decimalValue / 10.0;
      publishToMQTT("homeassistant/sensor/frisquet/CDC/state", CDCValue);
      
      // Extract bytes 12 and 13
      decimalValue = byteArr[11] << 8 | byteArr[12];
      DepartValue = decimalValue / 10.0;
      publishToMQTT("homeassistant/sensor/frisquet/Depart/state", DepartValue);

      // Extract bytes 44 and 45 - Consigne 1
      decimalValue = byteArr[43] << 8 | byteArr[44];
      Ambi1Value = decimalValue / 10.0;
      publishToMQTT("homeassistant/sensor/frisquet/consigne/state", Cons1Value);
      
      // Extract bytes 56 and 57 - Consigne 1
      decimalValue = byteArr[55] << 8 | byteArr[56];
      Cons1Value = decimalValue / 10.0;
      publishToMQTT("homeassistant/sensor/frisquet/consigne/state", Cons1Value);

       // Extract bytes 58 and 59 - Externe
      decimalValue = byteArr[57] << 8 | byteArr[58];
      ExtValue = decimalValue / 10.0;

    AfficheHeltec();
    }

    if (len == 49)
    {
      // Serial.println("Trame satellite reçue");
    }

      
    if (len == 23)// Check if the length is 23 bytes
    {
      // Serial.println("- Trame 23 reçue");

      // Extract bytes 16 and 17
      int decimalValueTemp = byteArr[15] << 8 | byteArr[16];
      Ambi1Value = decimalValueTemp / 10.0;
      publishToMQTT("homeassistant/sensor/frisquet/tempAmbiante1/state", Ambi1Value);

      // Extract bytes 18 and 19
      int decimalValueCons = byteArr[17] << 8 | byteArr[18];
      Cons1Value = decimalValueCons / 10.0;
      publishToMQTT("homeassistant/sensor/frisquet/consigne/state", Cons1Value);

      // Extract bytes 21
      int decimalValueMode = byteArr[20];
      ModeValue = decimalValueMode;
      publishToMQTT("homeassistant/sensor/frisquet/mode/state", ModeValue);

      AfficheHeltec();
    }
   }
  client.loop();
}

void initOTA() 
{
  ArduinoOTA.setHostname("ESP32 Frisquetconnect");
  ArduinoOTA.setTimeout(25);  // Augmenter le délai d'attente à 25 secondes

  ArduinoOTA
  .onStart([]() 
  {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() 
  {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) 
  {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) 
  {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
}
