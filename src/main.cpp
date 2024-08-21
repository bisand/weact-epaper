// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable or disable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
// #include <Fonts/FreeSansBold24pt7b.h>
#include <../fonts/RobotoCondensed_Bold48pt7b.h>
#include <../fonts/Roboto_Regular10pt7b.h>

// ESP32 CS(SS)=5,SCL(SCK)=18,SDA(MOSI)=23,BUSY=15,RES(RST)=2,DC=0
#define CS 5
#define RES 16
#define DC 17
#define BUSY 4

// 1.54'' EPD Module
// GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(/*CS=5*/ 5, /*DC=*/ 0, /*RES=*/ 2, /*BUSY=*/ 15)); // GDEH0154D67 200x200, SSD1681

// 2.13'' EPD Module
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(CS, DC, RES, BUSY)); // DEPG0213BN 122x250, SSD1680
// GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> display(GxEPD2_213_Z98c(/*CS=5*/ 5, /*DC=*/ 0, /*RES=*/ 2, /*BUSY=*/ 15)); // GDEY0213Z98 122x250, SSD1680

// 2.9'' EPD Module
// GxEPD2_BW<GxEPD2_290_BS, GxEPD2_290_BS::HEIGHT> display(GxEPD2_290_BS(/*CS=5*/ 5, /*DC=*/ 0, /*RES=*/ 2, /*BUSY=*/ 15)); // DEPG0290BS 128x296, SSD1680
// GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(/*CS=5*/ 5, /*DC=*/ 0, /*RES=*/ 2, /*BUSY=*/ 15)); // GDEM029C90 128x296, SSD1680

// 4.2'' EPD Module
// GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(/*CS=5*/ 5, /*DC=*/ 0, /*RES=*/ 2, /*BUSY=*/ 15)); // 400x300, SSD1683

void write_temp(float temp)
{
    const char *info_text = "Siste: 2024-08-09 17:18:42";
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

void setup()
{
    // display.init(115200); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
    display.init(115200, true, 50, false);
    write_temp(20.4f);
    display.hibernate();
}

void loop() {};
