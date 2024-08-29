/*
   RadioLib SX126x Receive with Interrupts Example

   This example listens for LoRa transmissions and tries to
   receive them. Once a packet is received, an interrupt is
   triggered. To successfully receive data, the following
   settings have to be the same on both transmitter
   and receiver:
    - carrier frequency
    - bandwidth
    - spreading factor
    - coding rate
    - sync word

   Other modules from SX126x family can also be used.

   For default module settings, see the wiki page
   https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx126x---lora-modem

   For full API reference, see the GitHub Pages
   https://jgromes.github.io/RadioLib/
*/

// include the library
#include <RadioLib.h>

// NodeMCU (ESP8266) <-> SX1262 LoRa Module
// D5 (GPIO14) <-> SCK
// D6 (GPIO12) <-> MISO
// D7 (GPIO13) <-> MOSI
// D8 (GPIO15) <-> NSS (Chip Select)
// D2 (GPIO4) <-> RST
// D0 (GPIO16) <-> DIO1
// D1 (GPIO5) <-> BUSY

// SX1262 has the following connections:
// NSS pin: 15 -> D8
// DIO1 pin: 16 -> D0
// NRST pin: 4 -> D2
// BUSY pin: 5 -> D1
SX1262 radio = new Module(D8, D0, D2, D1);

// or using RadioShield
// https://github.com/jgromes/RadioShield
// SX1262 radio = RadioShield.ModuleA;

// or using CubeCell
// SX1262 radio = new Module(RADIOLIB_BUILTIN_MODULE);

float lora_sx1262_freq = 868.0F;
float lora_sx1262_bw = 31.25F;
uint8_t lora_sx1262_sf = 12U;
uint8_t lora_sx1262_cr = 5U;
uint8_t lora_sx1262_syncWord = 0x3444U;
int8_t lora_sx1262_power = 22;
uint16_t lora_sx1262_preambleLength = 12UL;
float lora_sx1262_tcxoVoltage = 1.8F;
bool lora_sx1262_useRegulatorLDO = false;

// flag to indicate that a packet was received
volatile bool receivedFlag = false;

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setFlag(void)
{
    // we got a packet, set the flag
    receivedFlag = true;
}

void setup()
{
    Serial.begin(9600);

    // initialize SX1262 with default settings
    Serial.print(F("[SX1262] Initializing ... "));

    int state = RADIOLIB_ERR_UNKNOWN;

    Serial.print(F("TXCO voltage: "));
    Serial.print(lora_sx1262_tcxoVoltage);
    Serial.println(F("V ... "));
    state = radio.begin(lora_sx1262_freq, lora_sx1262_bw, lora_sx1262_sf, lora_sx1262_cr, lora_sx1262_syncWord, lora_sx1262_power, lora_sx1262_preambleLength, lora_sx1262_tcxoVoltage, lora_sx1262_useRegulatorLDO);

    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(F("success!"));
    }
    else
    {
        Serial.print(F("failed, code "));
        Serial.println(state);
        // while (true)
        //     ;
    }

    // set the function that will be called
    // when new packet is received
    radio.setPacketReceivedAction(setFlag);

    // start listening for LoRa packets
    Serial.print(F("[SX1262] Starting to listen ... "));
    state = radio.startReceive();
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

    // if needed, 'listen' mode can be disabled by calling
    // any of the following methods:
    //
    // radio.standby()
    // radio.sleep()
    // radio.transmit();
    // radio.receive();
    // radio.scanChannel();
}

void loop()
{
    // check if the flag is set
    if (receivedFlag)
    {
        // reset flag
        receivedFlag = false;

        // you can read received data as an Arduino String
        String str;
        int state = radio.readData(str);

        // you can also read received data as byte array
        /*
          byte byteArr[8];
          int numBytes = radio.getPacketLength();
          int state = radio.readData(byteArr, numBytes);
        */

        if (state == RADIOLIB_ERR_NONE)
        {
            // packet was successfully received
            Serial.println(F("[SX1262] Received packet!"));

            // print data of the packet
            Serial.print(F("[SX1262] Data:\t\t"));
            Serial.println(str);

            // print RSSI (Received Signal Strength Indicator)
            Serial.print(F("[SX1262] RSSI:\t\t"));
            Serial.print(radio.getRSSI());
            Serial.println(F(" dBm"));

            // print SNR (Signal-to-Noise Ratio)
            Serial.print(F("[SX1262] SNR:\t\t"));
            Serial.print(radio.getSNR());
            Serial.println(F(" dB"));

            // print frequency error
            Serial.print(F("[SX1262] Frequency error:\t"));
            Serial.print(radio.getFrequencyError());
            Serial.println(F(" Hz"));
        }
        else if (state == RADIOLIB_ERR_CRC_MISMATCH)
        {
            // packet was received, but is malformed
            Serial.println(F("CRC error!"));
        }
        else
        {
            // some other error occurred
            Serial.print(F("failed, code "));
            Serial.println(state);
        }
    }
}
