#include <Arduino.h>
#include <WiFi.h>

const char *WIFI_SSID = "ssid";
const char *WIFI_PASSWORD = "******";

void setup()
{
  Serial.begin(9600);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('C');
    delay(500);
  }

  Serial.print('K');
}

void loop()
{
}