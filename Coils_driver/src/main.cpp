/*
* name:     main.cpp
  author:   Ding Jérémy
*/
#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);

void setup()
{
  Serial.begin(9600);
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
  Serial.println("Hell world");
}

void loop()
{
  // put your main code here, to run repeatedly:
  Serial.println("Hello world");
  delay(1000);
}

// put function definitions here:
int myFunction(int x, int y)
{
  return x + y;
}