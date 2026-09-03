#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "web_server.h"
#include "camera_server.h"

HardwareSerial SerialESP(1);

void setup()
{
  // 1. Initialisation des communications
  Serial.begin(115200); // Vers le PC (Moniteur)

  // Vers l'Arduino (Broches 33 et 32 à 9600 bauds)
  SerialESP.begin(BAUD_RATE_ARDUINO, SERIAL_8N1, ESP_RX_PIN, ESP_TX_PIN);

  Serial.println("\n==================================");
  Serial.println("   CYBORG CARS - CONTROL CENTER   ");
  Serial.println("==================================");

  // 2. Connexion WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Recherche du reseau WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[OK] WiFi connecte !");

  // 3. Initialisation de la caméra et de son serveur dédié (Port 81)
  Serial.println("Initialisation du module vision...");
  initCamera(); // On tente d'allumer la caméra (si elle échoue, on continue quand même pour avoir le pilotage !)
  startCameraServer();

  // 4. Récupération et envoi de l'IP à l'Arduino
  String ipAddress = WiFi.localIP().toString();
  Serial.print("[INFO] Adresse IP du tableau de bord : http://");
  Serial.println(ipAddress);

  // L'ESP32 envoie un message texte à l'Arduino (Format : "IP:192.168.1.15")
  SerialESP.print("IP:");
  SerialESP.println(ipAddress);

  // 4. Lancement du serveur Web et de l'API
  startWebServer();
}

void loop()
{
  // Avec l'ESPAsyncWebServer, le processeur gère les requêtes web
  // en arrière-plan. La boucle principale est maintenant libre à 100% !
  delay(10);
}