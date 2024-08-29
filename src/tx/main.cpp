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

    // some modules have an external RF switch
    // controlled via two pins (RX enable, TX enable)
    // to enable automatic control of the switch,
    // call the following method
    // RX enable:   4
    // TX enable:   5
    /*
      radio.setRfSwitchPins(4, 5);
    */
}

// counter to keep track of transmitted packets
int count = 0;

void loop()
{
    Serial.print(F("[SX1262] Transmitting packet ... "));

    // you can transmit C-string or Arduino string up to
    // 256 characters long
    String str = "Hello World! #" + String(count++);
    int state = radio.transmit(str);

    // you can also transmit byte array up to 256 bytes long
    /*
      byte byteArr[] = {0x01, 0x23, 0x45, 0x56, 0x78, 0xAB, 0xCD, 0xEF};
      int state = radio.transmit(byteArr, 8);
    */

    if (state == RADIOLIB_ERR_NONE)
    {
        // the packet was successfully transmitted
        Serial.println(F("success!"));

        // print measured data rate
        Serial.print(F("[SX1262] Datarate:\t"));
        Serial.print(radio.getDataRate());
        Serial.println(F(" bps"));
    }
    else if (state == RADIOLIB_ERR_PACKET_TOO_LONG)
    {
        // the supplied packet was longer than 256 bytes
        Serial.println(F("too long!"));
    }
    else if (state == RADIOLIB_ERR_TX_TIMEOUT)
    {
        // timeout occured while transmitting packet
        Serial.println(F("timeout!"));
    }
    else
    {
        // some other error occurred
        Serial.print(F("failed, code "));
        Serial.println(state);
    }

    // wait for a second before transmitting again
    delay(1000);
}
