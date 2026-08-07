#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int LEFT_LED = A2;
const int RIGHT_LED = A3;
const int PIN_BUZZER = 8;

LiquidCrystal_I2C lcd(0x27, 16, 2);

void blink_leds(int nb_fois)
{
  for (int i = 0; i < nb_fois; i++)
  {
    digitalWrite(LEFT_LED, HIGH);
    digitalWrite(RIGHT_LED, HIGH);
    delay(200);
    digitalWrite(LEFT_LED, LOW);
    digitalWrite(RIGHT_LED, LOW);
    delay(200);
  }
}

void connexion_reussie()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connexion");
  lcd.setCursor(0, 1);
  lcd.print("reussie");

  blink_leds(3);
  tone(PIN_BUZZER, 250, 500);
}

void connexion_en_cours()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connexion");
  lcd.setCursor(0, 1);
  lcd.print("en cours...");

  digitalWrite(LEFT_LED, HIGH);
  digitalWrite(RIGHT_LED, LOW);
}

void setup()
{
  Serial.begin(9600);

  pinMode(LEFT_LED, OUTPUT);
  pinMode(RIGHT_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  lcd.init();
  lcd.backlight();

  connexion_en_cours();
}

void loop()
{
  if (Serial.available())
  {
    char etat = Serial.read();

    if (etat == 'C')
      connexion_en_cours();
    else if (etat == 'K')
      connexion_reussie();
  }
}