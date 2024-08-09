// GxEPD2_HelloWorld.ino by Jean-Marc Zingg
//
// Display Library example for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// Display Library based on Demo Example from Good Display: https://www.good-display.com/companyfile/32/
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

// Supporting Arduino Forum Topics (closed, read only):
// Good Display ePaper for Arduino: https://forum.arduino.cc/t/good-display-epaper-for-arduino/419657
// Waveshare e-paper displays with SPI: https://forum.arduino.cc/t/waveshare-e-paper-displays-with-spi/467865
//
// Add new topics in https://forum.arduino.cc/c/using-arduino/displays/23 for new questions and issues

// see GxEPD2_wiring_examples.h for wiring suggestions and examples
// if you use a different wiring, you need to adapt the constructor parameters!

// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX
// #include <GFX.h>

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
// #include <Fonts/FreeSansBold24pt7b.h>
#include <../fonts/RobotoCondensed_Bold48pt7b.h>
#include <../fonts/Roboto_Regular10pt7b.h>

// select the display class and display driver class in the following file (new style):
#include "GxEPD2_display_selection_new_style.h"

// or select the display constructor line in one of the following files (old style):
// #include "GxEPD2_display_selection.h"
// #include "GxEPD2_display_selection_added.h"

// alternately you can copy the constructor from GxEPD2_display_selection.h or GxEPD2_display_selection_added.h to here
// e.g. for Wemos D1 mini:
// GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(/*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 4)); // GDEH0154D67

// for handling alternative SPI pins (ESP32, RP2040) see example GxEPD2_Example.ino

void write_temp(float temp)
{
    const char *info_text = "Siste: 2024-08-09 17:18:42";
    char temp_text[10];                  // Make sure this is large enough to hold the formatted string
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

void setup()
{
    // display.init(115200); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
    display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
    write_temp(20.4f);
    display.hibernate();
}

void loop() {};
