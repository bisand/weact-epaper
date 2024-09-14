/*
  RadioLib SX126x Blocking Transmit Example

  This example transmits packets using SX1262 LoRa radio module.
  Each packet contains up to 256 bytes of data, in the form of:
  - Arduino String
  - null-terminated char array (C-string)
  - arbitrary binary data (byte array)

  Other modules from SX126x family can also be used.

  Using blocking transmit is not recommended, as it will lead
  to inefficient use of processor time!
  Instead, interrupt transmit is recommended.

  For default module settings, see the wiki page
  https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx126x---lora-modem

  For full API reference, see the GitHub Pages
  https://jgromes.github.io/RadioLib/
*/

// include the libraries
#include <LoraSx1262.h>
#include <time.h>
#include <EEPROM.h>
#include <TimeLib.h>
#include "LowPower.h"
#include <ArduinoUniqueID.h>
#include <SPI.h>
#include "1_54in_epaper.h"
#include <stdlib.h>

#define LORA_DIO1 9
#define LORA_NSS SS
#define LORA_RESET 8

LoraSx1262 radio(LORA_DIO1, LORA_NSS, LORA_RESET);

#define RST_PIN 5
#define DC_PIN 6
#define CS_PIN 7
#define BUSY_PIN 4

Epd epd(RST_PIN, DC_PIN, CS_PIN, BUSY_PIN); // initiate e-paper display [epd]
unsigned char image[1024];                  // memory for display
Paint paint(image, 0, 0);                   // setup for text display

#define COLORED 0   // background color handler (dark)
#define UNCOLORED 1 // background color handler (light)

int iter_val = 0; // storage variable for serial data

int initial_space = 10; // initial white/dark space at the top of the display
int row_line = 3;       // counting rows to avoid overlap
int row_height = 24;    // row height (based on text size)

uint8_t rfPower = 22;
const char delimiter = '|';

struct LoRaMessage
{
    uint32_t sensorID;
    uint32_t messageID;
    time_t epochTime;
    byte cmd;
    float temperature;
};

LoRaMessage loraMessage;

// RTC memory structure
struct RtcData
{
    uint32_t crc32 = 0;
    char data[200];
};

struct SensorData
{
    uint8_t sensorId;
    uint32_t messageId;
    time_t epochTime;
};

RtcData rtcData;
SensorData sensorData;

uint32_t calculateCRC32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xffffffff;
    while (length--)
    {
        uint8_t c = *data++;
        for (uint32_t i = 0x80; i > 0; i >>= 1)
        {
            bool bit = crc & 0x80000000;
            if (c & i)
            {
                bit = !bit;
            }
            crc <<= 1;
            if (bit)
            {
                crc ^= 0x04c11db7;
            }
        }
    }
    return crc;
}

void printMemory()
{
    char buf[3];
    uint8_t *ptr = (uint8_t *)&rtcData;
    for (size_t i = 0; i < sizeof(rtcData); i++)
    {
        sprintf(buf, "%02X", ptr[i]);
        Serial.print(buf);
        if ((i + 1) % 32 == 0)
        {
            Serial.println();
        }
        else
        {
            Serial.print(" ");
        }
    }
    Serial.println();
}

void writeMemory()
{
    // Update CRC32 of data
    rtcData.crc32 = calculateCRC32((uint8_t *)&rtcData.data[0], sizeof(rtcData.data));
    // Write struct to RTC memory
    // if (!ESP.rtcUserMemoryWrite(0, (uint32_t *)&rtcData, sizeof(rtcData)))
    // {
    //     Serial.println("Error writing to RTC memory");
    // }
    // Write struct to EEPROM
    for (size_t i = 0; i < sizeof(rtcData); i++)
    {
        EEPROM.write(i, ((uint8_t *)&rtcData)[i]);
    }
}

void readMemory()
{
    // if (ESP.rtcUserMemoryRead(0, (uint32_t *)&rtcData, sizeof(rtcData)))
    // {
    //     uint32_t crcOfData = calculateCRC32((uint8_t *)&rtcData.data[0], sizeof(rtcData.data));
    //     if (crcOfData != rtcData.crc32)
    //     {
    //         Serial.println("CRC32 in RTC memory doesn't match CRC32 of data. Data is probably invalid!");
    //     }
    //     else
    //     {
    //         Serial.println("CRC32 check ok, data is probably valid.");
    //     }
    // }

    // Read struct from EEPROM
    for (size_t i = 0; i < sizeof(rtcData); i++)
    {
        ((uint8_t *)&rtcData)[i] = EEPROM.read(i);
    }
} // counter to keep track of transmitted packets

void convertToLocalTime(const char *utcDatetime, char *localDatetime, size_t size, int timeZoneOffset)
{
    // Parse the input UTC datetime string (format: "YYYY-MM-DD HH:MM:SS")
    int inputYear, inputMonth, inputDay, inputHour, inputMinute, inputSecond;
    sscanf(utcDatetime, "%d-%d-%d %d:%d:%d", &inputYear, &inputMonth, &inputDay, &inputHour, &inputMinute, &inputSecond);

    // Create a tm structure
    tmElements_t tm;
    tm.Year = inputYear - 1970; // TimeLib uses years since 1970
    tm.Month = inputMonth;
    tm.Day = inputDay;
    tm.Hour = inputHour;
    tm.Minute = inputMinute;
    tm.Second = inputSecond;

    // Convert to time_t
    time_t utcTime = makeTime(tm);

    // Adjust for the time zone offset
    time_t localTime = utcTime + timeZoneOffset * SECS_PER_HOUR;

    // Format the local time back into a string
    snprintf(localDatetime, size, "%04d-%02d-%02d %02d:%02d:%02d",
             year(localTime), month(localTime), day(localTime),
             hour(localTime), minute(localTime), second(localTime));
}

void writeSensorDataToRtc(const SensorData &sensorData, RtcData &rtcData)
{
    // Convert sensorId, messageId, and lastTicks to char array
    sprintf(rtcData.data, "%u,%lu,%lu", sensorData.sensorId, sensorData.messageId, sensorData.epochTime);
    Serial.print("Writing to RTC memory: ");
    Serial.println(rtcData.data);
}

void readSensorDataFromRtc(const RtcData &rtcData, SensorData &sensorData)
{
    // Parse the char array data and assign values to sensorId, messageId, and lastTicks
    Serial.print("Reading from RTC memory: ");
    Serial.println(rtcData.data);
    sscanf(rtcData.data, "%u,%lu,%lu", &sensorData.sensorId, &sensorData.messageId, &sensorData.epochTime);
}

void initRF()
{
    // initialize the second LoRa instance with
    // non-default settings
    // this LoRa link will have high data rate,
    // but lower range
    Serial.println(F("[SX1268] Initializing ... "));
    // carrier frequency:           868.0 MHz
    float freq = 868.0;
    // bandwidth:                   125.0 kHz
    float bw = 125.0 / 2;
    // spreading factor:            10
    uint8_t sf = 10;
    // coding rate:                 5
    uint8_t cr = 5;
    // sync word:                   0x34 (public network/LoRaWAN)
    uint16_t syncWord = 0x24;
    // output power:                2 dBm
    int8_t power = 22;
    // preamble length:             20 symbols
    uint16_t preambleLength = 20;

    if (!radio.begin())
    { // Initialize radio
        Serial.println("Failed to initialize radio.");
    }

    // FREQUENCY - Set frequency to 902Mhz (default 915Mhz)
    radio.configSetFrequency(866000000); // Freq in Hz. Must comply with your local radio regulations

    // BANDWIDTH - Set bandwidth to 250khz (default 500khz)
    radio.configSetBandwidth(0x03); // 0=7.81khz, 5=200khz, 6=500khz. See documentation for more

    // CODING RATE - Set the coding rate to CR_4_6
    radio.configSetCodingRate(5); // 1-4 = coding rate CR_4_5, CR_4_6, CR_4_7, and CR_4_8 respectively

    // SPREADING FACTOR - Set the spreading factor to SF12.  (default is SF7)
    radio.configSetSpreadingFactor(12); // 5-12 are valid ranges.  5 is fast and short range, 12 is slow and long range

    radio.configSetSyncWord(syncWord); // Set the sync word to 0x34 (public network/LoRaWAN)

    // radio.configSetPreset(PRESET_LONGRANGE);
    // int16_t state = lora.begin(freq, bw, sf, cr, syncWord, power, preambleLength);
    // if (state == RADIOLIB_ERR_NONE)
    // {
    //     Serial.println(F("success!"));
    // }
    // else
    // {
    //     Serial.print(F("failed, code "));
    //     Serial.println(state);
    //     while (true)
    //         ;
    // }

    // if (lora.setTCXO(2.4) == RADIOLIB_ERR_INVALID_TCXO_VOLTAGE)
    // {
    //     Serial.println(F("Selected TCXO voltage is invalid for this module!"));
    // }

    // radio.setOutputPower(rfPower);
    Serial.println(F("Radio initialized."));
}

void header_print()
{
    paint.SetWidth(200); // set display width
    paint.SetHeight(24); // set initial vertical space

    paint.Clear(COLORED);                                            // darkr background
    paint.DrawStringAt(0, 4, "1.54in e-Paper!", &Font20, UNCOLORED); // light text
    epd.SetFrameMemory(paint.GetImage(), 0, initial_space + (0 * row_height), paint.GetWidth(), paint.GetHeight());

    paint.Clear(UNCOLORED);                                    // light background
    paint.DrawStringAt(0, 4, "Simple Demo", &Font20, COLORED); // dark text
    epd.SetFrameMemory(paint.GetImage(), 0, initial_space + (1 * row_height), paint.GetWidth(), paint.GetHeight());

    paint.Clear(COLORED);                                         // dark background
    paint.DrawStringAt(0, 4, "Maker Portal", &Font20, UNCOLORED); // light text
    epd.SetFrameMemory(paint.GetImage(), 0, initial_space + (2 * row_height), paint.GetWidth(), paint.GetHeight());
}

void setup()
{
    Serial.begin(9600);
    while (!Serial)
        ; // wait for Serial to be initialized

    // Read struct from RTC memory
    readMemory();
    readSensorDataFromRtc(rtcData, sensorData);
    Serial.println("Data stored in RTC memory: ");
    Serial.print("Sensor ID: ");
    Serial.println(sensorData.sensorId);
    Serial.print("Message ID: ");
    Serial.println(sensorData.messageId);
    Serial.print("Datetime: ");
    Serial.println(sensorData.epochTime);

    // Initialize the sensor ID if it's not set
    UniqueID8;
    if (sensorData.sensorId != UniqueID8)
    {
        sensorData.sensorId = UniqueID8;
        sensorData.messageId = 0;
        sensorData.epochTime = 0;
        writeSensorDataToRtc(sensorData, rtcData);
        writeMemory();
    }

    initRF();

    Serial.println("Initializing e-paper display...");
    epd.LDirInit();                  // initialize epaper
    Serial.println("Display initialized.");
    Serial.println("Clearing display...");
    epd.Clear();                     // clear old text/imagery
    Serial.println("Display cleared.");
    Serial.println("Displaying base image...");
    epd.DisplayPartBaseWhiteImage(); // lay a base white layer down first
    Serial.println("Base image displayed.");
}

int count = 0;

void loop()
{
    String str_to_print = "Val: ";    // string prepend
    str_to_print += String(iter_val); // add integer value

    header_print(); // print header text

    paint.Clear(UNCOLORED);                                           // clear background
    paint.DrawStringAt(0, 4, str_to_print.c_str(), &Font20, COLORED); // light background
    epd.SetFrameMemory(paint.GetImage(), 0, initial_space + (3 * row_height), paint.GetWidth(), paint.GetHeight());
    epd.DisplayPartFrame(); // display new text
    iter_val += 1;          // increase integer value

    bool ackReceived = false;
    sensorData.messageId++;
    // writeSensorDataToRtc(sensorData, rtcData);

    // Create the message payload including the sensor ID and message ID
    String message = String(sensorData.sensorId) + "," + String(sensorData.messageId) + "," + String(sensorData.epochTime) + ",Hello, World!";

    loraMessage.sensorID = sensorData.sensorId;
    loraMessage.messageID = sensorData.messageId;
    loraMessage.epochTime = sensorData.epochTime;
    loraMessage.cmd = 0x00;
    loraMessage.temperature = (float)random(0, 2500) / 100;

    // Loop to attempt transmission up to maxRetransmissions times
    for (int attempt = 0; attempt < 5; attempt++)
    {
        unsigned long randomDelay = random(500, 5000);

        // Send the message
        uint8_t *dataPtr = (uint8_t *)&loraMessage;
        // int16_t state = lora.transmit(dataPtr, sizeof(LoRaMessage));
        // int16_t state = lora.transmit(message);
        radio.transmit(dataPtr, sizeof(LoRaMessage));

        // if (state == RADIOLIB_ERR_NONE)
        {
            Serial.println("Message sent successfully, waiting for ACK...");

            // Switch to receive mode and wait for acknowledgment
            // Read the incoming message
            unsigned long start = millis();

            int16_t receiveState = -1;
            Serial.println(F("Waiting for ACK..."));
            do
            {
                int bytesRead = radio.lora_receive_async((uint8_t *)&loraMessage, sizeof(LoRaMessage));
                // receiveState = lora.receive((uint8_t *)&loraMessage, sizeof(LoRaMessage));
                // Serial.print("State: ");
                // Serial.println(receiveState);
                if (millis() - start > randomDelay)
                {
                    Serial.println("Timeout waiting for ACK.");
                    break;
                }
            } while (receiveState < 0);

            if (receiveState >= 0)
            {
                Serial.print("Received ACK: ");
                Serial.print("Sensor ID: ");
                Serial.println(loraMessage.sensorID);
                Serial.print("Message ID: ");
                Serial.println(loraMessage.messageID);
                Serial.print("Command: ");
                Serial.println(loraMessage.cmd);
                Serial.print("Payload: ");
                Serial.println(loraMessage.temperature);

                // Check if the received ACK matches this sensor and message
                if (loraMessage.sensorID == sensorData.sensorId && loraMessage.messageID == sensorData.messageId && loraMessage.cmd == 0xFF)
                {
                    Serial.println("Correct ACK received with datetime!");

                    // Store the received datetime and ticks in RTC memory
                    sensorData.epochTime = loraMessage.epochTime;

                    // Mark ACK as received and break loop
                    ackReceived = true;
                    break;
                }
                else
                {
                    Serial.println("Incorrect ACK received or ID mismatch.");
                }
            }
            else
            {
                Serial.print("Error receiving ACK, code: ");
                Serial.println(receiveState);
            }
        }
        // else
        // {
        //     Serial.print("Error transmitting message, code: ");
        //     Serial.println(state);
        // }

        // If ACK not received, wait for a random delay before retrying
        if (!ackReceived)
        {
            Serial.print("No ACK received, retrying...");
            delay(randomDelay);
        }
    }

    if (!ackReceived)
    {
        Serial.println("Failed to receive correct ACK after maximum retries.");
    }
    else
    {
        Serial.print("Stored datetime: ");
        Serial.println(sensorData.epochTime);
        Serial.print("Message ID: ");
        Serial.println(sensorData.messageId);
        // Write the updated sensor data to RTC memory
        writeSensorDataToRtc(sensorData, rtcData);
        writeMemory();
    }

    Serial.println("Going to sleep for 1 hour...");
    Serial.flush();

    // Enter deep sleep for one hour
    // ESP.deepSleep(3600e6, RF_DISABLED); // Sleep for 1 hour
    // ESP.deepSleep(60e6, RF_DISABLED); // Sleep for 1 minute
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}
