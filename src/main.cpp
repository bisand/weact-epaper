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
#define ENABLE_GxEPD2_GFX 0

// include the libraries
#include <ESP8266WiFi.h>
#include <RadioLib.h>
#include <time.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include <GxEPD2_BW.h>
// #include <Fonts/FreeSansBold24pt7b.h>
#include <../fonts/RobotoCondensed_Bold48pt7b.h>
#include <../fonts/Roboto_Regular10pt7b.h>

#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS D4

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// ESP32 CS(SS)=5,SCL(SCK)=18,SDA(MOSI)=23,RES(RST)=2,DC=0
// ESP8266 BUSY=12,RES(RST)=2,DC=1,CS(SS)=15,SCL(SCK)=14,SDA(MOSI)=13
#define SCR_CS 15
#define SCR_RES 2
#define SCR_DC 1
#define SCR_BUSY 12

// 2.13'' EPD Module
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(SCR_CS, SCR_DC, SCR_RES, SCR_BUSY)); // DEPG0213BN 122x250, SSD1680

#define LORA_CS D8
#define LORA_DIO1 D1
#define LORA_RST D3
#define LORA_BUSY D2

SX1262 lora = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

// flag to indicate that a packet was received
volatile bool receivedFlag = false;
volatile bool enableInterrupt = true;

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
#if defined(ESP8266) || defined(ESP32)
IRAM_ATTR
#endif
void setFlag(void)
{
    if (!enableInterrupt)
    {
        return;
    }
    // we got a packet, set the flag
    receivedFlag = true;
}

uint8_t rfPower = 22;
const char delimiter = '|';

struct LoRaMessage
{
    uint64_t sensorId;
    uint32_t messageId;
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
    uint64_t sensorId;
    uint32_t messageId;
    time_t epochTime;
};

// RtcData rtcData;
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

void WriteEeprom()
{
    // Write struct to EEPROM
    for (size_t i = 0; i < sizeof(sensorData); i++)
    {
        EEPROM.write(i, ((uint8_t *)&sensorData)[i]);
    }
}

void ReadEeprom()
{
    // Read struct from EEPROM
    for (size_t i = 0; i < sizeof(sensorData); i++)
    {
        ((uint8_t *)&sensorData)[i] = EEPROM.read(i);
    }
}

void writeMemory()
{
    // Update CRC32 of data
    // sensorData.crc32 = calculateCRC32((uint8_t *)&sensorData.data[0], sizeof(sensorData.data));
    // Write struct to RTC memory
    if (!ESP.rtcUserMemoryWrite(0, (uint32_t *)&sensorData, sizeof(sensorData)))
    {
        Serial.println("Error writing to RTC memory");
    }
}

void readMemory()
{
    if (ESP.rtcUserMemoryRead(0, (uint32_t *)&sensorData, sizeof(sensorData)))
    {
        // uint32_t crcOfData = calculateCRC32((uint8_t *)&sensorData.data[0], sizeof(sensorData.data));
        // if (crcOfData != sensorData.crc32)
        // {
        //     Serial.println("CRC32 in RTC memory doesn't match CRC32 of data. Data is probably invalid!");
        // }
        // else
        // {
        //     Serial.println("CRC32 check ok, data is probably valid.");
        // }
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

// void writeSensorDataToRtc(const SensorData &sensorData, RtcData &rtcData)
// {
//     // Convert sensorId, messageId, and lastTicks to char array
//     sprintf(rtcData.data, "%llu,%u,%llu", sensorData.sensorId, sensorData.messageId, sensorData.epochTime);
//     Serial.print("Writing to RTC memory: ");
//     Serial.println(rtcData.data);
// }

// void readSensorDataFromRtc(const RtcData &rtcData, SensorData &sensorData)
// {
//     // Parse the char array data and assign values to sensorId, messageId, and lastTicks
//     Serial.print("Reading from RTC memory: ");
//     Serial.println(rtcData.data);
//     sscanf(rtcData.data, "%llu,%u,%llu", &sensorData.sensorId, &sensorData.messageId, &sensorData.epochTime);
// }

uint64_t getChipId()
{
    uint8_t baseMac[6];
    WiFi.macAddress(baseMac);
    uint64_t chipId = 0;
    for (int i = 0; i < 6; i++)
    {
        chipId |= (uint64_t)(baseMac[i]) << ((5 - i) * 8);
    }
    return chipId;
}

void display_temp(time_t time, float temp)
{
    char info_text[26];
    sprintf(info_text, "Siste: %s", asctime(localtime(&time)));
    // const char *info_text = "Siste: 2024-08-09 17:18:42";
    char temp_text[10];                 // Make sure this is large enough to hold the formatted string
    sprintf(temp_text, "%.1f C", temp); // Convert float to string with 1 decimal places

    // Replace the decimal point with a comma
    char *comma_pos = strchr(temp_text, '.');
    if (comma_pos)
    {
        *comma_pos = ','; // Replace '.' with ','
    }

    display.setRotation(1);
    display.setTextColor(GxEPD_BLACK);

    display.setFont(&RobotoCondensed_Bold48pt7b);
    int16_t tb1_x, tb1_y;
    uint16_t tb1_w, tb1_h;
    display.getTextBounds(temp_text, 0, 0, &tb1_x, &tb1_y, &tb1_w, &tb1_h);
    // center the bounding box by transposition of the origin:
    uint16_t x1 = ((display.width() - tb1_w) / 2) - tb1_x;
    uint16_t y1 = tb1_h - 15;

    display.setFont(&Roboto_Regular10pt7b);
    int16_t tb2_x, tb2_y;
    uint16_t tb2_w, tb2_h;
    display.getTextBounds(info_text, 0, 0, &tb2_x, &tb2_y, &tb2_w, &tb2_h);
    // center the bounding box by transposition of the origin:
    uint16_t x2 = ((display.width() - tb2_w) / 2) - tb2_x;
    uint16_t y2 = display.height() - 5;

    display.setFullWindow();
    display.firstPage();
    do
    {
        display.fillScreen(GxEPD_WHITE);
        // display.writeFastHLine(0, 0, display.width(), GxEPD_BLACK);
        // display.writeFastHLine(0, display.height() - 1, display.width(), GxEPD_BLACK);
        // display.writeFastVLine(0, 0, display.height(), GxEPD_BLACK);
        // display.writeFastVLine(display.width() - 1, 0, display.height(), GxEPD_BLACK);
        display.setCursor(x1, y1);
        display.setFont(&RobotoCondensed_Bold48pt7b);
        display.print(temp_text);
        display.setFont(&Roboto_Regular10pt7b);
        display.setCursor(x2, y2);
        display.print(info_text);
    } while (display.nextPage());
}

void initRF()
{
    // initialize the second LoRa instance with
    // non-default settings
    // this LoRa link will have high data rate,
    // but lower range
    Serial.print(F("[SX1262] Initializing ... "));
    // carrier frequency:           868.0 MHz
    float freq = 868.0;
    // bandwidth:                   125.0 kHz
    float bw = 62.5;
    // spreading factor:            10
    uint8_t sf = 10;
    // coding rate:                 5
    uint8_t cr = 5;
    // sync word:                   0x34 (public network/LoRaWAN), 0x24 (private)
    uint8_t syncWord = 0x24;
    // output power:                2 dBm
    int8_t power = 22;
    // preamble length:             20 symbols
    uint16_t preambleLength = 20;

    int state = lora.begin(freq, bw, sf, cr, syncWord, power, preambleLength);
    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(F("success!"));
    }
    else
    {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true)
            ;
    }

    if (lora.setTCXO(2.4) == RADIOLIB_ERR_INVALID_TCXO_VOLTAGE)
    {
        Serial.println(F("Selected TCXO voltage is invalid for this module!"));
    }

    // set the function that will be called
    // when new packet is received
    lora.setPacketReceivedAction(setFlag);

    // start listening for LoRa packets
    Serial.print(F("[SX1262] Starting to listen ... "));
    state = lora.startReceive();
    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(F("success!"));
    }
    else
    {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true)
        {
            delay(10);
        }
    }
}

float getTemperature()
{
    // Send the command to get temperatures
    Serial.print("Requesting temperatures...");
    sensors.requestTemperatures();
    Serial.println("DONE");
    // Get the temperature in Celsius
    float tempC = sensors.getTempCByIndex(0);
    Serial.print("Temperature for the device 1 (index 0) is: ");
    Serial.println(tempC);
    return tempC;
}

void setup()
{
    // initialize the serial port
    Serial.begin(9600);
    while (!Serial)
        delay(10); // wait for Serial to be initialized

    pinMode(RX, OUTPUT);
    digitalWrite(RX, HIGH);

    // Read struct from RTC memory
    readMemory();
    // readSensorDataFromRtc(sensorData, sensorData);
    Serial.println("Data stored in RTC memory: ");
    Serial.print("Sensor ID: ");
    Serial.println(sensorData.sensorId);
    Serial.print("Message ID: ");
    Serial.println(sensorData.messageId);
    Serial.print("Datetime: ");
    Serial.println(sensorData.epochTime);

    // Initialize the sensor ID if it's not set
    uint64_t chipId = getChipId();
    if (sensorData.sensorId != chipId)
    {
        Serial.println("Sensor ID not set, initializing...");
        Serial.print("Chip ID: ");
        Serial.println(chipId);
        Serial.print("Chip ID (HEX): ");
        Serial.println(chipId, HEX);
        sensorData.sensorId = chipId;
        sensorData.messageId = 0;
        sensorData.epochTime = 0;
        // writeSensorDataToRtc(sensorData, sensorData);
        writeMemory();
    }

    delay(1000);
    initRF();
    // Start up the library
    sensors.begin();
}

int count = 0;

void loop()
{
    bool ackReceived = false;
    sensorData.messageId++;

    loraMessage.sensorId = sensorData.sensorId;
    loraMessage.messageId = sensorData.messageId;
    loraMessage.epochTime = sensorData.epochTime;
    loraMessage.cmd = 0x00;
    loraMessage.temperature = getTemperature(); //(float)random(0, 2500) / 100;

    // Loop to attempt transmission up to maxRetransmissions times
    for (int attempt = 0; attempt < 5; attempt++)
    {
        unsigned long randomDelay = random(500, 5000);

        // Send the message
        uint8_t *dataPtr = (uint8_t *)&loraMessage;
        int16_t state = lora.transmit(dataPtr, sizeof(LoRaMessage));

        if (state == RADIOLIB_ERR_NONE)
        {
            Serial.println("Message sent successfully, waiting for ACK...");

            // Switch to receive mode and wait for acknowledgment
            // Read the incoming message
            unsigned long start = millis();

            int16_t receiveState = -1;
            Serial.println(F("Waiting for ACK..."));
            do
            {
                receiveState = lora.receive((uint8_t *)&loraMessage, sizeof(LoRaMessage));
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
                Serial.println(loraMessage.sensorId);
                Serial.print("Message ID: ");
                Serial.println(loraMessage.messageId);
                Serial.print("Command: ");
                Serial.println(loraMessage.cmd);
                Serial.print("Payload: ");
                Serial.println(loraMessage.temperature);

                // Check if the received ACK matches this sensor and message
                if (loraMessage.sensorId == sensorData.sensorId && loraMessage.messageId == sensorData.messageId && loraMessage.cmd == 0xFF)
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
        else
        {
            Serial.print("Error transmitting message, code: ");
            Serial.println(state);
        }

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
        // writeSensorDataToRtc(sensorData, sensorData);
        writeMemory();
    }

    // display.init(115200, true, 50, false);
    // display_temp(sensorData.epochTime, loraMessage.temperature);
    // display.hibernate();

    Serial.println("Going to sleep for 1 hour...");
    Serial.flush();

    digitalWrite(RX, LOW);

    // Enter deep sleep for one hour
    // ESP.deepSleep(3600e6, RF_DISABLED); // Sleep for 1 hour
    ESP.deepSleep(60e6, RF_DISABLED); // Sleep for 1 minute
}
