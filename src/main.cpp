#include <Arduino.h>

// Motors
const int ENA = 10;
const int ENB = 11;

const int IN1 = 2;
const int IN2 = 3;
const int IN3 = 4;
const int IN4 = 5;

// Ultrasonics
const int TRI_US_FRONT = 12;
const int ECHO_US_FRONT = 13;

const int TRI_US_RIGHT = A0;
const int ECHO_US_RIGHT = A1;

const int TRI_US_LEFT = 8;
const int ECHO_US_LEFT = 9;

// Leds and buzzer
const int LEFT_LED = 6;
const int RIGHT_LED = 7;
const int PIN_BUZZER = A5;

// Configuration
const int NBR_MEASURE = 1;
const int SLOW_FRONT_DIST = 30;
const int STOP_FRONT_DIST = 8;
const int STOP_LAT_DIST = 10;
const int MAX_VEL = 180;
const int SLOW_VEL = 127;

// functions for move cars

void forward(int vel)
{
  digitalWrite(IN1, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN4, LOW);

  analogWrite(ENA, vel);
  analogWrite(ENB, vel);
}

void backward(int vel)
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN4, HIGH);

  analogWrite(ENA, vel);
  analogWrite(ENB, vel);
}

void turn_left(int vel)
{
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  analogWrite(ENA, vel);
  analogWrite(ENB, vel);
}

void turn_right(int vel)
{
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  analogWrite(ENA, vel);
  analogWrite(ENB, vel);
}

void stop()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

// get distance and navigate

float get_distances(int pin_trig, int pin_echo)
{

  digitalWrite(pin_trig, LOW);
  delayMicroseconds(2);
  digitalWrite(pin_trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pin_trig, LOW);

  long time = pulseIn(pin_echo, HIGH, 15000);

  if (time == 0)
    return 150.0;

  float dist = (time * 0.034) / 2;
  return dist;
}

void navigation(float dist_front, float dist_left, float dist_right)
{
  if (dist_front < SLOW_FRONT_DIST && dist_front > STOP_FRONT_DIST)
  {
    if (dist_left < STOP_LAT_DIST)
    {
      turn_right(SLOW_VEL);

      Serial.print("FRONT: ");
      Serial.print(dist_front);
      Serial.print(" - LEFT: ");
      Serial.print(dist_left);
      Serial.print(" - RIGHT: ");
      Serial.print(dist_right);
      Serial.println(" - TURN RIGHT AND FORWARD");
    }
    else if (dist_right < STOP_LAT_DIST)
    {
      turn_left(SLOW_VEL);

      Serial.print("FRONT: ");
      Serial.print(dist_front);
      Serial.print(" - LEFT: ");
      Serial.print(dist_left);
      Serial.print(" - RIGHT: ");
      Serial.print(dist_right);
      Serial.println(" - TURN LEFT AND FORWARD");
    }
    else
    {
      turn_right(SLOW_VEL);

      Serial.print("FRONT: ");
      Serial.print(dist_front);
      Serial.print(" - LEFT: ");
      Serial.print(dist_left);
      Serial.print(" - RIGHT: ");
      Serial.print(dist_right);
      Serial.println(" - TURN RIGHT AND FORWARD");
    }
  }
  else if (dist_front < STOP_FRONT_DIST)
  {
    stop();
    if (dist_left < STOP_LAT_DIST)
    {
      turn_right(SLOW_VEL);

      Serial.print("FRONT: ");
      Serial.print(dist_front);
      Serial.print(" - LEFT: ");
      Serial.print(dist_left);
      Serial.print(" - RIGHT: ");
      Serial.print(dist_right);
      Serial.println(" - TURN RIGHT AND STOP");
    }
    else if (dist_right < STOP_LAT_DIST)
    {
      turn_left(SLOW_VEL);

      Serial.print("FRONT: ");
      Serial.print(dist_front);
      Serial.print(" - LEFT: ");
      Serial.print(dist_left);
      Serial.print(" - RIGHT: ");
      Serial.print(dist_right);
      Serial.println(" - TURN LEFT AND STOP");
    }
    else
    {
      turn_right(SLOW_VEL);

      Serial.print("FRONT: ");
      Serial.print(dist_front);
      Serial.print(" - LEFT: ");
      Serial.print(dist_left);
      Serial.print(" - RIGHT: ");
      Serial.print(dist_right);
      Serial.println(" - TURN RIGHT AND STOP");
    }
  }
  else
  {

    if (dist_left < STOP_LAT_DIST)
    {
      turn_right(SLOW_VEL);

      Serial.print("FRONT: ");
      Serial.print(dist_front);
      Serial.print(" - LEFT: ");
      Serial.print(dist_left);
      Serial.print(" - RIGHT: ");
      Serial.print(dist_right);
      Serial.println(" - TURN RIGHT AND STOP");
    }
    else if (dist_right < STOP_LAT_DIST)
    {
      turn_left(SLOW_VEL);

      Serial.print("FRONT: ");
      Serial.print(dist_front);
      Serial.print(" - LEFT: ");
      Serial.print(dist_left);
      Serial.print(" - RIGHT: ");
      Serial.print(dist_right);
      Serial.println(" - TURN LEFT AND STOP");
    }
    else
    {
      forward(MAX_VEL);

      Serial.print("FRONT: ");
      Serial.print(dist_front);
      Serial.print(" - LEFT: ");
      Serial.print(dist_left);
      Serial.print(" - RIGHT: ");
      Serial.print(dist_right);
      Serial.println(" - FORWARD");
    }
  }
}

void setup()
{
  Serial.begin(9600);

  // Capteur AVANT
  pinMode(TRI_US_FRONT, OUTPUT);
  pinMode(ECHO_US_FRONT, INPUT);

  pinMode(TRI_US_LEFT, OUTPUT);
  pinMode(ECHO_US_LEFT, INPUT);

  pinMode(TRI_US_RIGHT, OUTPUT);
  pinMode(ECHO_US_RIGHT, INPUT);
}

void loop()
{
  float dist_front = get_distances(TRI_US_FRONT, ECHO_US_FRONT);

  float dist_left = get_distances(TRI_US_LEFT, ECHO_US_LEFT);

  float dist_right = get_distances(TRI_US_RIGHT, ECHO_US_RIGHT);

  navigation(dist_front, dist_left, dist_right);
}
