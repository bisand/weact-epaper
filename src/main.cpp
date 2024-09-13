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

// NodeMCU (ESP8266) <-> SX1262 LoRa Module
// D5 (GPIO14) <-> SCK
// D6 (GPIO12) <-> MISO
// D7 (GPIO13) <-> MOSI
// D8 (GPIO15) <-> NSS (Chip Select)
// D2 (GPIO4) <-> RST
// D3 (GPIO0) <-> DIO1
// D1 (GPIO5) <-> BUSY

// SX1262 has the following connections:
// NSS pin: 15 -> D8
// DIO1 pin: 0 -> D3
// NRST pin: 4 -> D2
// BUSY pin: 5 -> D1

#define LORA_CS PB2
#define LORA_DIO1 PB1
#define LORA_RST PB0
#define LORA_BUSY PD2

// SX1262 lora = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
LoraSx1262 radio;

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
    sprintf(rtcData.data, "%u,%u,%lld", sensorData.sensorId, sensorData.messageId, sensorData.epochTime);
    Serial.print("Writing to RTC memory: ");
    Serial.println(rtcData.data);
}

void readSensorDataFromRtc(const RtcData &rtcData, SensorData &sensorData)
{
    // Parse the char array data and assign values to sensorId, messageId, and lastTicks
    Serial.print("Reading from RTC memory: ");
    Serial.println(rtcData.data);
    sscanf(rtcData.data, "%u,%u,%lld", &sensorData.sensorId, &sensorData.messageId, &sensorData.epochTime);
}

void initRF()
{
    // initialize the second LoRa instance with
    // non-default settings
    // this LoRa link will have high data rate,
    // but lower range
    Serial.print(F("[SX1268] Initializing ... "));
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
    radio.configSetFrequency(868000000); // Freq in Hz. Must comply with your local radio regulations

    // BANDWIDTH - Set bandwidth to 250khz (default 500khz)
    radio.configSetBandwidth(0x01); // 0=7.81khz, 5=200khz, 6=500khz. See documentation for more

    // CODING RATE - Set the coding rate to CR_4_6
    radio.configSetCodingRate(2); // 1-4 = coding rate CR_4_5, CR_4_6, CR_4_7, and CR_4_8 respectively

    // SPREADING FACTOR - Set the spreading factor to SF12.  (default is SF7)
    radio.configSetSpreadingFactor(12); // 5-12 are valid ranges.  5 is fast and short range, 12 is slow and long range
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
}

int count = 0;

void loop()
{
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
            do
            {
                Serial.println(F("Waiting for ACK..."));
                int bytesRead = radio.lora_receive_async((uint8_t *)&loraMessage, sizeof(LoRaMessage));
                // receiveState = lora.receive((uint8_t *)&loraMessage, sizeof(LoRaMessage));
                Serial.print("State: ");
                Serial.println(receiveState);
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
