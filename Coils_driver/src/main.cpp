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

#define D_SPI_BUFFSIZE 10

#define D_NOTES_BUFFER_SIZE 20 // Notes to be stored

#define D_PHASE_ACC_DURATION 20 // Solenoid acceleration [ms]

//-------------- ENUMS ---------------
typedef enum
{
  E_SPI_COMM_NOOPERA = 0x00, // No operation
  E_SPI_COMM_NOTE = 0x01,    // Note info
  E_SPI_COMM_ALL_OFF = 0x02, // Turn all solenoid off
  E_SPI_COMM_START = 0x03,   // Start playing the notes, (reset timer)
  E_SPI_COMM_PAUSE = 0x04,   // Pause the music
  E_SPI_COMM_RESUME = 0x05   // Resume the music
} E_SPI_COMM;

//-------------- STRUCTS / UNION ---------------

typedef struct
{
  uint8_t midi;      // 0 - 88, physical notes of the piano
  uint8_t on;        // 1 = On, 0 = off
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
void all_off();
void remove_first_note();
// Two boards with different I2C addresses
Adafruit_PWMServoDriver g_pwm1 = Adafruit_PWMServoDriver(0x40);
// Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(0x41);

//-------------- VARIABLES ---------------
U_FRAME g_spi_buf_rx;                       // SPI receive buffer
volatile byte g_spi_buf_index;              // Index of SPI buffer
volatile boolean g_spi_msg_ready;           // Message ready flag
S_NOTE g_notes_buffer[D_NOTES_BUFFER_SIZE]; // Note buffer
uint8_t g_notes_buffer_index = 0;           // Note index (last note to play)

uint32_t g_current_time = 0; // Time to play midi
uint32_t g_resume_time = 0;  // Time of resuming
uint32_t g_midi_time = 0;    // Midi time (ms)
bool g_playing = false;      // Playing flag

void setup()
{
  Serial.begin(D_SERIAL_BAUD);
  init_spi();
  pinMode(D_OE_PIN, OUTPUT);
  digitalWrite(D_OE_PIN, 0);

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
  // Treat incoming spi message
  if (g_spi_msg_ready)
  {
    g_spi_buf_index = 0;
    g_spi_msg_ready = false;

    switch (g_spi_buf_rx.bits.command)
    {
    case E_SPI_COMM_NOOPERA:
      break;

    case E_SPI_COMM_ALL_OFF:
      all_off();
      break;
    case E_SPI_COMM_PAUSE:
      if (g_playing)
      {
        Serial.println("paused");
        g_playing = false;
      }
      break;

    case E_SPI_COMM_RESUME:
      // Resume command, comes with the current midi time (sent by server)
      if (!g_playing)
      {
        g_playing = true;
        Serial.println("playing");
        g_resume_time = millis();                  // Save current time of resumal
        g_midi_time = g_spi_buf_rx.bits.note.time; // Get midi time at resumal
      }
      break;

    case E_SPI_COMM_NOTE:
      g_notes_buffer[g_notes_buffer_index] = g_spi_buf_rx.bits.note;
      g_notes_buffer_index++;
      Serial.println(g_notes_buffer[g_notes_buffer_index].midi);
      Serial.println(g_notes_buffer[g_notes_buffer_index].on);
      if (g_notes_buffer_index >= D_NOTES_BUFFER_SIZE)
      {
        Serial.println("Note buffer overflow");
        g_notes_buffer_index = 0;
      }
      break;

    default:
      break;
    }
  }
  // Run solenoids if playing
  if (g_playing)
  {
    // Get current time
    g_current_time = millis() - g_resume_time + g_midi_time;

    // Check for solenoids to start in the futur buffer
    while (g_notes_buffer_index > 0)
    {
      S_NOTE &note = g_notes_buffer[0];

      if (note.time > g_current_time)
        break; // earliest note is still in the future

      // --- Execute note ---
      if (note.midi > 0 && note.midi < 16)
      {
        if (note.on)
        {
          g_pwm1.setPWM(note.midi, 0, 4095);
        }
        else
        {
          g_pwm1.setPWM(note.midi, 0, 0);
        }
      }
      // --- Remove note from buffer ---
      remove_first_note();
    }
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

//-------------- FUNCTIONS ---------------

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

/*
 * Turns all solenoids off
 */
void all_off()
{
}

/*
 * Remove the first note of the note buffer (shifting all others)
 */
void remove_first_note()
{
  // Shift all remaining notes left by one
  for (uint8_t i = 1; i < g_notes_buffer_index; i++)
  {
    g_notes_buffer[i - 1] = g_notes_buffer[i];
  }

  g_notes_buffer_index--;
}