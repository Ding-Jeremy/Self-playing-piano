/*
* name:     main.cpp
  author:   Ding Jérémy
*/

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>

// Defines
#define D_OE_PIN 4
#define D_SERIAL_BAUD 9600
// Two boards with different I2C addresses
Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(0x40);
// Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(0x41);

void i2c_scan();

void setup()
{
  Serial.begin(D_SERIAL_BAUD);
  pwm1.begin();
  // pwm2.begin();
  pwm1.setPWMFreq(60); // Typical servo refresh rate
  pwm1.setPWM(15, 1000, 4095);
  pwm1.setPWM(1, 1000, 4095);
  pwm1.setPWM(2, 1000, 4095);

  // pwm2.setPWMFreq(60);
  i2c_scan();
}

void loop()
{
  // put your main code here, to run repeatedly:
  delay(1000);
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
