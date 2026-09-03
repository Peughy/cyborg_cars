#include "web_server.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "config.h"

AsyncWebServer server(80);

extern HardwareSerial SerialESP;

void setupRoutes()
{

    // 1. ROUTE PRINCIPALE
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html"); });

    // 2. API : Changement de Mode
    server.on("/setMode", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("m")) {
            String mode = request->getParam("m")->value();
            Serial.println("[API] Changement de mode demande : " + mode);
            
            // Envoi de l'ordre à l'Arduino
            SerialESP.print("MODE:");
            SerialESP.println(mode);
            
            request->send(200, "text/plain", "Mode mis a jour");
        } else {
            request->send(400, "text/plain", "Parametre 'm' manquant");
        } });

    // 3. API : Mouvements Moteurs
    server.on("/action", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("dir")) {
            String direction = request->getParam("dir")->value();
            Serial.println("[API] Commande moteur recue : " + direction);
            
            // Envoi de l'ordre à l'Arduino
            SerialESP.print("DIR:");
            SerialESP.println(direction);
            
            request->send(200, "text/plain", "Commande envoyee");
        } else {
            request->send(400, "text/plain", "Parametre 'dir' manquant");
        } });

    // 4. API : Lecture de la Télémétrie
    server.on("/telemetry", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // On renvoie un JSON
        String json = "{\"a\": 45, \"g\": 12, \"d\": 80}";
        request->send(200, "application/json", json); });
}

void startWebServer()
{
    // Montage de la partition LittleFS
    if (!LittleFS.begin(true))
    {
        Serial.println("ERREUR: Impossible de monter LittleFS.");
        return;
    }
    Serial.println("LittleFS monte avec succes.");

    // Initialisation et démarrage du serveur Asynchrone
    setupRoutes();
    server.begin();
    Serial.println("Serveur Web Asynchrone demarre et en ecoute !");
}