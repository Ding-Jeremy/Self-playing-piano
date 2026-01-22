/*
* name:     main.cpp
  author:   Ding Jérémy
*/

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>

// Defines
#define D_OE_PIN 4
#define D_SERIAL_BAUD 9600 // bit/s
#define D_I2C_SPEED 200000 // bit/s
#define D_PWM_FREQ 100     // Hz
// Two boards with different I2C addresses
Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(0x40);
// Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(0x41);

uint16_t brightness = 0;
uint16_t leds_state = 0x01;

unsigned long startTime;
unsigned long elapsedTime;

void i2c_scan();

void setup()
{
  Serial.begin(D_SERIAL_BAUD);
  pwm1.begin();
  // Set SCL speed
  Wire.setClock(D_I2C_SPEED); // 400 kHz
  // set totem pole output
  pwm1.setOutputMode(true);

  pwm1.setPWMFreq(D_PWM_FREQ);

  for (uint8_t i = 0; i < 16; i++)
  {
    pwm1.setPWM(i, 0, 0);
  }
}

void loop()
{
  startTime = micros(); // capture start time

  for (uint8_t i = 0; i < 10; i++)
  {
    pwm1.setPWM(i, 0, 0xFF); // your code
  }

  elapsedTime = micros() - startTime; // compute elapsed microseconds

  Serial.print("Time taken: ");
  Serial.print(elapsedTime);
  Serial.println(" us");

  delay(1000); // wait a second to see output
}

/*
 * Scans for available addresses on the bus.
 * pass a 1D array of size 127.
 * 1 = Present
 * 0 = absent
 */
void i2c_scan()
{
  uint8_t index, error;
  for (index = 1; index < 128; index++)
  {
    Wire.beginTransmission(index);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at 0x");
      Serial.print(index, HEX);
    }
  }
}
