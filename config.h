// config.h
#ifndef CONFIG_H
#define CONFIG_H

// Configuration Wifi
const char* ssid = "ssid wifi";  // Mettre votre SSID Wifi
const char* password = "wifi password";  // Mettre votre mot de passe Wifi

// Définition de l'adresse du broket MQTT
const char* mqttServer = "192.168.XXX.XXX"; // Mettre l'IP du serveur MQTT
const int mqttPort = 1883;

// MQTT secrets
const char* mqttUsername = "mqttUsername"; // Mettre le user MQTT
const char* mqttPassword = "mqttPassword"; // Mettre votre mot de passe MQTT

// NetworkID secret
uint8_t network_id[] = {0xNN, 0xNN, 0xNN, 0xNN}; // Remplacer NN par le network id de la chaudière

#endif // CONFIG_H
