// created: 10.02.2022
// updated: 26.02.2022

#pragma once


#define _SSID           "CK"                        // Your WiFi credentials here
#define _PW             "Sup3r3r0s47"
#define TZName          "CET-1CEST,M3.5.0,M10.5.0/3"    // Timezone (more TZNames in "rtime.cpp")
#define DECODER         1                               // (0)VS1053 , (1)SW DECODER DAC via I2S
<<<<<<< Updated upstream
#define TFT_CONTROLLER  3                               // (0)ILI9341, (1)HX8347D, (2)ILI9486, (3)ILI9488, (4)Remote Display
=======
#define TFT_CONTROLLER  3                               // (0)ILI9341, (1)HX8347D, (2)ILI9486, (3)ILI9488
>>>>>>> Stashed changes
#define TFT_FREQUENCY   40000000                        // 27000000, 40000000, 80000000
#define TFT_ROTATION    3                               // 0 ... 3
#define TP_VERSION      4                               // (0)ILI9341, (1)ILI9341RPI, (2)HX8347D, (3)ILI9486RPI, (4)ILI9488
#define TP_ROTATION     3                               // 0 ... 3
#define FTP_USERNAME    "esp32"                         // user and pw in FTP Client
#define FTP_PASSWORD    "esp32"
<<<<<<< Updated upstream
#define HOSTNAME Hostname

=======
>>>>>>> Stashed changes



// All RGB565 Color definitions
// remove all those in TFT_eSPI.h
#define TFT_AQUAMARINE      0x7FFA // 127, 255, 212
#define TFT_BEIGE           0xF7BB // 245, 245, 220
<<<<<<< Updated upstream
#define TFT_BLACK           0x0000 //   0,   0,   0
#define TFT_BLUE            0x001F //   0,   0, 255
#define TFT_BROWN2           0xA145 // 165,  42,  42
#define TFT_CHOCOLATE       0xD343 // 210, 105,  30
#define TFT_CORNSILK        0xFFDB // 255, 248, 220
#define TFT_CYAN            0x07FF //   0, 255, 255
#define TFT_DARKGREEN       0x0320 //   0, 100,   0
#define TFT_DARKGREY        0xAD55 // 169, 169, 169
#define TFT_DARKCYAN        0x0451 //   0, 139, 139
#define TFT_DEEPSKYBLUE     0x05FF //   0, 191, 255
#define TFT_GRAY            0x8410 // 128, 128, 128
#define TFT_GREEN           0x0400 //   0, 128,   0
#define TFT_GREENYELLOW     0xAFE5 // 173, 255,  47
#define TFT_GOLD            0xFEA0 // 255, 215,   0
=======
//#define TFT_BLACK           0x0000 //   0,   0,   0
//#define TFT_BLUE            0x001F //   0,   0, 255
#define TFT_BROWN2           0xA145 // 165,  42,  42
#define TFT_CHOCOLATE       0xD343 // 210, 105,  30
#define TFT_CORNSILK        0xFFDB // 255, 248, 220
//#define TFT_CYAN            0x07FF //   0, 255, 255
//#define TFT_DARKGREEN       0x0320 //   0, 100,   0
//#define TFT_DARKGREY        0xAD55 // 169, 169, 169
//#define TFT_DARKCYAN        0x0451 //   0, 139, 139
#define TFT_DEEPSKYBLUE     0x05FF //   0, 191, 255
#define TFT_GRAY            0x8410 // 128, 128, 128
//#define TFT_GREEN           0x0400 //   0, 128,   0
//#define TFT_GREENYELLOW     0xAFE5 // 173, 255,  47
//#define TFT_GOLD            0xFEA0 // 255, 215,   0
>>>>>>> Stashed changes
#define TFT_HOTPINK         0xFB56 // 255, 105, 180
#define TFT_LAVENDER        0xE73F // 230, 230, 250
#define TFT_LAWNGREEN       0x7FE0 // 124, 252,   0
#define TFT_LIGHTBLUE       0xAEDC // 173, 216, 230
#define TFT_LIGHTCYAN       0xE7FF // 224, 255, 255
#define TFT_LIGHTGREY       0xD69A // 211, 211, 211
#define TFT_LIGHTGREEN      0x9772 // 144, 238, 144
#define TFT_LIGHTYELLOW     0xFFFC // 255, 255, 224
#define TFT_LIME            0x07E0 //   0. 255,   0
<<<<<<< Updated upstream
#define TFT_MAGENTA         0xF81F // 255,   0, 255
#define TFT_MAROON          0x7800 // 128,   0,   0
#define TFT_MEDIUMVIOLETRED 0xC0B0 // 199,  21, 133
#define TFT_NAVY            0x000F //   0,   0, 128
#define TFT_OLIVE           0x7BE0 // 128, 128,   0
#define TFT_ORANGE          0xFD20 // 255, 165,   0
#define TFT_PINK            0xFE19 // 255, 192, 203
#define TFT_PURPLE          0x780F // 128,   0, 128
#define TFT_RED             0xF800 // 255,   0,   0
#define TFT_SANDYBROWN      0xF52C // 244, 164,  96
#define TFT_TURQUOISE       0x471A //  64, 224, 208
#define TFT_VIOLET          0x801F // 128,   0, 255
#define TFT_WHITE           0xFFFF // 255, 255, 255
#define TFT_YELLOW          0xFFE0 // 255, 255,   0
=======
//#define TFT_MAGENTA         0xF81F // 255,   0, 255
//#define TFT_MAROON          0x7800 // 128,   0,   0
#define TFT_MEDIUMVIOLETRED 0xC0B0 // 199,  21, 133
//#define TFT_NAVY            0x000F //   0,   0, 128
//#define TFT_OLIVE           0x7BE0 // 128, 128,   0
//#define TFT_ORANGE          0xFD20 // 255, 165,   0
//#define TFT_PINK            0xFE19 // 255, 192, 203
//#define TFT_PURPLE          0x780F // 128,   0, 128
//#define TFT_RED             0xF800 // 255,   0,   0
#define TFT_SANDYBROWN      0xF52C // 244, 164,  96
#define TFT_TURQUOISE       0x471A //  64, 224, 208
//#define TFT_VIOLET          0x801F // 128,   0, 255
//#define TFT_WHITE           0xFFFF // 255, 255, 255
//#define TFT_YELLOW          0xFFE0 // 255, 255,   0
>>>>>>> Stashed changes
/**********************************************************************************************************************/

#include <Arduino.h>
#include <Preferences.h>
#include <Ticker.h>
#include <SPI.h>
<<<<<<< Updated upstream
#include <SD_MMC.h>
#include <SD.h>
//#include <SDFat.h>
#include <FS.h>
//#include <WiFiClient.h>
//#include <WiFiMulti.h>
#include <WiFi.h>
#define HOSTNAME Hostname
#include <DNSServer.h>
#include <WiFiManager.h> 
#include <WiFiUdp.h>
=======
//#include <SD_MMC.h>
#include <SD.h>
#include <SDFat.h>
#include <FS.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
>>>>>>> Stashed changes
#include "index.h"
#include "websrv.h"
#include "rtime.h"
#include "IR.h"
//#include "tft.h"
<<<<<<< Updated upstream
#include "BluetoothA2DPSink.h"  //BT-In
#include "ESP32FtpServer.h"
#include "soc/rtc_wdt.h"
//#include "fonts/Arial22num.h"
#include "ES8388.h" 
#include <WiFiUdp.h>
#include <esp_task_wdt.h>
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
//#include <TFT_eSPI.h>
//#include <TJpg_Decoder.h>
=======
//#include "BluetoothA2DPSink.h"  //BT-In
#include "ESP32FtpServer.h"
//#include "fonts/Arial22num.h"
#include "ES8388.h" 
>>>>>>> Stashed changes








// Digital I/O used
/*#define VS1053_CS     2
#define VS1053_DCS     4
#define VS1053_DREQ   32
#define TFT_CS        22
#define TFT_DC        21
#define TFT_BL        0
#define TP_IRQ        33
#define TP_CS         15*/
//#define SD_MMC_D0      2  // cannot be changed
//#define SD_MMC_CLK    14  // cannot be changed
//#define SD_MMC_CMD    15  // cannot be changed
#define SD_CS         13    // 5
#define VS1053_MOSI   15  // SD
#define VS1053_MISO   2  // SD
#define VS1053_SCK    14  //SD
//#define IR_PIN        3
/*#define SPI_MOSI      13  // TFT and TP (HSPI)
#define SPI_MISO      12  // TFT and TP (HSPI)
#define SPI_SCK       14  // TFT and TP (HSPI)
#define VS1053_MOSI   23  // VS1053     (VSPI)
#define VS1053_MISO   19  // VS1053     (VSPI)
#define VS1053_SCK    18  // VS1053     (VSPI)*/
#define I2S_DOUT      26    //26    //25  // DAC: I2S_DIN
#define I2S_BCLK      5
#define I2S_LRC       25    //26
#define I2S_MCLK       0  // mostly not used
#define I2S_ASDOUT      35  // mostly not used

// I2C GPIOs
#define IIC_CLK       23
#define IIC_DATA      18

<<<<<<< Updated upstream
// LyraT_buttons and specials
#define GPIO_PA_EN    21
#define SET           32
#define REC           36
#define MODE          39
#define PLAY          33
#define VOLUP         27
#define VOLDWN        13
#define BOOT          2

=======
// Amplifier enable
#define GPIO_PA_EN    21
>>>>>>> Stashed changes

#define SerialPrintfln(...) {xSemaphoreTake(mutex_rtc, portMAX_DELAY); \
                            Serial.printf("%s ", rtc.gettime_s()); \
                            Serial.printf(__VA_ARGS__); \
                            Serial.println(""); \
                            xSemaphoreGive(mutex_rtc);}




/**********************************************************************************************************************/

extern boolean psRAMavail;
extern String _mp3Name[];
extern boolean audioTask_runs;
extern boolean BTTask_runs;
extern boolean _Audio;

// //prototypes (main.cpp)
boolean defaultsettings();
boolean saveStationsToNVS();
<<<<<<< Updated upstream
void setTFTbrightness(uint8_t duty);
const char* UTF8toASCII(const char* str);
const char* ASCIItoUTF8(const char* str);
void showHeadlineVolume(uint8_t vol);
void showHeadlineTime();
void showHeadlineItem(uint8_t idx);
void showFooterIPaddr();
void showFooterStaNr();
void updateSleepTime(boolean noDecrement = false);
void showVolumeBar();
void showBrightnessBar();
void setBrightness(uint8_t br);
void showFooter();
void display_info(const char *str, int xPos, int yPos, uint16_t color, uint16_t indent, uint16_t winHeight);
void showStreamTitle();
void showLogoAndStationName();
void showFileName(const char* fname);
void display_time(boolean showall = false);
void display_alarmDays(uint8_t ad, boolean showall=false);
void display_alarmtime(int8_t xy = 0, int8_t ud = 0, boolean showall = false);
void display_sleeptime(int8_t ud = 0);
boolean drawImage(const char* path, uint16_t posX, uint16_t posY, uint16_t maxWidth = 0 , uint16_t maxHeigth = 0);
=======
//void setTFTbrightness(uint8_t duty);
const char* UTF8toASCII(const char* str);
const char* ASCIItoUTF8(const char* str);
//void showHeadlineVolume(uint8_t vol);
//void showHeadlineTime();
//void showHeadlineItem(uint8_t idx);
//void showFooterIPaddr();
//void showFooterStaNr();
//void updateSleepTime(boolean noDecrement = false);
//void showVolumeBar();
//void showBrightnessBar();
//void showFooter();
//void display_info(const char *str, int xPos, int yPos, uint16_t color, uint16_t indent, uint16_t winHeight);
//void showStreamTitle();
//void showLogoAndStationName();
//void showFileName(const char* fname);
//void display_time(boolean showall = false);
//void display_alarmDays(uint8_t ad, boolean showall=false);
//void display_alarmtime(int8_t xy = 0, int8_t ud = 0, boolean showall = false);
//void display_sleeptime(int8_t ud = 0);
//boolean drawImage(const char* path, uint16_t posX, uint16_t posY, uint16_t maxWidth = 0 , uint16_t maxHeigth = 0);
>>>>>>> Stashed changes
bool setAudioFolder(const char* audioDir);
const char* listAudioFile();
bool sendAudioList2Web(const char* audioDir);
bool connectToWiFi();
const char* byte_to_binary(int8_t x);
bool startsWith (const char* base, const char* str);
bool endsWith (const char* base, const char* str);
int indexOf (const char* base, const char* str, int startIndex);
boolean strCompare(char* str1, char* str2);
boolean strCompare(const char* str1, char* str2);
const char* scaleImage(const char* path);
inline uint8_t getvolume();
uint8_t downvolume();
uint8_t upvolume();
void setStation(uint16_t sta);
void nextStation();
void prevStation();
void StationsItems();
<<<<<<< Updated upstream
void changeBtn_pressed(uint8_t btnNr);
void changeBtn_released(uint8_t btnNr);
void savefile(const char* fileName, uint32_t contentLength);
String setTone();
String setI2STone();
bool send_tracks_to_web(void);
void audiotrack(const char* fileName, uint32_t resumeFilePos = 0);
void changeState(int state);
void connecttohost(const char* host);
void connecttoFS(const char* filename, uint32_t resumeFilePos = 0);
void stopSong();
void tp_released();
void tp_pressed(uint16_t x, uint16_t y);
void UDPsendPacket(void);

=======
//void changeBtn_pressed(uint8_t btnNr);
//void changeBtn_released(uint8_t btnNr);
void savefile(const char* fileName, uint32_t contentLength);
String setTone();
String setI2STone();
void audiotrack(const char* fileName, uint32_t resumeFilePos = 0);
//void changeState(int state);
void connecttohost(const char* host);
void connecttoFS(const char* filename, uint32_t resumeFilePos = 0);
void stopSong();
//void tp_released();
//void tp_pressed(uint16_t x, uint16_t y);
>>>>>>> Stashed changes

// //prototypes (audiotask.cpp)
void audioTask(void *parameter);
void BTTask(void *parameter);
void avrc_metadata_callback(uint8_t id, const uint8_t *text);
void audioInit();
void audioSetVolume(uint8_t vol);
uint8_t audioGetVolume();
boolean audioConnecttohost(const char* host);
boolean audioConnecttoFS(const char* filename, uint32_t resumeFilePos = 0);
uint32_t audioStopSong();
void audioSetTone(int8_t param0, int8_t param1, int8_t param2, int8_t param3 = 0);
uint32_t audioInbuffFilled();
<<<<<<< Updated upstream
uint32_t audioInbuffFree();


=======
uint32_t audioInbuffFree();
>>>>>>> Stashed changes
