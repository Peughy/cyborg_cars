#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Adresse 0x27, écran de 16 colonnes et 2 lignes
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Moteurs
const int ENA = 5;
const int ENB = 6;
const int IN1 = 13;
const int IN2 = 11;
const int IN3 = 12;
const int IN4 = 10;

// Ultrasons
const int TRI_US_FRONT = A0;
const int ECHO_US_FRONT = A1;
const int TRI_US_RIGHT = 2;
const int ECHO_US_RIGHT = 3;
const int TRI_US_LEFT = 7;
const int ECHO_US_LEFT = 4;

// LEDs et buzzer
const int LEFT_LED = A3;
const int RIGHT_LED = A2;
const int PIN_BUZZER = 8;

// Config navigation
const int STOP_FRONT_DIST = 20; // Augmenté pour éviter de taper les murs
const int SLOW_FRONT_DIST = 60; // Commence à ralentir un peu plus tôt
const int MAX_FRONT_DIST = 300;
const int STOP_LAT_DIST = 15;  // Marge latérale
const int HYSTERESIS_DIST = 3; // Marge anti-chattering plus robuste (évite le yoyo)
const int NORMAL_VEL = 180;
const int MAX_VEL = 255;
const int SLOW_VEL = 127;

// Config signalisation non-bloquante
const unsigned long INTERVALLE_CLIGNOTEMENT = 250; // ms

enum class Commande
{
  AVANCER,
  TOURNE_DROITE,
  TOURNE_GAUCHE,
  RECULER,
  ARRET_URGENCE
};

enum class PhaseRecuperation
{
  AUCUNE,
  RECUL,
  PIVOT
};

const unsigned long DUREE_RECUL = 400; // ms
const unsigned long DUREE_PIVOT = 500; // ms

struct Distances
{
  float avant;
  float gauche;
  float droite;
};

// --- Couche capteurs -------------------------------------------------

float mesurer_impulsion(int pin_trig, int pin_echo)
{
  digitalWrite(pin_trig, LOW);
  delayMicroseconds(2);
  digitalWrite(pin_trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pin_trig, LOW);

  // Avec 15000 en timeout, portée max à ~2,5 mètres.
  long duree = pulseIn(pin_echo, HIGH, 30000);

  if (duree == 0)
  {
    // Si timeout (aucun écho reçu avant 15ms), c'est que l'obstacle est au-delà
    // de 2,5 mètres. On renvoie donc une grande distance (voie libre).
    return 400.0;
  }

  return (duree * 0.034) / 2;
}

Distances lire_capteurs()
{
  static float dernier_avant = 400.0;
  static int err_avant = 0;

  static float dernier_gauche = 400.0;
  static int err_gauche = 0;

  static float dernier_droite = 400.0;
  static int err_droite = 0;

  Distances d;

  // 1. Lecture Avant
  float raw_avant = mesurer_impulsion(TRI_US_FRONT, ECHO_US_FRONT);
  delay(30);

  // 2. Lecture Gauche
  float raw_gauche = mesurer_impulsion(TRI_US_LEFT, ECHO_US_LEFT);
  delay(30);

  // 3. Lecture Droite
  float raw_droite = mesurer_impulsion(TRI_US_RIGHT, ECHO_US_RIGHT);
  delay(30);

  // --- FILTRE ANTI-GLITCH ---
  // Si on reçoit 400.0 (timeout), c'est souvent parce que le son a rebondi sur un mur de biais
  // et n'est pas revenu. On exige d'avoir 3 erreurs consécutives pour valider que la voie est vraiment libre.

  if (raw_avant >= 400.0)
  {
    err_avant++;
    if (err_avant >= 3)
      dernier_avant = 400.0;
  }
  else
  {
    err_avant = 0;
    dernier_avant = raw_avant;
  }
  d.avant = dernier_avant;

  if (raw_gauche >= 400.0)
  {
    err_gauche++;
    if (err_gauche >= 3)
      dernier_gauche = 400.0;
  }
  else
  {
    err_gauche = 0;
    dernier_gauche = raw_gauche;
  }
  d.gauche = dernier_gauche;

  if (raw_droite >= 400.0)
  {
    err_droite++;
    if (err_droite >= 3)
      dernier_droite = 400.0;
  }
  else
  {
    err_droite = 0;
    dernier_droite = raw_droite;
  }
  d.droite = dernier_droite;

  Serial.print("A:");
  Serial.print(d.avant);
  Serial.print(" G:");
  Serial.print(d.gauche);
  Serial.print(" D:");
  Serial.println(d.droite);

  return d;
}

// --- Couche décision (pure, pas d'action moteur ici) ------------------

Commande decider(const Distances &d)
{
  static Commande derniere_commande = Commande::AVANCER;
  static PhaseRecuperation phase_actuelle = PhaseRecuperation::AUCUNE;
  static unsigned long debut_phase = 0;
  static Commande direction_pivot = Commande::TOURNE_GAUCHE;

  // --- Gestion de la manœuvre de dégagement (non-bloquante) ---
  if (phase_actuelle != PhaseRecuperation::AUCUNE)
  {
    if (phase_actuelle == PhaseRecuperation::RECUL)
    {
      if (millis() - debut_phase < DUREE_RECUL)
      {
        derniere_commande = Commande::RECULER;
        return derniere_commande;
      }
      else
      {
        phase_actuelle = PhaseRecuperation::PIVOT;
        debut_phase = millis();
      }
    }

    if (phase_actuelle == PhaseRecuperation::PIVOT)
    {
      if (millis() - debut_phase < DUREE_PIVOT)
      {
        derniere_commande = direction_pivot;
        return derniere_commande;
      }
      else
      {
        phase_actuelle = PhaseRecuperation::AUCUNE; // Fin de la manœuvre
      }
    }
  }

  // --- Évaluation classique des distances ---
  bool en_virage = (derniere_commande == Commande::TOURNE_DROITE ||
                    derniere_commande == Commande::TOURNE_GAUCHE ||
                    derniere_commande == Commande::ARRET_URGENCE ||
                    derniere_commande == Commande::RECULER);

  // Marge de sortie d'état plus grande que l'entrée -> anti-chattering
  int seuil_front = en_virage ? STOP_FRONT_DIST + HYSTERESIS_DIST : STOP_FRONT_DIST;
  int seuil_lat = en_virage ? STOP_LAT_DIST + HYSTERESIS_DIST : STOP_LAT_DIST;

  // 1. Priorité absolue : bloqué de partout
  if (d.avant < seuil_front && d.gauche < seuil_lat && d.droite < seuil_lat)
  {
    derniere_commande = Commande::ARRET_URGENCE;
    return derniere_commande;
  }

  // 2. Obstacle frontal : déclenchement du dégagement (Recul + Pivot)
  if (d.avant < seuil_front)
  {
    phase_actuelle = PhaseRecuperation::RECUL;
    debut_phase = millis();

    // On choisit de pivoter vers le côté où on a le plus d'espace
    if (d.droite > d.gauche)
    {
      direction_pivot = Commande::TOURNE_DROITE;
    }
    else
    {
      direction_pivot = Commande::TOURNE_GAUCHE;
    }

    derniere_commande = Commande::RECULER;
    return derniere_commande;
  }

  // 3. Obstacles latéraux (seulement si l'avant est dégagé !)
  if (d.gauche < seuil_lat)
  {
    derniere_commande = Commande::TOURNE_DROITE;
    return derniere_commande;
  }

  if (d.droite < seuil_lat)
  {
    derniere_commande = Commande::TOURNE_GAUCHE;
    return derniere_commande;
  }

  // Par défaut
  derniere_commande = Commande::AVANCER;
  return derniere_commande;
}

// --- Couche action moteur ---------------------------------------------

void moteurs_avancer(int vel)
{
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, vel);
  analogWrite(ENB, vel);
}

void moteurs_reculer(int vel)
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENA, vel);
  analogWrite(ENB, vel);
}

void moteurs_tourner_gauche(int vel)
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, vel);
  analogWrite(ENB, vel);
}

void moteurs_tourner_droite(int vel)
{
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENA, vel);
  analogWrite(ENB, vel);
}

void moteurs_stop()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

void executer(Commande cmd, const Distances &d)
{
  switch (cmd)
  {
  case Commande::AVANCER:
  {
    int vitesse_cible;
    if (d.avant >= MAX_FRONT_DIST)
    {
      vitesse_cible = MAX_VEL;
    }
    else if (d.avant > SLOW_FRONT_DIST)
    {
      vitesse_cible = NORMAL_VEL;
    }
    else
    {
      // Décélération lissée entre SLOW_FRONT_DIST et STOP_FRONT_DIST
      vitesse_cible = map(d.avant, STOP_FRONT_DIST, SLOW_FRONT_DIST, SLOW_VEL, NORMAL_VEL);
      vitesse_cible = constrain(vitesse_cible, SLOW_VEL, NORMAL_VEL);
    }
    moteurs_avancer(vitesse_cible);
  }
  break;
  case Commande::RECULER:
    moteurs_reculer(SLOW_VEL);
    break;
  case Commande::TOURNE_DROITE:
    moteurs_tourner_droite(SLOW_VEL);
    break;
  case Commande::TOURNE_GAUCHE:
    moteurs_tourner_gauche(SLOW_VEL);
    break;
  case Commande::ARRET_URGENCE:
    moteurs_stop();
    break;
  default:
    moteurs_stop();
  }
}

// --- Couche signalisation (non-bloquante) ------------------------------

void signaler(Commande cmd)
{
  static Commande derniere_cmd_signalee = Commande::AVANCER;
  static unsigned long dernier_bascule = 0;
  static bool led_allumee = false;

  if (cmd != derniere_cmd_signalee)
  {
    derniere_cmd_signalee = cmd;
    led_allumee = false;
    dernier_bascule = millis();
    digitalWrite(LEFT_LED, LOW);
    digitalWrite(RIGHT_LED, LOW);
    noTone(PIN_BUZZER);
  }

  if (cmd == Commande::AVANCER)
    return; // rien à signaler en roulage normal

  if (millis() - dernier_bascule >= INTERVALLE_CLIGNOTEMENT)
  {
    dernier_bascule = millis();
    led_allumee = !led_allumee;

    // Reculer allume les deux LEDs comme des feux de détresse
    bool led_gauche = led_allumee && (cmd == Commande::TOURNE_GAUCHE || cmd == Commande::ARRET_URGENCE || cmd == Commande::RECULER);
    bool led_droite = led_allumee && (cmd == Commande::TOURNE_DROITE || cmd == Commande::ARRET_URGENCE || cmd == Commande::RECULER);

    digitalWrite(LEFT_LED, led_gauche);
    digitalWrite(RIGHT_LED, led_droite);

    if (led_allumee)
    {
      if (cmd == Commande::ARRET_URGENCE)
      {
        tone(PIN_BUZZER, 200);
      }
      else if (cmd == Commande::RECULER)
      {
        tone(PIN_BUZZER, 400); // Bip de recul (style engin de chantier)
      }
      else
      {
        tone(PIN_BUZZER, 250);
      }
    }
    else
    {
      noTone(PIN_BUZZER);
    }
  }
}

// --- Arduino ------------------------------------------------------------

void setup()
{
  Serial.begin(9600);

  pinMode(TRI_US_FRONT, OUTPUT);
  pinMode(ECHO_US_FRONT, INPUT);
  pinMode(TRI_US_LEFT, OUTPUT);
  pinMode(ECHO_US_LEFT, INPUT);
  pinMode(TRI_US_RIGHT, OUTPUT);
  pinMode(ECHO_US_RIGHT, INPUT);

  pinMode(LEFT_LED, OUTPUT);
  pinMode(RIGHT_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  // Configuration et allumage du LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Cyborg Cars init");
  lcd.setCursor(0, 1);
  lcd.print("En attente Wi-Fi");

  Serial.println("Arduino demarre. En attente des ordres de l'ESP32...");
}

void loop()
{
  Distances distances = lire_capteurs();
  Commande commande = decider(distances);

  executer(commande, distances);
  signaler(commande);

  if (Serial.available())
  {
    String message = Serial.readStringUntil('\n');
    message.trim();

    Serial.println("Recu de l'ESP32 : " + message);

    // ====================================================
    // ACTION A : RECEPTION DE L'ADRESSE IP
    // ====================================================
    if (message.startsWith("IP:"))
    {
      String ipStr = message.substring(3); //  "IP:"

      tone(PIN_BUZZER, 1000, 500);

      // Affichage sur l'ecran
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Reseau Connecte!");
      lcd.setCursor(0, 1);
      lcd.print(ipStr);
    }

    // ====================================================
    // ACTION B : RECEPTION DU MODE DE CONDUITE
    // ====================================================
    else if (message.startsWith("MODE:"))
    {
      String mode = message.substring(5); // MODE:

      tone(PIN_BUZZER, 1500, 100);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Mode en cours :");
      lcd.setCursor(0, 1);
      mode.toUpperCase();
      lcd.print(mode);
    }

    // ====================================================
    // ACTION C : RECEPTION DES MOUVEMENTS MANUELS
    // ====================================================
    else if (message.startsWith("DIR:"))
    {
      String direction = message.substring(4);

      // direction contiendra "F" (Avant), "B" (Arrière), "L" (Gauche), "R" (Droite) ou "S" (Stop)
      // -> ICI : Tu ajouteras tes fonctions pour faire tourner tes roues !
      // Exemple : if(direction == "F") { avancer(); }
    }
  }
}
