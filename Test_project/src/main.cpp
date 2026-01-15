/*
 * File:          esp32_c.ino
 * Author:        Ding Jérémy
 * Date:          2025-04-07
 *
 * Description:   Main control program for the Arachnova bot.
 *                This file handles SPI communication as a master to the arduino (slave) and
 *                sets up an ESP32-based WiFi access point. A built-in web interface
 *                allows users to remotely control the bot.
 *
 * Modifications:
 * Date:
 * Changes:
 */

//-------------- INCLUDES ---------------
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <SPI.h>
#include <Arduino_JSON.h>
#include <queue>

//-------------- DEFINES ---------------
#define D_UART_BAUDRATE 115200

// SPI pins
#define D_SPI_CLK 14
#define D_SPI_MISO 12
#define D_SPI_MOSI 13
#define D_SPI_SS 15
#define D_SPI_BAUDRATE 500000

// SPI buffer sizes
#define D_SPI_BUFFSIZE 5

// WiFi credentials
#define D_SSID "Self-playing-piano"
#define D_PASSWORD "1234"

// Notes buffer
#define NOTE_BUFFER 50

//-------------- ENUMS ---------------
typedef enum
{
    E_SPI_COMM_NOOPERA = 0x00, // No operation
    E_SPI_COMM_CTRLMOT = 0x01, // Control motors
    E_SPI_COMM_REQSBAT = 0x02, // Request battery status
    E_SPI_COMM_CTRLSRV = 0x03, // Control servos
    E_SPI_COMM_CALIBRT = 0x04, // Calibrate steppers
    E_SPI_COMM_DEMO = 0x05,    // Activate demo mode
    E_SPI_COMM_CONNECT = 0x06, // Indicates user presence
    E_SPI_COMM_COLOR = 0x07    // Update the robot's eyes color
} E_SPI_COMM;

//-------------- ENUMS ---------------

// Define a frame structure
typedef struct __attribute__((packed))
{
    E_SPI_COMM command : 8;
    int16_t data_1 : 16;
    int16_t data_2 : 16;
} S_FRAME;

typedef union
{
    S_FRAME bits;
    uint8_t bytes[D_SPI_BUFFSIZE];
} U_FRAME;

typedef struct
{
    uint8_t note;      // 0 - 88, physical notes of the piano
    uint16_t duration; // Ticks
    uint16_t time;     // Ticks
    uint8_t vel;       // Velocity
} S_NOTE;

typedef struct
{
    uint16_t ticksPerBeat; // e.g., 480
    uint16_t tempo;        // BPM
} S_TRACK_INFO;

//-------------- FUNCTION PROTOTYPES ---------------
void init_littlefs();
void init_wifi();
void init_spi();

void notify_clients(String msg);
void handle_websocket_message(void *arg, uint8_t *data, size_t len);
void on_event(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void init_websocket();

U_FRAME send_frame(U_FRAME tx_frame);

//-------------- GLOBAL VARIABLES ---------------
AsyncWebServer server(80); // Web server
AsyncWebSocket ws("/ws");  // WebSocket endpoint

// Note queue
std::queue<S_NOTE> note_queue;

//-------------- SETUP FUNCTION ---------------
void setup()
{
    Serial.begin(D_UART_BAUDRATE); // Open serial port
    init_spi();
    init_wifi();
    init_littlefs();
    init_websocket();

    // Serve root HTML
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html"); });

    // Serve static files
    server.serveStatic("/", LittleFS, "/");

    server.begin();
}

//-------------- MAIN LOOP ---------------
void loop()
{
}

//-------------- FUNCTION IMPLEMENTATIONS ---------------

/*
 * Sends a 5-byte frame to the SPI slave device, reads the response
 */
U_FRAME send_frame(U_FRAME tx_frame)
{
    U_FRAME rx_frame;
    SPI.beginTransaction(SPISettings(D_SPI_BAUDRATE, MSBFIRST, SPI_MODE0));
    digitalWrite(D_SPI_SS, LOW);
    delay(1);

    // First received byte is trash
    SPI.transfer(tx_frame.bytes[0]);
    for (int i = 1; i < D_SPI_BUFFSIZE; i++)
    {
        rx_frame.bytes[i - 1] = SPI.transfer(tx_frame.bytes[i]);
    }
    // Send last dummy byte to receive the last data info
    rx_frame.bytes[D_SPI_BUFFSIZE - 1] = SPI.transfer(0xFF);

    delay(1);
    digitalWrite(D_SPI_SS, HIGH);
    SPI.endTransaction();

    return rx_frame;
}
/*
 * Initializes the LittleFS file system
 */
void init_littlefs()
{
    if (!LittleFS.begin(true))
    {
        Serial.println("An error has occurred while mounting LittleFS");
    }
    else
    {
        Serial.println("LittleFS mounted successfully");
    }
}

/*
 * Initializes the ESP32 in Access Point mode
 */
void init_wifi()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(D_SSID, D_PASSWORD);
    Serial.println("Starting Access Point...");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
}

/*
 * Sends a text message to all connected WebSocket clients
 */
void notify_clients(String msg)
{
    ws.textAll(msg);
}

/*
 * Handles incoming WebSocket messages from the client
 */
void handle_websocket_message(void *arg, uint8_t *data, size_t len)
{
    // Get frame infos
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    // Check if the received websocket message is valid
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        // Parse Data and Message.
        String message = (char *)data;
        JSONVar obj = JSON.parse(message);
        // Read ws message
        if (obj.hasOwnProperty("type"))
        {
            String type = (const char *)obj["type"];

            if (type == "trackInfo")
            {
                S_TRACK_INFO info;
                info.ticksPerBeat = (uint16_t)obj["data"]["ticksPerBeat"];
                info.tempo = (uint16_t)obj["data"]["tempo"];
            }
            else if (type == "noteChunk")
            {
                JSONVar notesArray = obj["data"];
                for (int i = 0; i < notesArray.length(); i++)
                {
                    S_NOTE n;
                    n.note = (uint8_t)notesArray[i]["note"];
                    n.duration = (uint16_t)notesArray[i]["duration"];
                    n.time = (uint16_t)notesArray[i]["time"];
                    n.vel = (uint8_t)notesArray[i]["velocity"];
                    note_queue.push(n);
                }
            }
        }
    }
}

/*
 * WebSocket event callback handler
 */
void on_event(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
        handle_websocket_message(arg, data, len);
        break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
        break;
    }
}

/*
 * Initializes the WebSocket server and binds events
 */
void init_websocket()
{
    ws.onEvent(on_event);
    server.addHandler(&ws);
}

/*
 * Initializes the SPI interface with predefined pins
 */
void init_spi()
{
    pinMode(D_SPI_SS, OUTPUT);
    digitalWrite(D_SPI_SS, HIGH);
    SPI.begin(D_SPI_CLK, D_SPI_MISO, D_SPI_MOSI, D_SPI_SS);
}
