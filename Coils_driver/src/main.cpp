/*
* name:     main.cpp
  author:   Ding Jérémy
*/

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>

//-------------- DEFINES ---------------
#define D_OE_PIN 4
#define D_SERIAL_BAUD 9600 // bit/s
#define D_I2C_SPEED 200000 // bit/s
#define D_PWM_FREQ 100     // Hz

#define D_SPI_BUFFSIZE 9

//-------------- ENUMS ---------------
typedef enum
{
  E_SPI_COMM_NOOPERA = 0x00, // No operation
  E_SPI_COMM_NOTE = 0x01,    // Note info
  E_SPI_ALL_OFF = 0x02       // Turn all solenoid off
} E_SPI_COMM;

//-------------- STRUCTS / UNION ---------------

typedef struct
{
  uint8_t midi;      // 0 - 88, physical notes of the piano
  uint16_t duration; // [ms]
  uint32_t time;     // [ms]
  uint8_t vel;       // Velocity [0-255]
} S_NOTE;

typedef struct
{
  uint16_t ticksPerBeat; // e.g., 480
  uint16_t tempo;        // BPM
} S_TRACK_INFO;

// Define a frame structure
typedef struct __attribute__((packed))
{
  E_SPI_COMM command : 8;
  S_NOTE note;
} S_FRAME;

typedef union
{
  S_FRAME bits;
  uint8_t bytes[D_SPI_BUFFSIZE];
} U_FRAME;

//-------------- PROTOTYPES---------------
void i2c_scan();
void init_spi();

// Two boards with different I2C addresses
Adafruit_PWMServoDriver g_pwm1 = Adafruit_PWMServoDriver(0x40);
// Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(0x41);

//-------------- VARIABLES ---------------
U_FRAME g_spi_buf_rx;             // SPI receive buffer
volatile byte g_spi_buf_index;    // Index of SPI buffer
volatile boolean g_spi_msg_ready; // Message ready flag

void setup()
{
  Serial.begin(D_SERIAL_BAUD);
  init_spi();
  g_pwm1.begin();
  // Set SCL speed
  Wire.setClock(D_I2C_SPEED);
  // set totem pole output
  g_pwm1.setOutputMode(true);

  g_pwm1.setPWMFreq(D_PWM_FREQ);

  for (uint8_t i = 0; i < 16; i++)
  {
    g_pwm1.setPWM(i, 0, 0);
  }
}

//-------------- MAIN LOOP ---------------

void loop()
{
  // Reset buffer index and message flag
  if (g_spi_msg_ready)
  {
    g_spi_buf_index = 0;
    g_spi_msg_ready = false;

    switch (g_spi_buf_rx.bits.command)
    {
    case E_SPI_ALL_OFF:
    {
      break;
    }
    case E_SPI_COMM_NOTE:
    {
      Serial.println(g_spi_buf_rx.bits.note.midi);
      break;
    }
    }
    delay(100); // wait a second to see output
  }
}

//-------------- INTERRUPTS ---------------
ISR(SPI_STC_vect)
{
  byte c = SPDR; // Read incoming SPI byte

  if (g_spi_buf_index < D_SPI_BUFFSIZE)
  {
    // Store the current value in a buffer (not the last dummy)
    g_spi_buf_rx.bytes[g_spi_buf_index] = c;
    // Sends the corresponding tx (DUMMY)
    SPDR = 0x00;
    g_spi_buf_index++;
  }
  else if (g_spi_buf_index == D_SPI_BUFFSIZE)
  { // Last dummy byte received
    g_spi_msg_ready = true;
  }
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

/*
 * Init SPI module
 */
void init_spi()
{
  pinMode(MISO, OUTPUT);
  pinMode(MOSI, INPUT);
  pinMode(SCK, INPUT);
  pinMode(SS, INPUT);

  SPCR |= _BV(SPE);  // Enable SPI
  SPCR |= _BV(SPIE); // Enable SPI interrupt
}
