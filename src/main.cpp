/***********************************************************************************************************************
    MiniWebRadio -- Webradio receiver for ESP32

    first release on 03/2017
    Version 2.e, Feb 26/2022

    2.8" color display (320x240px) with controller ILI9341 or HX8347D (SPI) or
    3.5" color display (480x320px) wihr controller ILI9486 (SPI)

    HW decoder VS1053 or
    SW decoder with external DAC over I2S

    SD is mandatory
    IR remote is optional

***********************************************************************************************************************/

// THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT.
// FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHOR
// OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE

#include "common.h"
#include "pins.h"

#include "network.h"
#include "dmesg_functions.h"
#include "file_system.h"
#include "time_functions.h"               // file_system.h is needed prior to #including time_functions.h if you want to store the default parameters
#include "ftpClient.h"                    // file_system.h is needed prior to #including ftpClient.h if you want to store the default parameters



//global variables

const uint8_t  _max_volume     = 21;
const uint16_t _max_stations   = 1000;
uint8_t        _alarmdays      = 0;
uint16_t       _cur_station    = 0;      // current station(nr), will be set later
uint16_t       _sleeptime      = 0;      // time in min until MiniWebRadio goes to sleep
uint16_t       _sum_stations   = 0;
uint8_t        _cur_volume     = 0;      // will be set from stored preferences
uint8_t        _state          = 0;      // statemaschine
uint8_t        _touchCnt       = TouchCnt;
uint8_t        _commercial_dur = 0;      // duration of advertising
uint16_t       _alarmtime      = 0;      // in minutes (23:59 = 23 *60 + 59)
int8_t         _releaseNr      = -1;
uint32_t       _resumeFilePos  = 0;
uint16_t x, y;                  // touch points
uint8_t         rtc_tries=0;
int             input=1;        //1=radio; 2=Player_SD; 4= Player_network?; 8=BT_in
int             output=1;       //1=speaker(I2S); 2=BT-out; 4=? 
char           _chbuf[512];
char           _myIP[25];
char           _afn[256];                // audioFileName
char           _path[128];
char           _prefix[5]      = "/m";
char*          _lastconnectedfile = nullptr;
char*          _lastconnectedhost = nullptr;
char*          _stationURL = nullptr;
const char*    _pressBtn[7];
const char*    _releaseBtn[7];
boolean        _f_rtc  = false;             // true if time from ntp is received
boolean        _f_1sec = false;
boolean        _f_1min = false;
boolean        _f_mute = false;
boolean        _f_sleeping = false;
boolean        _f_isWebConnected = false;
boolean        _f_isFSConnected = false;
boolean        _f_eof = false;
boolean        _f_eof_alarm = false;
boolean        _f_semaphore = false;
boolean        _f_alarm = false;
boolean        _f_irNumberSeen = false;
boolean        _f_newIcyDescription = false;
boolean        _f_newStreamTitle = false;
boolean        _f_newCommercial = false;
boolean        _f_volBarVisible = false;
boolean        _f_switchToClock = false;  // jump into CLOCK mode at the next opportunity
boolean        _f_hpChanged = false; // true, if HeadPhone is plugged or unplugged
boolean        _f_muteIncrement = false; // if set increase Volume (from 0 to _cur_volume)
boolean        _f_muteDecrement = false; // if set decrease Volume (from _cur_volume to 0)
boolean        _f_timeAnnouncement = false; // time announcement every full hour
boolean        _f_playlistEnabled = false;
boolean        _f_loop=true;
boolean        _BT_In=false;
boolean         audioTask_runs=false;
boolean         BTTask_runs=   false;

String         _station = "";
String         _stationName_nvs = "";
String         _stationName_air = "";
String         _homepage = "";
String         _streamTitle = "";
String         _filename = "";
String         _icydescription = "";
boolean         psRAMavail=false;

uint           _numServers = 0;
uint8_t        _level = 0;
int            _currentServer = -1;
uint32_t       _media_downloadPort = 0;
String         _media_downloadIP = "";
//vector<String> _names{};

char timc[20]; //for digital time
int clocktype=2;
int previous_sec=100;
int previous_day=100;
bool but_done=true;
bool volbut_done=true;
bool ButPlay=false;
bool ButSet=false;
bool ButVolMin=false;
bool ButVolPlus=false;

uint8_t nbroftracks=0;
int previousMillis=0;
bool mp3playall = true;        //play next mp3, after end of file when true
bool shuffle_play=false;
String _audiotrack="";             //track from SD/mp3files/
String artsong="";
String connectto="";
String Title="";
char packet[255]; //for incoming packet UDP
char * packettosend;
char reply[] = "Received by ESP32_Lyrat_Musicplayer"; //create reply
int packetnumber=0;

char _hl_item[11][25]{                          // Title in headline
                "Internet Radio ",         // "* интернет-радио *"  "ραδιόφωνο Internet"
                "Internet Radio ",
                "Internet Radio ",
                "Clock ",                    // Clock "** часы́ **"  "** ρολόι **"
                "Clock ",
                "Brightness ",             // Brightness яркость λάμψη
                "Audioplayer ",            // "** цифрово́й плеер **"
                "Audioplayer ",
                "" ,                            // Alarm should be empty
                "Sleeptimer ",       // "Sleeptimer" "Χρονομετρητής" "Таймер сна"
                "Settings",
};

enum status{RADIO = 0, RADIOico = 1, RADIOmenue = 2,
            CLOCK = 3, CLOCKico = 4, BRIGHTNESS = 5,
            PLAYER= 6, PLAYERico= 7,
            ALARM = 8, SLEEP    = 9, SETTINGS = 10};

/** variables for time, set in platformio.ini*/
char NTP_pool_name[] = NTP_pool;
const char* ntpServer = NTP_pool_name;

WiFiUDP UDP;
WebSrv webSrv;
WiFiManager wifiManager;
Preferences pref;
Preferences stations;
RTIME rtc;
struct tm timeinfo;
time_t now;
hw_timer_t * timer = NULL;
Ticker ticker;
//IR ir(IR_PIN);                  // do not change the objectname, it must be "ir"
//TP tp(TP_CS, TP_IRQ);
//WiFiMulti wifiMulti;
File audioFile;
FtpServer ftpSrv;
//BluetoothA2DPSink a2dp_sink;
/** Task handle of the taskhandler */
TaskHandle_t audioTaskHandler;
TaskHandle_t BTTaskHandler;



SemaphoreHandle_t  mutex_rtc;
//SemaphoreHandle_t  mutex_display;


portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
//volatile bool screen_touched = false;
//portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

#define AA_FONT_SMALL 20
#define AA_FONT_NORMAL 32
#define AA_FONT_LARGE 42
#define AA_FONT_XLARGE 50
#define AA_FONT_XXLARGE 60
//#define AA_FONT_num22 Arial22
#define AA_FONT_num50 50       //for file in a .h => array refs, no "" quotes
#define AA_FONT_num100 60
int fontLoaded=0;

/** hardware timer to count secs */
volatile int interruptCounter;  //msecs; int(max) = 2.147.483.647; a day = 864000 secs, so over 24.854 days or 68 years
volatile int totalcounter;	//multisecs (long eg. hours)
volatile int mediumcounter;	//multisecs (medium long eg. minutes)
volatile int shortcounter;  //sec
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);    //portenter and exit are needed as to block it for other processes
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux); 
}

#if TFT_CONTROLLER == 0 || TFT_CONTROLLER == 1
    //
    //  Display 320x240
    //  +-------------------------------------------+ _yHeader=0
    //  | Header                                    |       _hHeader=20px
    //  +-------------------------------------------+ _yName=20
    //  |                                           |
    //  | Logo                   StationName        |       _hName=100px
    //  |                                           |
    //  +-------------------------------------------+ _yTitle=120
    //  |                                           |
    //  |              StreamTitle                  |       _hTitle=100px
    //  |                                           |
    //  +-------------------------------------------+ _yFooter=220
    //  | Footer                                    |       _hFooter=20px
    //  +-------------------------------------------+ 240
    //                                             320
    const unsigned short* _fonts[6] = {
        Times_New_Roman15x14,
        Times_New_Roman21x17,
        Times_New_Roman27x21,
        Times_New_Roman34x27,
        Times_New_Roman38x31,
        Times_New_Roman43x35,
    };

    struct w_h {uint16_t x = 0;   uint16_t y = 0;   uint16_t w = 320; uint16_t h = 20; } const _winHeader;
    struct w_l {uint16_t x = 0;   uint16_t y = 20;  uint16_t w = 100; uint16_t h = 100;} const _winLogo;
    struct w_n {uint16_t x = 100; uint16_t y = 20;  uint16_t w = 220; uint16_t h = 100;} const _winName;
    struct w_e {uint16_t x = 0;   uint16_t y = 20;  uint16_t w = 320; uint16_t h = 100;} const _winFName;
    struct w_t {uint16_t x = 0;   uint16_t y = 120; uint16_t w = 320; uint16_t h = 100;} const _winTitle;
    struct w_f {uint16_t x = 0;   uint16_t y = 220; uint16_t w = 320; uint16_t h = 20; } const _winFooter;
    struct w_i {uint16_t x = 0;   uint16_t y = 0;   uint16_t w = 180; uint16_t h = 20; } const _winItem;
    struct w_v {uint16_t x = 180; uint16_t y = 0;   uint16_t w =  50; uint16_t h = 20; } const _winVolume;
    struct w_m {uint16_t x = 260; uint16_t y = 0;   uint16_t w =  60; uint16_t h = 20; } const _winTime;
    struct w_s {uint16_t x = 0;   uint16_t y = 220; uint16_t w =  60; uint16_t h = 20; } const _winStaNr;
    struct w_p {uint16_t x = 60;  uint16_t y = 220; uint16_t w = 120; uint16_t h = 20; } const _winSleep;
    struct w_a {uint16_t x = 180; uint16_t y = 220; uint16_t w = 160; uint16_t h = 20; } const _winIPaddr;
    struct w_b {uint16_t x = 0;   uint16_t y = 120; uint16_t w = 320; uint16_t h = 14; } const _winVolBar;
    struct w_o {uint16_t x = 0;   uint16_t y = 154; uint16_t w =  64; uint16_t h = 64; } const _winButton;
    uint16_t _alarmdaysXPos[7] = {3, 48, 93, 138, 183, 228, 273};
    uint16_t _alarmtimeXPos[5] = {2, 75, 173, 246, 148}; // last is colon
    uint16_t _sleeptimeXPos[5] = {5, 77, 129, 57}; // last is colon
    uint8_t  _alarmdays_w = 44 + 4;
    uint8_t  _alarmdays_h = 40;
    uint16_t _dispHeight  = 240;
    uint8_t  _tftSize     = 0;
    //
    TFT tft(TFT_CONTROLLER);
    //
#endif //TFT_CONTROLLER == 0 || TFT_CONTROLLER == 1


#if TFT_CONTROLLER == 2 || TFT_CONTROLLER == 3 || TFT_CONTROLLER == 4
    //
    //  Display 480x320
    //  +-------------------------------------------+ _yHeader=0
    //  | Header                                    |       _winHeader=30px
    //  +-------------------------------------------+ _yName=30
    //  |                                           |
    //  | Logo                   StationName        |       _winFName=130px
    //  |                                           |
    //  +-------------------------------------------+ _yVolumebar=160
    //  +-------------------------------------------+ _yTitle=165
    //  |                                           |
    //  |              StreamTitle                  |       _winTitle=120px
    //  |                                           |
    //  +-------------------------------------------+ _yFooter=290
    //  | Footer                                    |       _winFooter=30px
    //  +-------------------------------------------+ 320
    //                                             480
/*
    const unsigned short* _fonts[6] = {
        Times_New_Roman27x21,
        Times_New_Roman34x27,
        Times_New_Roman38x31,
        Times_New_Roman43x35,
        Times_New_Roman56x46,
        Times_New_Roman66x53,
    };
*/
// Window sizes
    struct w_h {uint16_t x = 0;   uint16_t y = 0;   uint16_t w = 480; uint16_t h = 30; } const _winHeader;
    struct w_l {uint16_t x = 0;   uint16_t y = 30;  uint16_t w = 130; uint16_t h = 130;} const _winLogo;
    struct w_n {uint16_t x = 135; uint16_t y = 30;  uint16_t w = 345; uint16_t h = 130;} const _winName;
    struct w_e {uint16_t x = 0;   uint16_t y = 30;  uint16_t w = 480; uint16_t h = 130;} const _winFName;
    struct w_b {uint16_t x = 0;   uint16_t y = 160; uint16_t w = 480; uint16_t h = 5;} const _winVolBar;
    struct w_t {uint16_t x = 0;   uint16_t y = 165; uint16_t w = 480; uint16_t h = 125;} const _winTitle;
    struct w_f {uint16_t x = 0;   uint16_t y = 290; uint16_t w = 480; uint16_t h = 30; } const _winFooter;
    struct w_m {uint16_t x = 390; uint16_t y = 0;   uint16_t w =  90; uint16_t h = 30; } const _winTime;
    struct w_i {uint16_t x = 0;   uint16_t y = 0;   uint16_t w = 280; uint16_t h = 30; } const _winItem;
    struct w_v {uint16_t x = 210; uint16_t y = 0;   uint16_t w = 90; uint16_t h = 30; } const _winVolume;
    struct w_a {uint16_t x = 240; uint16_t y = 295; uint16_t w = 250; uint16_t h = 25; } const _winIPaddr;
    struct w_s {uint16_t x = 0;   uint16_t y = 295; uint16_t w = 100; uint16_t h = 25; } const _winStaNr;
    struct w_p {uint16_t x = 140; uint16_t y = 295; uint16_t w = 70; uint16_t h = 25; } const _winSleep;
    //struct w_b {uint16_t x = 0;   uint16_t y = 160; uint16_t w = 480; uint16_t h = 30; } const _winVolBar;
    struct w_o {uint16_t x = 0;   uint16_t y = 210; uint16_t w =  69; uint16_t h = 96; } const _winButton;  //w was 96, y was 190 but I use small buttons to use up to 7 buttons
    uint16_t _alarmdaysXPos[7] = {2, 70, 138, 206, 274, 342, 410};
    uint16_t _alarmtimeXPos[5] = {12, 118, 266, 372, 224}; // last is colon
    uint16_t _sleeptimeXPos[5] = {5, 107, 175, 73 };
    uint8_t  _alarmdays_w = 64 + 4;
    uint8_t  _alarmdays_h = 56;
    uint16_t _dispHeight  = 320;
    uint8_t  _tftSize     = 1;
    //
    //TFT tft(TFT_CONTROLLER);
    //
#endif  // #if TFT_CONTROLLER == 2 || TFT_CONTROLLER == 3

//TIME  *********************************************************************************************************
void get_time(void){
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        rtc_tries++;
        if(rtc_tries>24){_f_rtc=false;}  //if _f_rtc=false every minute if _f_rtc=true every hour; max 24 hours without sync
        return;
    }

    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    if (timeinfo.tm_year+1900 > 2020) 
    {
        _f_rtc=true; 
        rtc_tries=0;
    }else{
        _f_rtc=false;
    }
    Serial.print("_f_rtc= "); Serial.println(_f_rtc);
}
const char* gettime_s(){  // hh:mm:ss
	time(&now);
	localtime_r(&now, &timeinfo);
	sprintf(timc,"%02d:%02d:%02d ",  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
	return timc;
}
const char* gettime_xs(){  // hh:mm
    time(&now);
    localtime_r(&now, &timeinfo);
    sprintf(timc,"%02d:%02d",  timeinfo.tm_hour, timeinfo.tm_min);
    return timc;
}
uint8_t getweekday(){ //So=0, Mo=1 ... Sa=6
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_wday;
}
uint16_t getMinuteOfTheDay(){ // counts at 00:00, from 0...23*60+59
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_hour * 60 + timeinfo.tm_min;
}
//WiFi and Reset ************************************************************************************************
#ifndef ip_fixed    //This happens when ip_fixed is commented out (or not present) in platformio.ini ->DHCP
    IPAddress local_IP;    //unit ip, gateway and subnet are set in platformio.ini
    IPAddress gateway;
    IPAddress subnet;
#endif
#ifdef ip_fixed     //This happens when ip_fixed has a value in platformio.ini ->Fixed ip
    IPAddress local_IP(unit_ip);    //unit ip, gateway and subnet are set in platformio.ini
    IPAddress gateway(unit_gateway);    //editor might complain, as it is not found in the code
    IPAddress subnet(unit_subnet);
#endif
//const char* ssid = WIFI_SSID;
//const char* password =  WIFI_PASS;

void UDP_Check(void)
{
  int packetSize = UDP.parsePacket();
  if (packetSize) {
    // read the packet into packetBufffer
    int len = UDP.read(packet, 255);
    if (len > 0) {
      packet[len] = 0;
    }
    Serial.print("Contents:"); Serial.println(packet);
    String StrPacket= String(packet);
    if(!StrPacket.startsWith("Received"))  //No reply to a reply
    {
      int p[4];
      int StrPos=StrPacket.indexOf(",");
      String StrCommand=StrPacket.substring(0, StrPos);
      String rest=StrPacket.substring(StrPos+1,255);
      //if(StrCommand=="pressed"){return;}
      if(StrCommand=="xy-touched") {for(int i=0; i<3; i++) {StrPos=rest.indexOf(",");   //handle steps to take (tft messages) before acknowledging packetnumber
        p[i] = rest.substring(0,StrPos).toInt(); rest=rest.substring(StrPos+1,255);} x=p[1]; y=p[2]; tp_pressed(x,y);}
      if(StrCommand=="Alarm triggered") {for(int i=0; i<3; i++) {StrPos=rest.indexOf(",");
        p[i] = rest.substring(0,StrPos).toInt(); rest=rest.substring(StrPos+1,255);} _f_alarm=true; }
        packetnumber=p[0];
        // send a reply, to the IP address and port that sent us the packet we received
        UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
        UDP.printf("%s data from %s number, %d\n", reply, UDP.remoteIP().toString(), packetnumber);
        UDP.endPacket();
    }
  }
}


void BTInit() {
    xTaskCreatePinnedToCore(
        BTTask,              // Function to implement the task 
        "BT-In",            // Name of the task 
        5000,                   // Stack size in words 
        NULL,                   // Task input parameter 
        2 | portPRIVILEGE_BIT,  // Priority of the task 
        &BTTaskHandler,                   // Task handle. 
        0                       // Core where the task should run 
    );
}
//BT-In
void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  Serial.printf("==> AVRC metadata rsp: attribute id 0x%x, %s\n", id, text);
}

void wifi_conn(void)  // Connect the WiFi
{
	wifiManager.setConnectRetries(3);
    wifiManager.setHostname(HOSTNAME);
    wifiManager.autoConnect(WiFiAP);
    Serial.print("WiFi RSSI = ");
    Serial.println(WiFi.RSSI());
    Serial.print("WiFi Quality = ");
    Serial.println(wifiManager.getRSSIasQuality(WiFi.RSSI()));
	// Print information how to connect
	IPAddress ip = WiFi.localIP();
	//Serial.print("\nWiFi connected with IP ");
	//Serial.println(ip);
    sprintf(_myIP, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

/** Handle resetDevice */
void resetDevice(void)
{
	delay(100);
	WiFi.disconnect();
	esp_restart();
}

/***********************************************************************************************************************
*                                        D E F A U L T S E T T I N G S                                                 *
***********************************************************************************************************************/
boolean defaultsettings(){
    if(pref.getUInt("default", 0) != 1000){
        log_i("first init, set defaults");
		if(!saveStationsToNVS()) return false;
        pref.clear();
        //
        pref.putUShort("alarm_weekday",0); // for alarmclock
        pref.putUInt("alarm_time", 0);
        pref.putUShort("ringvolume",21);
        //
        pref.putUShort("volume",4); // 0...21
        pref.putUShort("mute",   0); // no mute

        pref.putUShort("brightness", 100); // 0...100
        pref.putUInt("sleeptime", 0);

        pref.putUShort("toneha", 0); // BassFreq 0...15        VS1053
        pref.putUShort("tonehf", 0); // TrebleGain 0...14      VS1053
        pref.putUShort("tonela", 0); // BassGain 0...15        VS1053
        pref.putUShort("tonelf", 0); // BassFreq 0...13        VS1053

        pref.putShort("toneLP", 0); // -40 ... +6 (dB)     I2S DAC
        pref.putShort("toneBP", 0); // -40 ... +6 (dB)     I2S DAC
        pref.putShort("toneHP", 0); // -40 ... +6 (dB)     I2S DAC
        //
        pref.putUInt("station", 1);
        //
        pref.putUInt("default", 1000);
    }
	return true;
}

boolean saveStationsToNVS(){
    String X="", Cy="", StationName="", StreamURL="", currentLine="", tmp="";
    uint16_t cnt = 0;
    // StationList
	if(!SD_MMC.exists("/stations.csv")){
		log_e("SD/stations.csv not found");
		return false;
	}

    File file = SD_MMC.open("/stations.csv");
    if(file){  // try to read from SD
        stations.clear();
        currentLine = file.readStringUntil('\n');             // read the headline
        while(file.available()){
            currentLine = file.readStringUntil('\n');         // read the line
            uint p = 0, q = 0;
            X=""; Cy=""; StationName=""; StreamURL="";
            for(int i = 0; i < currentLine.length() + 1; i++){
                if(currentLine[i] == '\t' || i == currentLine.length()){
                    if(p == 0) X            = currentLine.substring(q, i);
                    if(p == 1) Cy           = currentLine.substring(q, i);
                    if(p == 2) StationName  = currentLine.substring(q, i);
                    if(p == 3) StreamURL    = currentLine.substring(q, i);
                    p++;
                    i++;
                    q = i;
                }
            }
            if(X == "*") continue;
            if(StationName == "") continue; // is empty
            if(StreamURL   == "") continue; // is empty
            //log_i("Cy=%s, StationName=%s, StreamURL=%s",Cy.c_str(), StationName.c_str(), StreamURL.c_str());
            cnt++;
            if(cnt ==_max_stations){
                SerialPrintfln("No more than %d entries in stationlist allowed!", _max_stations);
                cnt--; // maxstations 999
                break;
            }
            tmp = StationName + "#" + StreamURL;
            sprintf(_chbuf, "station_%03d", cnt);
            stations.putString(_chbuf, tmp);
        }
        _sum_stations = cnt;
        stations.putLong("stations.size", file.size());
        file.close();
        stations.putUInt("sumstations", cnt);
        SerialPrintfln("stationlist internally loaded");
        SerialPrintfln("number of stations: %i", cnt);
        return true;
    }
    else return false;
}

/***********************************************************************************************************************
*                                                     A S C I I                                                        *
***********************************************************************************************************************/
const char* UTF8toASCII(const char* str){
    uint16_t i=0, j=0;
    char tab[96]={
          96,173,155,156, 32,157, 32, 32, 32, 32,166,174,170, 32, 32, 32,248,241,253, 32,
          32,230, 32,250, 32, 32,167,175,172,171, 32,168, 32, 32, 32, 32,142,143,146,128,
          32,144, 32, 32, 32, 32, 32, 32, 32,165, 32, 32, 32, 32,153, 32, 32, 32, 32, 32,
         154, 32, 32,225,133,160,131, 32,132,134,145,135,138,130,136,137,141,161,140,139,
          32,164,149,162,147, 32,148,246, 32,151,163,150,129, 32, 32,152
     };
    while((str[i]!=0)&&(j<1020)){
        _chbuf[j]=str[i];
        if(str[i]==0xC2){ // compute unicode from utf8
            i++;
            if((str[i]>159)&&(str[i]<192)) _chbuf[j]=tab[str[i]-160];
            else _chbuf[j]=32;
        }
        else if(str[i]==0xC3){
            i++;
            if((str[i]>127)&&(str[i]<192)) _chbuf[j]=tab[str[i]-96];
            else _chbuf[j]=32;
        }
        i++; j++;
    }
    _chbuf[j]=0;
    return (_chbuf);
}
const char* ASCIItoUTF8(const char* str){
    uint16_t i=0, j=0, uni=0;
    uint16_t tab[128]={
         199, 252, 233, 226, 228, 224, 229, 231, 234, 235, 232, 239, 238, 236, 196, 197,
         201, 230, 198, 244, 246, 242, 251, 249, 255, 214, 220, 162, 163, 165,8359, 402,
         225, 237, 243, 250, 241, 209, 170, 186, 191,8976, 172, 189, 188, 161, 171, 187,
        9617,9618,9619,9474,9508,9569,9570,9558,9557,9571,9553,9559,9565,9564,9563,9488,
        9492,9524,9516,9500,9472,9532,9566,9567,9562,9556,9577,9574,9568,9552,9580,9575,
        9576,9572,9573,9561,9560,9554,9555,9579,9578,9496,9484,9608,9604,9612,9616,9600,
         945, 223, 915, 960, 931, 963, 181, 964, 934, 920, 937, 948,8734, 966, 949,8745,
        8801, 177,8805,8804,8992,8993, 247,8776, 176,8729, 183,8730,8319, 178,9632, 160
    };
    while((str[i]!=0)&&(j<1020)){
        uni=str[i];
        if(uni>=128){uni-=128; uni=tab[uni];}
//            uni=UTF8fromASCII(str[i]);
            switch(uni){
                case   0 ... 127:{_chbuf[j]=str[i]; i++; j++; break;}
                case 160 ... 191:{_chbuf[j]=0xC2; _chbuf[j+1]=uni; j+=2; i++; break;}
                case 192 ... 255:{_chbuf[j]=0xC3; _chbuf[j+1]=uni-64; j+=2; i++; break;}
                default:{_chbuf[j]=' '; i++; j++; break;} // ignore all other
            }
    }
    _chbuf[j]=0;
    return _chbuf;
}
/***********************************************************************************************************************
*                                                     T I M E R                                                        *
***********************************************************************************************************************/
void timer1sec() {
    static volatile uint8_t sec=0;
    _f_1sec = true;
    sec++;
    //log_i("sec=%i", sec);
    if(sec==60){sec=0; _f_1min = true;}
}
/***********************************************************************************************************************
*                                                   D I S P L A Y                                                      *
***********************************************************************************************************************/
inline void clearHeader() {UDP.print("showTime(0)\n");UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winHeader.x, _winHeader.y, _winHeader.w, _winHeader.h, TFT_BLACK);}
inline void clearLogo()   {UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winLogo.x,   _winLogo.y,   _winLogo.w,   _winLogo.h,   TFT_BLACK);}
inline void clearStation(){UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winName.x,   _winName.y,   _winName.w,   _winName.h,   TFT_BLACK);}
inline void clearFName()  {UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winFName.x,  _winFName.y,  _winFName.w,  _winFName.h,  TFT_BLACK);}
inline void clearTitle()  {UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winTitle.x,  _winTitle.y,  _winTitle.w,  _winTitle.h,  TFT_BLACK);}
inline void clearFooter() {UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winFooter.x, _winFooter.y, _winFooter.w, _winFooter.h, TFT_BLACK);}
inline void clearTime()   {UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winTime.x,   _winTime.y,   _winTime.w,   _winTime.h,   TFT_BLACK);}
inline void clearItem()   {UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winItem.x,   _winItem.y,   _winItem.w,   _winTime.h,   TFT_BLACK);}
inline void clearVolume() {UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winVolume.x, _winVolume.y, _winVolume.w, _winVolume.h, TFT_BLACK);}
inline void clearIPaddr() {UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winIPaddr.x, _winIPaddr.y, _winIPaddr.w, _winIPaddr.h, TFT_BLACK);}
inline void clearStaNr()  {UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winStaNr.x,  _winStaNr.y,  _winStaNr.w,  _winStaNr.h,  TFT_BLACK);}
inline void clearSleep()  {UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winSleep.x,  _winSleep.y,  _winSleep.w,  _winSleep.h,  TFT_BLACK);}
inline void clearVolBar() {UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winVolBar.x, _winVolBar.y, _winVolBar.w, _winVolBar.h, TFT_BLACK);}
inline void clearMid()    {UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winFName.x, _winFName.y, _winFName.w, 260, TFT_BLACK);}
inline void clearAll()    {UDP.printf("tft.fillScreen(%d)\n",0);}                      // y   0...239

inline uint16_t txtlen(String str) {uint16_t len=0; for(int i=0; i<str.length(); i++) if(str[i]<=0xC2) len++; return len;}
void showHeadlineVolume(uint8_t vol){
    clearVolume();
    sprintf(_chbuf, "Vol %02d", vol);
    if(fontLoaded!=AA_FONT_SMALL)
    {   
        UDP.printf("tft.loadFont(%d)\n",AA_FONT_SMALL);
        fontLoaded=AA_FONT_SMALL;
    }
    UDP.printf("tft.setTextColor(%d,%d)\n",TFT_DEEPSKYBLUE, TFT_BLACK);
    UDP.printf("tft.drawString(%s,%d,%d)\n",_chbuf,_winVolume.x + 6, _winVolume.y + AA_FONT_SMALL + 2);
    UDP.endPacket();    //close
    vTaskDelay(50);
    UDP.beginPacket(Display, UDP_port); //open
}
void showHeadlineTime(){

}
void showHeadlineItem(uint8_t idx){
    if(fontLoaded!=AA_FONT_SMALL)
    {   
        UDP.printf("tft.loadFont(%d)\n",AA_FONT_SMALL);
        fontLoaded=AA_FONT_SMALL;
    }
    UDP.printf("tft.setTextColor(%d,%d)\n",TFT_WHITE, TFT_BLACK);
    clearItem();
    UDP.printf("tft.drawString(%s,%d,%d)\n",_hl_item[idx],_winItem.x , _winItem.y + AA_FONT_SMALL + 2);
    UDP.endPacket();
    delay(100);
    UDP.beginPacket(Display, UDP_port);
}
void showFooterIPaddr(){
    char myIP[30] = "myIP:";
    strcpy(myIP + 5, _myIP);
    if(fontLoaded!=AA_FONT_SMALL)
    {   
        UDP.printf("tft.loadFont(%d)\n",AA_FONT_SMALL);
        fontLoaded=AA_FONT_SMALL;
    }
    UDP.printf("tft.setTextColor(%d,%d)\n",TFT_GREENYELLOW, TFT_BLACK);
    clearIPaddr();
    UDP.printf("tft.drawString(%s,%d,%d)\n",myIP,_winIPaddr.x + 26 , _winIPaddr.y + AA_FONT_SMALL + 2);

}
void showFooterStaNr(){
    clearStaNr();
    if(fontLoaded!=AA_FONT_SMALL)
    {   
        UDP.printf("tft.loadFont(%d)\n",AA_FONT_SMALL);
        fontLoaded=AA_FONT_SMALL;
    }
    UDP.printf("tft.setTextColor(%d,%d)\n",TFT_LAVENDER, TFT_BLACK);
    //sprintf(_chbuf, "STA: %03d", _cur_station);
    UDP.printf("tft.drawString(STA: %03d,%d,%d)\n", _cur_station, _winStaNr.x, _winStaNr.y + AA_FONT_SMALL + 2);
}
void updateSleepTime(boolean noDecrement){  // decrement and show new value in footer
    if(_f_sleeping) return;
    boolean sleep = false;
    if(_sleeptime == 1) sleep = true;
    if(_sleeptime > 0 && !noDecrement) _sleeptime--;

    if(_state != ALARM){
        char Slt[15];
        sprintf(Slt,"S  %d:%02d", _sleeptime / 60, _sleeptime % 60);
        if(fontLoaded!=AA_FONT_SMALL)
        {   
            UDP.printf("tft.loadFont(%d)\n",AA_FONT_SMALL);
            fontLoaded=AA_FONT_SMALL;
        }
        if(!_sleeptime) UDP.printf("tft.setTextColor(%d,%d)\n",TFT_DEEPSKYBLUE, TFT_BLACK);
        else UDP.printf("tft.setTextColor(%d,%d)\n",TFT_RED, TFT_BLACK);
        clearSleep();
        UDP.printf("tft.drawString(%s,%d,%d)\n",Slt, _winSleep.x + 25 , _winSleep.y +AA_FONT_SMALL + 2);
    }
    if(sleep){ // fall asleep
        stopSong();
        clearAll();
        UDP.printf("setTFTbrightness(%d)\n",0);
        _f_sleeping = true;
        SerialPrintfln("falling asleep");
    }
}
void showVolumeBar(){   //Mostly visible and touch
    uint16_t vol = 480 * _cur_volume/21;
    UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winVolBar.x, _winVolBar.y, vol, 8, TFT_RED);
    UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",vol+1, _winVolBar.y, 480-vol+1, 8, TFT_GREEN);
    UDP.endPacket();    //close
    vTaskDelay(50);
    UDP.beginPacket(Display, UDP_port); //open
}
void showFooter(){  // stationnumber, sleeptime, IPaddress
    showFooterStaNr();
    updateSleepTime();
    UDP.endPacket();    //close
    vTaskDelay(50);
    UDP.beginPacket(Display, UDP_port); //open
    showFooterIPaddr();
}
void display_info(const char *str, int xPos, int yPos, uint16_t color, uint16_t indent, uint16_t winHeight){
    UDP.beginPacket(Display, UDP_port); //open
    UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",xPos, yPos, 480 - xPos, winHeight, TFT_BLACK);  // Clear the space for new info
    UDP.printf("tft.setTextColor(%d,%d)\n",color, TFT_BLACK);                                // Set the requested color
    UDP.printf("tft.setCursor(%d,%d)\n",xPos + indent, yPos);                            // Prepare to show the info
    UDP.printf("tft.print(%s)\n",str);
    UDP.endPacket();    //close
}
void showStreamTitle(){
    UDP.endPacket();    //close
    vTaskDelay(50);
    UDP.beginPacket(Display, UDP_port); //open
    String ST = _streamTitle;
    Serial.println("streamtitle=" + ST);
    String ST2="";
    ST.trim();  // remove all leading or trailing whitespaces
    webSrv.send("streamtitle=" + ST);
    ST.replace(" | ", "\n");   // some stations use pipe as \n or
    ST.replace("| ", "\n");    // or
    ST.replace("|", "\n");
    ST.replace(", ", " ");  //remove comma for Remote display
    Serial.println("adjusted streamtitle=" + ST);
    if (ST.indexOf( " - ")>1){
        ST2=ST.substring(ST.indexOf( " - ")+3); //usualy the song
        ST=ST.substring(0, ST.indexOf( " - ")); //usualy the artist
        Serial.println("artist=" + ST);
        Serial.println("song=" + ST2);
    }
    if(ST.length()>50) {ST=ST.substring(0,49);}if(ST2.length()>50) {ST2=ST2.substring(0,49);}
    int font;
    if (ST.length()<=18 || ST2.length()<=18) {font=AA_FONT_LARGE;}
    if (ST.length()>18 || ST2.length()>18) {font=AA_FONT_NORMAL;}
    if (ST.length()>27 || ST2.length()>27) {font=AA_FONT_SMALL;}
    if(fontLoaded!=font)
    {   
        UDP.printf("tft.loadFont(%d)\n",font);
        fontLoaded=font;
    }
    UDP.printf("tft.setTextColor(%d,%d)\n",TFT_HOTPINK, TFT_BLACK);
    UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winTitle.x, _winTitle.y, 480 - _winTitle.x, _winTitle.h, TFT_BLACK);  // Clear the space for new info
    if (ST2==""){
        UDP.printf("tft.drawString(%s,%d,%d)\n", ST.c_str(), 0, _winTitle.y + 45 + fontLoaded);
    } else{
        UDP.printf("tft.drawString(%s,%d,%d)\n", ST.c_str(), 0, _winTitle.y + 20 + fontLoaded);
        UDP.printf("tft.setTextColor(%d,%d)\n",TFT_CYAN, TFT_BLACK);
        UDP.printf("tft.drawString(%s,%d,%d)\n", ST2.c_str(), 0, _winTitle.y + 20 + fontLoaded + fontLoaded);
    }
    UDP.endPacket();    //close
    vTaskDelay(50);
    UDP.beginPacket(Display, UDP_port); //open
}
void showLogoAndStationName()
{
    String  SN_utf8 = "";
    String  SN_ascii = "";
    String SN="";
    String SN2="";
    if(_cur_station){
        SN_utf8  = _stationName_nvs;
        SN_ascii = _stationName_nvs;
    }
    else{
        SN_utf8  = _stationName_air;
        SN_ascii = _stationName_air;
    }
    int16_t idx = SN_ascii.indexOf('|');
    if(idx>0){
        SN_ascii = SN_ascii.substring(idx + 1); // before pipe
        SN_utf8 = SN_utf8.substring(0, idx);
    }
    SN_ascii.trim();
    SN_utf8.trim();  
    UDP.endPacket();    //close
    vTaskDelay(50);
    UDP.beginPacket(Display, UDP_port); //open  
    if (SN_utf8.indexOf( " - ")>1){
        SN2=SN_utf8.substring(SN_utf8.indexOf( " - ")+3); //2nd part
        SN=SN_utf8.substring(0, SN_utf8.indexOf( " - ")); //1rst part
    } else{     //if no " - ", then split at first space
        SN=SN_utf8.substring(0, SN_utf8.indexOf(" "));    //This is first (or only part)
        if(SN_utf8.indexOf( " ")>=1){    //When there is a space
            SN2=SN_utf8.substring(SN_utf8.indexOf( " ")+1);  //Split at first space, this is second part
        } else{UDP.printf("tft.setCursor(%d,%d)\n",_winName.x, _winName.y+30);}   //When no second line, show a little lower
    }
    int font;
    if (SN.length()<=15 || SN2.length()<=15) {font=AA_FONT_LARGE;}
    if (SN.length()>15 || SN2.length()>15) {font=AA_FONT_NORMAL;}
    if (SN.length()>40 || SN2.length()>40) {font=AA_FONT_SMALL;}
    if(fontLoaded!=font)
    {   
        UDP.printf("tft.loadFont(%d)\n",font);
        fontLoaded=font;
    }
    clearFName();       //clear logo and station Name
    UDP.printf("tft.setTextColor(%d,%d)\n",TFT_WHITE, TFT_BLACK);
    UDP.printf("tft.drawString(%s,%d,%d)\n", SN.c_str(), _winName.x, _winName.y + font + 10);
    UDP.printf("tft.drawString(%s,%d,%d)\n", SN2.c_str(), _winName.x, _winName.y + font + font + 10);
    String logo = "/logo/m/" + String(UTF8toASCII(SN_ascii.c_str())) +".jpg";
    Serial.print("logo="); Serial.println(logo);
    UDP.printf("TJpgDec.drawSdJpg(%d,%d,%s)\n",  0, _winName.y, logo.c_str());
    UDP.endPacket();
    vTaskDelay(50);
    UDP.beginPacket(Display, UDP_port); //open

}
void showFileName(const char* fname){
    String str=String(fname);
    int strlen=str.length();
    if(strlen > 51) {str=str.substring(strlen-51);}    //limit to 1 line, we want last part
    UDP.beginPacket(Display, UDP_port); //open
    if(fontLoaded!=AA_FONT_SMALL)
    {   
        UDP.printf("tft.loadFont(%d)\n",AA_FONT_SMALL);
        fontLoaded=AA_FONT_SMALL;
    }
    
    UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",0, _winTitle.y,   480, 40,   TFT_BLACK);
    UDP.printf("tft.drawString(%s,%d,%d)\n", str.c_str(), _winTitle.x, _winTitle.y + AA_FONT_SMALL + 10);
    UDP.endPacket();    //close
}
void showArtistSongAudioFile(){
    if(_state==PLAYER || _state==PLAYERico){
        UDP.endPacket();
        vTaskDelay(20);
        UDP.beginPacket(Display, UDP_port); //open
        if (Title.length() <= 15 && artsong.length() <= 15) 
        {
            if(fontLoaded!=AA_FONT_LARGE)
            {   
                UDP.printf("tft.loadFont(%d)\n",AA_FONT_LARGE);
                fontLoaded=AA_FONT_LARGE;
            }
        } else if (Title.length() <= 25 && artsong.length() <= 25) {
            if(fontLoaded!=AA_FONT_NORMAL)
            {   
                UDP.printf("tft.loadFont(%d)\n",AA_FONT_NORMAL);
                fontLoaded=AA_FONT_NORMAL;
            }
        } else {
            UDP.printf("tft.loadFont(%d)\n",AA_FONT_SMALL);
            fontLoaded=AA_FONT_SMALL;
        }
        clearFName();
        UDP.printf("tft.setTextColor(%d,%d)\n",TFT_HOTPINK, TFT_BLACK);
        UDP.printf("tft.drawString(%s,%d,%d)\n", artsong.c_str(), 0, _winName.y + fontLoaded);
        UDP.printf("tft.setTextColor(%d,%d)\n",TFT_CYAN, TFT_BLACK);
        UDP.printf("tft.drawString(%s,%d,%d)\n", Title.c_str(), 0, _winName.y + fontLoaded + fontLoaded);
        artsong="";
        UDP.endPacket();
        vTaskDelay(20);
        UDP.beginPacket(Display, UDP_port); //open
    }
}
void display_time(boolean showall){ //show current time on the TFT Display
    
    
    /*static String t, oldt = "";
    static boolean k = false;
    uint8_t  i = 0, yOffset = 0;
    uint16_t x, y, space, imgHeigh, imgWidth_l, imgWidth_s;
    if(TFT_CONTROLLER < 2){
        x = 0;
        y = _winFName.y +33;
        yOffset = 8;
        space = 2;
        imgHeigh = 120;
        imgWidth_s = 24;
        imgWidth_l = 72;
    }
    else{
        x = 11;
        y = _winFName.y + 50;
        yOffset = 0;
        space = 10; // 10px between jpgs
        imgHeigh = 160;
        imgWidth_s = 32;
        imgWidth_l = 96;
    }
    if(showall == true) oldt = "";
    if((_state == CLOCK) || (_state == CLOCKico)){
        t = gettime_s();
        for(i = 0; i < 5; i++) {
            if(t[i] == ':') {
                if(k == false) {k = true; t[i] = 'd';} else{t[i] = 'e'; k = false;}}
            if(t[i] != oldt[i]) {
                if(TFT_CONTROLLER < 2){
                    sprintf(_chbuf,"/digits/%cgn.jpg",t[i]);
                }
                else{
                    sprintf(_chbuf,"/digits/%cgn.jpg",t[i]);
                }
                //log_i("drawImage %s, x=%i, y=%i", _chbuf, x, y);
                if(_state == CLOCKico) drawImage(_chbuf, x, _winFName.y);
                else drawImage(_chbuf, x, y + yOffset);
            }
            if((t[i]=='d')||(t[i]=='e'))x += imgWidth_s + space; else x += imgWidth_l + space;
        }
        oldt=t;
    }*/
}
void display_alarmDays(uint8_t ad, boolean showall){ // Sun ad=0, Mon ad=1, Tue ad=2 ....
    uint8_t i = 0;
    String str="";

    if(showall){
        clearHeader();
    }
    else{
        _alarmdays ^= (1 << ad);     // toggle bit
    }

    for(i=0;i<7;i++){
        str = "/day/" + String(i);
        if(_alarmdays & (1 << i))  str+="_rt_en.bmp";    // l<<i instead pow(2,i)
        else                       str+="_gr_en.bmp";
        drawImage(str.c_str(), _alarmdaysXPos[i], 0);
    }
}
void display_alarmtime(int8_t xy, int8_t ud, boolean showall){
    uint16_t j[4] = {5, 77, 173, 245};
    static int8_t pos, h, m;
    int8_t updatePos = -1, oldPos = -1;
    uint8_t corrY = 0;
    if(TFT_CONTROLLER < 2){
        corrY = 8;
    }
    else {
        corrY = 3;
    }

    if(showall){
        h = _alarmtime / 60;
        m = _alarmtime % 60;
    }

    if(ud == 1){
        if(pos == 0) if(((h / 10) == 1 && (h % 10) < 4) || ((h / 10) == 0))                {h += 10; updatePos = 0;}
        if(pos == 1) if(((h / 10) == 2 && (h % 10) < 3) || ((h / 10) < 2 && (h % 10) < 9)) {h++;     updatePos = 1;}
        if(pos == 2) if((m / 10) < 5) {m += 10; updatePos = 2;}
        if(pos == 3) if((m % 10) < 9) {m++;     updatePos = 3;}
        _alarmtime = h * 60 + m;
    }
    if(ud == -1){
        if(pos == 0) if((h / 10) > 0) {h -= 10; updatePos = 0;}
        if(pos == 1) if((h % 10) > 0) {h--;     updatePos = 1;}
        if(pos == 2) if((m / 10) > 0) {m -= 10; updatePos = 2;}
        if(pos == 3) if((m % 10) > 0) {m--;     updatePos = 3;}
        _alarmtime = h * 60 + m;
    }

    if(xy == 1) {
        oldPos = pos++;
        if(pos == 4)pos = 0;
        updatePos = pos; //pos 0...3 only
    }
    if(xy == -1){
        oldPos = pos--;
        if(pos ==-1) pos = 3;
        updatePos = pos;
    }

    char hhmm[15];
    sprintf(hhmm,"%d%d%d%d", h / 10, h %10, m /10, m %10);
    UDP.endPacket();
    vTaskDelay(50);
    UDP.beginPacket(Display, UDP_port); //open
    if(showall){
        //drawImage("/digits/m/drt.jpg", _alarmtimeXPos[4], _alarmdays_h + corrY);
        UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n", _alarmtimeXPos[4], _alarmdays_h + corrY, "/digits/m/drt.jpg");
    }

    for(uint8_t i = 0; i < 4; i++){
        strcpy(_path, "/digits/m/");
        strncat(_path, (const char*) hhmm + i, 1);
        if(showall){
            if(i == pos) strcat(_path, "or.jpg");   //show orange number
            else         strcat(_path, "rt.jpg");   //show red numbers
            //drawImage(_path, _alarmtimeXPos[i], _alarmdays_h + corrY);
            UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n", _alarmtimeXPos[i], _alarmdays_h + corrY, _path);
        }

        else{
            if(i == updatePos){
                strcat(_path, "or.jpg");
                //drawImage(_path, _alarmtimeXPos[i], _alarmdays_h + corrY);
                UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n", _alarmtimeXPos[i], _alarmdays_h + corrY, _path);
            }
            if(i == oldPos){
                strcat(_path, "rt.jpg");
                //drawImage(_path, _alarmtimeXPos[i], _alarmdays_h + corrY);
                UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n", _alarmtimeXPos[i], _alarmdays_h + corrY, _path);
            }
        }
    }
}
void display_sleeptime(int8_t ud){  // set sleeptimer
    uint8_t xpos[4] = {5,54,71,120};

    if(ud == 1){
        switch(_sleeptime){
            case  0 ...  14:  _sleeptime = (_sleeptime /  5) *  5 +  5; break;
            case 15 ...  59:  _sleeptime = (_sleeptime / 15) * 15 + 15; break;
            case 60 ... 359:  _sleeptime = (_sleeptime / 60) * 60 + 60; break;
            default: _sleeptime = 360; break; // max 6 hours
        }
    }
    if(ud == -1){
        switch(_sleeptime){
            case  1 ...  15:  _sleeptime = ((_sleeptime - 1) /  5) *  5; break;
            case 16 ...  60:  _sleeptime = ((_sleeptime - 1) / 15) * 15; break;
            case 61 ... 360:  _sleeptime = ((_sleeptime - 1) / 60) * 60; break;
            default: _sleeptime = 0; break; // min
        }
    }
    char tmp[10];
    sprintf(tmp, "%d%02d", _sleeptime / 60, _sleeptime % 60);
    char path[128] = "/digits/m/";

    for(uint8_t i = 0; i < 4; i ++){
        strcpy(path, "/digits/m/");
        if(i == 3){
            if(!_sleeptime) strcat(path, "dsgn.jpg");
            else            strcat(path, "dsrt.jpg");
        }
        else{
            strncat(path, (tmp + i), 1);
            if(!_sleeptime) strcat(path, "sgn.jpg");
            else            strcat(path, "srt.jpg");
        }
        //drawImage(path, _sleeptimeXPos[i], 48);
        UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n", _sleeptimeXPos[i], 48, path);
    }
}
boolean drawImage(const char* path, uint16_t posX, uint16_t posY, uint16_t maxWidth , uint16_t maxHeigth){
    Serial.print("path="); Serial.println(path);
    const char* scImg = scaleImage(path);
    Serial.print("scImg="); Serial.println(scImg);
    if(SD.exists("/logo/m/0n 70s.jpg")) {Serial.println("Existsss");}
    if(!SD.exists(scImg)){
        log_e("file \"%s\" not found", scImg);
        return false;
    }
    if(endsWith(scImg, "bmp")){
        Serial.println("BMP!!!");
        return false;   //drawBmp(scImg, posX, posY, maxWidth, maxHeigth);
    }
    if(endsWith(scImg, "jpg")){
        UDP.beginPacket(Display, UDP_port); //open
        UDP.printf("TJpgDec.drawSdJpeg(%d,%d,%s)\n", posX, posY, scImg);
        UDP.endPacket();    //close 
        return true;
    }
    return false; // neither jpg nor bmp
}
void setTFTbrightness(uint8_t duty){ //duty 0...100 (min...max)
    UDP.printf("tft.brightness(%d)\n",duty);
    /*ledcAttachPin(TFT_BL, 1);        //Configure variable led, TFT_BL pin to channel 1
    ledcSetup(1, 12000, 8);          // 12 kHz PWM and 8 bit resolution
    ledcWrite(1, duty * 2.55);*/
}
inline uint32_t getTFTbrightness(){
    return ledcRead(1);
}
inline uint8_t getBrightness(){
    return pref.getUShort("brightness");
}
void showBrightnessBar(){
    uint16_t br = 480 * getBrightness()/100;   //same as volume bar
    //clearVolBar();
    UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",_winVolBar.x, _winVolBar.y, br, 8, TFT_RED);
    UDP.printf("tft.fillRect(%d,%d,%d,%d,%d)\n",br+1, _winVolBar.y, 480-br+1, 8, TFT_GREEN);
}
void setBrightness(uint8_t br)
{
    UDP.beginPacket(Display, UDP_port); //open
    pref.putUShort("brightness", br);
    UDP.printf("tft.brightness(%d)\n",br);
    showBrightnessBar();
    UDP.endPacket();
}
inline uint8_t downBrightness(){
    uint8_t br; br = pref.getUShort("brightness");
    if(br>5) {
        br-=5;
        setBrightness(br);
    } return br;
}
inline uint8_t upBrightness(){
    uint8_t br; br = pref.getUShort("brightness");
    if(br < 100){
        br += 5;
        setBrightness(br);
    }
    return br;
}



/***********************************************************************************************************************
*                                         L I S T A U D I O F I L E                                                    *
***********************************************************************************************************************/
bool setAudioFolder(const char* audioDir){
    Serial.println("setaudiofolder");
    if(audioFile) audioFile.close();  // same as rewind()
    if(!SD_MMC.exists(audioDir)){log_e("%s not exist", audioDir); return false;}
    audioFile = SD_MMC.open(audioDir);
    if(!audioFile.isDirectory()){log_e("%s is not a directory", audioDir); return false;}
    return true;
}
const char* listAudioFile(){
    Serial.println("listAudioFile");
    File file = audioFile.openNextFile();
    if(!file) {
        Serial.println("no more files found");
        audioFile.close();
        return NULL;
    }
    while(file){
        const char* name = file.name();
        if(endsWith(name, ".mp3") || endsWith(name, ".aac") ||
           endsWith(name, ".m4a") || endsWith(name, ".wav") ||
           endsWith(name, ".flac")  ) Serial.println(name); return name;
    }
    return NULL;
}

/***********************************************************************************************************************
*                                         C O N N E C T   TO   W I F I                                                 *
***********************************************************************************************************************/

/***********************************************************************************************************************
*                                                    A U D I O                                                        *
***********************************************************************************************************************/
void connecttohost(const char* host){
    String h= String(host);
    if(h.startsWith("https")) {h.replace("https", "http"); }
    _f_isWebConnected = audioConnecttohost(h.c_str());
    _f_isFSConnected = false;
}
void connecttoFS(const char* filename, uint32_t resumeFilePos){
    Serial.print("connecttoFS...");Serial.println(filename);
    Serial.print("_f_isFSConnected1...");Serial.println(_f_isFSConnected);
    _f_isFSConnected = audioConnecttoFS(filename, resumeFilePos);
    Serial.print("_f_isFSConnected2...");Serial.println(resumeFilePos);
    _f_isWebConnected = false;
    _audiotrack=filename;
    Serial.print("connecttoFS...");Serial.println(_audiotrack);
}
void stopSong(){
    audioStopSong();
    _f_isFSConnected = false;
    _f_isWebConnected = false;
}

/***********************************************************************************************************************
*                                                    S E T U P                                                         *
***********************************************************************************************************************/
void setup(){
    mutex_rtc     = xSemaphoreCreateMutex();
    Serial.begin(115200);
    if(TFT_CONTROLLER < 2)  strcpy(_prefix, "/s");
    else strcpy(_prefix, "/m");
    pref.begin("MiniWebRadio", false);  // instance of preferences for defaults (tone, volume ...)
    stations.begin("Stations", false);  // instance of preferences for stations (name, url ...)
    SerialPrintfln("setup: Init SD card");
    SPI.begin(VS1053_SCK, VS1053_MISO, VS1053_MOSI); //SPI forVS1053 and SD
    Serial.println("setup      : Init SD card");
    if(!SD_MMC.begin("/sdcard", true)){
        Serial.println("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD_MMC.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD_MMC card attached");
        return;
    }

    Serial.print("SD_MMC Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
    defaultsettings();  // first init

    if(TFT_CONTROLLER > 4) log_e("The value in TFT_CONTROLLER is invalid");

    SerialPrintfln("setup: seek for stations.csv");
    File file=SD_MMC.open("/stations.csv");
    if(!file){

        log_e("stations.csv not found");
        while(1){};  // endless loop, MiniWebRadio does not work without stations.csv
    }
    file.close();
    SerialPrintfln("setup: stations.csv found");
    wifi_conn();
    ftpSrv.begin(SD_MMC, FTP_USERNAME, FTP_PASSWORD); //username, password for ftp.
    configTime(0, 0, ntpServer); //set in platformIO.ini
    setenv("TZ", Timezone, 1);
    vTaskDelay(2000); 
    get_time(); //get local time
    SerialPrintfln("setup: init VS1053");
    // Begin listening to UDP port
    UDP.begin(UDP_port);
    Serial.print("Listening on UDP port ");
    Serial.println(UDP_port);
    audioInit();    //run audio to I2C (startup setting)

    if(CLEARLOG){
        File log;
        SD_MMC.remove("/log.txt");
        log = SD_MMC.open("/log.txt", FILE_WRITE); //create a new file
        log.close();
    }

    //ir.begin();  // Init InfraredDecoder

    webSrv.begin(80, 81); // HTTP port, WebSocket port

    ticker.attach(1, timer1sec);
        // some info about the board
    Serial.println("\n\n##################################");
    Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
    Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());
    Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
    Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
    Serial.println("##################################\n\n");
    if(psramInit()){
      Serial.println("\nPSRAM is correctly initialized");
    }else{
      Serial.println("PSRAM not available");
    }
    UDP.beginPacket(Display, UDP_port);
    _sum_stations = stations.getUInt("sumstations", 0);
    SerialPrintfln("Number of saved stations: %d", _sum_stations);
    _cur_station =  pref.getUInt("station", 1);
    SerialPrintfln("current station number: %d", _cur_station);
    _cur_volume = getvolume();

    SerialPrintfln("current volume: %d", _cur_volume);
    _f_mute = pref.getUShort("mute", 0);
    if(_f_mute) {
        SerialPrintfln("volume is muted: %d", _cur_volume);
        audioSetVolume(0);
    }
    else {
        audioSetVolume(_cur_volume);
    }
    _alarmdays = pref.getUShort("alarm_weekday");
    _alarmtime = pref.getUInt("alarm_time");
    _state = RADIO;
    delay(1000);
    UDP.endPacket();
    delay(100);
    UDP.beginPacket(Display, UDP_port);
    UDP.printf("setClock.Analog(0)\n"); //analog clock off
    UDP.printf("setClock.Digital(0)\n"); //Digital clock off
    UDP.printf("setClock.Show_time(1)\n");  //Show_time On
    UDP.endPacket();
    delay(100);
    UDP.beginPacket(Display, UDP_port);
    clearAll(); // Clear screen
    showHeadlineItem(RADIO);
    showHeadlineVolume(_cur_volume);
    UDP.endPacket();
    delay(100);
    UDP.beginPacket(Display, UDP_port);
    setStation(_cur_station);
    if(DECODER == 0) setTone();    // HW Decoder
    else             setI2STone(); // SW Decoder
    showFooter();
    showVolumeBar();
    UDP.printf("tft.setTextWrap(%d,%d)\n", 0, 0);
    UDP.endPacket();

  
    timer = timerBegin(0, 80, true); // start at 0; divider for 80 MHz = 80 so we have 1 MHz timer; count up = true; timers are 64 bits
    timerAttachInterrupt(timer, &onTimer, false);   //edge doesn't work propperly on esp32, so false here
    timerAlarmWrite(timer, 100000, true); // 100000 = writes an alarm, that triggers an interupt, every 0.1 sec with divider 80
    timerAlarmEnable(timer);
        if(LOG){
            File log;
            log = SD_MMC.open("/log.txt", FILE_APPEND);
            String str=gettime_s(); str.concat("\tsetup done\t"); str.concat(_state); str.concat("\t"); str.concat(_audiotrack); str.concat("\t"); str.concat(_stationURL); str.concat("\n");  //_stationURL
            log.print(str);
        }

    
}
/***********************************************************************************************************************
*                                                  C O M M O N                                                         *
***********************************************************************************************************************/
const char* byte_to_binary(int8_t x){
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1){
        strcat(b, ((x & z) == z) ? "1" : "0");
    }
    return b;
}
bool startsWith (const char* base, const char* str) {
    char c;
    while ( (c = *str++) != '\0' )
      if (c != *base++) return false;
    return true;
}
bool endsWith (const char* base, const char* str) {
    int slen = strlen(str) - 1;
    const char *p = base + strlen(base) - 1;
    while(p > base && isspace(*p)) p--;  // rtrim
    p -= slen;
    if (p < base) return false;
    return (strncmp(p, str, slen) == 0);
}
int indexOf (const char* base, const char* str, int startIndex) {
    const char *p = base;
    for (; startIndex > 0; startIndex--)
        if (*p++ == '\0') return -1;
    char* pos = strstr(p, str);
    if (pos == nullptr) return -1;
    return pos - base;
}
boolean strCompare(char* str1, char* str2){
    return strCompare((const char*) str1, str2);
}
boolean strCompare(const char* str1, char* str2){ // returns true if str1 == str2
    if(!str1) return false;
    if(!str2) return false;
    if(strlen(str1) != strlen(str2)) return false;
    boolean f = true;
    uint16_t i = strlen(str1);
    while(i){
        i--;
        if(str1[i] != str2[i]){f = false; break;}
    }
    return f;
}
const char* scaleImage(const char* path){
    if((!endsWith(path, "bmp")) && (!endsWith(path, "jpg"))){ // not a image
        return UTF8toASCII(path);
    }
    static char pathBuff[256];
    memset(pathBuff, 0, sizeof(pathBuff));
    char* pch = strstr(path + 1, "/");
    if(pch){
        strncpy(pathBuff, path, (pch - path));
        if(TFT_CONTROLLER < 2) strcat(pathBuff, _prefix); // small pic,  320x240px
        else                   strcat(pathBuff, _prefix); // medium pic, 480x320px
        strcat(pathBuff, pch);
    }
    else{
        strcpy(pathBuff, "/common");
        if(TFT_CONTROLLER < 2) strcat(pathBuff, _prefix); // small pic,  320x240px
        else                   strcat(pathBuff, _prefix); // medium pic, 480x320px
        strcat(pathBuff, path);
    }
    return UTF8toASCII(pathBuff);
}
inline uint8_t getvolume(){
    return pref.getUShort("volume");
}
inline void setVolume(uint8_t vol){
    pref.putUShort("volume", vol);
    _cur_volume = vol;
    if(_f_mute==false) audioSetVolume(vol);
    showHeadlineVolume(vol);
    if (_state == RADIO || _state == RADIOico || _state == PLAYER){
        showVolumeBar();
    }   
}
uint8_t downvolume(){
    if(_cur_volume == 0) return _cur_volume;
    _cur_volume --;
    setVolume(_cur_volume);
    return _cur_volume;
}
uint8_t upvolume(){
    if(_cur_volume == _max_volume) return _cur_volume;
    _cur_volume++;
    setVolume(_cur_volume);
    return _cur_volume;
}
inline void mute(){
    if(_f_mute==false){_f_mute=true; audioSetVolume(0);  webSrv.send("mute=1");}
    else {_f_mute=false; audioSetVolume(getvolume());  webSrv.send("mute=0");}
    pref.putUShort("mute", _f_mute);
}

void setStation(uint16_t sta){
    //log_i("sta %d, _cur_station %d", sta, _cur_station );
    int vol=_cur_volume;
    setVolume(0);
    if(sta > _sum_stations) sta = _cur_station;
    sprintf (_chbuf, "station_%03d", sta);
    String content = stations.getString(_chbuf);
    //log_i("content %s", content.c_str());
    _stationName_nvs = content.substring(0, content.indexOf("#")); //get stationname
    content = content.substring(content.indexOf("#") + 1, content.length()); //get URL
    content.trim();
    free(_stationURL);
    _stationURL = strdup(content.c_str());
    _homepage = "";
    _icydescription = "";
    if(_state != RADIOico) clearTitle();
    _cur_station = sta;
    if(!_f_isWebConnected) _streamTitle = "";
    showFooterStaNr();
    pref.putUInt("station", sta);
    if(!_f_isWebConnected){
        connecttohost(_stationURL);
    }
    else{
        if(!strCompare(_stationURL, _lastconnectedhost)) connecttohost(_stationURL);
    }
    showLogoAndStationName();
    StationsItems();
    vTaskDelay(500);
    setVolume(vol);
}
void nextStation(){
    if(_cur_station >= _sum_stations) return;
    _cur_station++;
    setStation(_cur_station);
}
void prevStation(){
    if(_cur_station <= 1) return;
    _cur_station--;
    setStation(_cur_station);
}
void StationsItems(){
    webSrv.send("stationNr=" + String(pref.getUInt("station")));
    webSrv.send("stationURL=" + String(_stationURL));
    webSrv.send("stationName=" + _stationName_nvs);
}
void savefile(const char* fileName, uint32_t contentLength){ //save the uploadfile on SD
    char fn[256];

    if(endsWith(fileName, "jpg")){
        strcpy(fn, "/logo");
        strcat(fn, _prefix);
        if(!startsWith(fileName, "/")) strcat(fn, "/");
        strcat(fn, fileName);
        if(webSrv.uploadB64image(SD_MMC, UTF8toASCII(fn), contentLength)){
            SerialPrintfln("save image %s to SD card was successfully", fn);
            webSrv.reply("OK");
            UDP.endPacket();
            vTaskDelay(50);
            UDP.beginPacket(Display, UDP_port); //open
            UDP.printf("FTP.SD()"); //make sure FTP is on SD
            UDP.endPacket();
            vTaskDelay(100);
            char * fn2;
            fn2=const_cast<char *>(UTF8toASCII(fn));
            ftpPut(fn2, fn2, const_cast<char *>("SDFTPpwd"), const_cast<char *>("SDFTP"), 21, const_cast<char *>(Display));
        }
        else {webSrv.reply("failure");}
    }
    else{
        if(!startsWith(fileName, "/")){
            strcpy(fn, "/");
            strcat(fn, fileName);
        }
        else{
            strcpy(fn, fileName);
        }
        if(webSrv.uploadfile(SD_MMC, UTF8toASCII(fn), contentLength)){
            SerialPrintfln("save file %s to SD card was successfully", fn);
            webSrv.reply("OK");
        }
        else webSrv.reply("failure");
        if(strcmp(fn, "/stations.csv") == 0) saveStationsToNVS();
    }
}
String setTone(){ // vs1053
    uint8_t ha =pref.getUShort("toneha");
    uint8_t hf =pref.getUShort("tonehf");
    uint8_t la =pref.getUShort("tonela");
    uint8_t lf =pref.getUShort("tonelf");
    audioSetTone(ha, hf, la, lf);
    sprintf(_chbuf, "toneha=%i\ntonehf=%i\ntonela=%i\ntonelf=%i\n", ha, hf, la, lf);
    String tone = String(_chbuf);
    return tone;
}
String setI2STone(){
    int8_t LP = pref.getShort("toneLP");
    int8_t BP = pref.getShort("toneBP");
    int8_t HP = pref.getShort("toneHP");
    audioSetTone(LP, BP, HP);
    sprintf(_chbuf, "LowPass=%i\nBandPass=%i\nHighPass=%i\n", LP, BP, HP);
    String tone = String(_chbuf);
    return tone;
}
void audiotrack(const char* fileName, uint32_t resumeFilePos){
    char* path = (char*)malloc(strlen(fileName) + 20);
    strcpy(path, "/audiofiles/");
    strcat(path, fileName);
    clearFName();
    showVolumeBar();
    showHeadlineVolume(_cur_volume);
    showFileName(fileName);
    changeState(PLAYER);
    connecttoFS((const char*) path, resumeFilePos);
    if(_f_isFSConnected){
        free(_lastconnectedfile);
        _lastconnectedfile = strdup(fileName);
        _resumeFilePos = 0;
    }
    if(path) free(path);
    String F = fileName;
    
    if(F.endsWith("flac")) {artsong=_lastconnectedfile;}
}
void next_track_SD(int tracknbr)
  {
    char ch;
    String track = "";
    String sstr = "";  //Search string
    bool found = false;
    int trcknbr=0;
    File trcklst = SD_MMC.open("/tracklist.txt", FILE_READ); //open file for reading
    if (nbroftracks<=1){     //get nbr of tracks (only once)
        while (trcklst.available())
        {
            ch = trcklst.read();
            if (ch == '\n'){ //this is what we want
                nbroftracks++;
            }
        } 
        track = "";
    }
    Serial.print("nbr tracks ");Serial.println(nbroftracks);
    trcklst.close();
    if(tracknbr>0){trcknbr=tracknbr; if(trcknbr>nbroftracks) {trcknbr=1;}}
    else{ trcknbr = random(1, nbroftracks+1);}
    Serial.print("Random or fixed trcknbr = ");Serial.println(trcknbr);  //we have the track number

    //get the number and name of that track
    sstr=String(trcknbr)+"\t:";
    Serial.print("Searching for tracknumber ");Serial.println(trcknbr);  // search for /mp3files/tracknumber<space>
    trcklst = SD_MMC.open("/tracklist.txt", FILE_READ); //open file for reading
    while (trcklst.available())
    {
      ch = trcklst.read();
      track += ch;
      if (track == sstr) {found = true; track = "";} //search for the random track, empty track if the random track is found
      while (found)
      {
        ch = trcklst.read();
        if (ch == '\n') //this is what we want
        {
          Serial.print("This is the track ");Serial.println(sstr+track); //do what is usefull here
          _audiotrack=track;
          track = "";     //reset and exit
          found = false;
          trcklst.close();
          return;
        }
        else
        {
          track += ch;    //add chars until new line, while found = true 
        }
      }  
      
      if (ch == '\n') //new line; delete track, while not found
      {
        track = "";
      }
    }
} 
void next_audio_tracknbr_SD(bool prevnext)  //1=next; 0=prev
  {
    char ch;
    String track = "";
    String sstr = "";  //Search string
    int tracknbr=0;
    bool found = false;
    bool find_nbr=false;
    File trcklst = SD_MMC.open("/tracklist.txt", FILE_READ); //open file for reading
    if (nbroftracks<=1){     //get nbr of tracks (only once)
        while (trcklst.available())
        {
            ch = trcklst.read();
            if (ch == '\n'){ //this is what we want
                nbroftracks++;
            }
        } 
        track = "";
    }
    Serial.print("nbr tracks ");Serial.println(nbroftracks);
    trcklst.close();
    //get the number of the track, so we know where to start if not shuffle
    sstr=_audiotrack;    //search for current track number
    //get the number and name of that track
    //sstr=String(trcknbr)+"\t:";
    trcklst = SD_MMC.open("/tracklist.txt", FILE_READ); //open file for reading
    while (trcklst.available())
    {
        ch = trcklst.read();
        if (ch == '\n') //new line'; delete track, while not found
        {
            track = "";
        }
        else if (ch == '\t'){    //clear track after '\t:' start looking for "\t:"
            ch = trcklst.read();    //this should be ":" after \t
            if (ch == ':'){ //yes, now we have '\t:'
                tracknbr=atoi(track.c_str());  //store the number
                track="";   //now we read the track name, so clear
            }     
        }
        else track += ch;
        // now find the path to the current track
        if (track == sstr) {found = true; track = "";} //found current track in list, now find the next track
        while (found)   //found current track, now we need the next number
        {
            if(prevnext){
                ch = trcklst.read();    //after we found it this should be the next char '\n'
                track += ch;
                if (ch == '\n') //this is what we want
                {
                track = "";     //clear and keep reading untill we find the '\t'
                found=false;
                find_nbr=true;
                }
            }
            else{
                track = "";     //clear and keep reading untill we find the '\t'
                found=false;
                find_nbr=true;
                Serial.println(tracknbr);
                if(tracknbr==1) {tracknbr=nbroftracks;   //if 1 the go to last
                } else {tracknbr-=1;}
                next_track_SD(tracknbr);
                return; //done here
            }
        }    
        while(find_nbr){
            ch = trcklst.read();    //after we found it this should be the next char '\n'
            if (ch == '\t'){    //now start looking for "\t:" don't add to track nbr
                ch = trcklst.read();    //this should be ":" after \t
                if (ch == ':'){ //yes, now we have the tracknbr + '\t:' don't add to tracknumber
                    found=false;    //reset for next time
                    tracknbr=atoi(track.c_str());
                    Serial.println(tracknbr);
                    next_track_SD(tracknbr);
                    return; //done here
                }
                 
            }
            track += ch;
            if(track.length()>5){      //3 digits for 999 tracks, larger then 5 must be end of file
                next_track_SD(1);       //then start at track 1
                trcklst.close();
                return;
            }

        }

    }

}
void next_track_needed(bool prevnext){  //1=next; 0=prev
    if (mp3playall)
    {
        if (millis() - previousMillis >= 1000)
        {
            String str = "";
            Serial.println("mp3 ended...");
            if (shuffle_play){
                next_track_SD(0);
            } else {
                next_audio_tracknbr_SD(prevnext);
            }
            Serial.print("_audiotrack in next_track_needed= ");
            Serial.println(_audiotrack);
            connectto = _audiotrack.substring(12); // remove "/audiofiles/" from _audiotrack (it is added in function audiotrack)
            Serial.print("connectto= ");
            Serial.println(connectto);
            audiotrack(connectto.c_str(), 0);
            str = "MP3_data=Playing track: ";
            str.concat(_audiotrack.substring(_audiotrack.lastIndexOf("/") + 1));
            str.concat("\n");
            webSrv.send(str);
            str = _audiotrack.substring(12);
            showFileName(str.c_str());
            previousMillis = millis();
        }
    }
}
void tracklist(File dir, int numTabs) {
    String str;
    File tracklst;
    if(nbroftracks<1){  //only first time after button push
        SD_MMC.remove("/tracklist.txt");
        tracklst = SD_MMC.open("/tracklist.txt", FILE_WRITE); //create a new file
        tracklst.close();
    }
    tracklst = SD_MMC.open("/tracklist.txt", FILE_APPEND);  //Open to add tracks
    while (true) {
        File entry =  dir.openNextFile();
        if (! entry) {      // no more files
            break;
        }
        if (entry.isDirectory()) {  //Directorie encountered
            tracklst.close();
            tracklist(entry, numTabs + 1);  //read the directory
            tracklst = SD_MMC.open("/tracklist.txt", FILE_APPEND);  //Open and continue for the rest of the root dir
        } else {        //Files; files have sizes, directories do not
            nbroftracks++;
            str=String(nbroftracks); str.concat("\t:");str.concat(entry.path());str.concat("\n");
            Serial.print(str);
            tracklst.print(str);
        }
        entry.close();
    }
    str="Tracks added = Number of tracks : "; str.concat(nbroftracks); str.concat("\n"); webSrv.send(str);
    Serial.print(str);
    tracklst.close();
}
bool send_tracks_to_web(void){  //read tracklist and send is to the webpage
    int i=0;
    char ch;
    String track = "";
    String webstring="AudioFileList=";
    File trcklst = SD_MMC.open("/tracklist.txt", FILE_READ); //open file for reading
    while (trcklst.available())
    {
        ch = trcklst.read();
        if (ch == '\n') //new line'; send the track name and delete track
        {
            log_i("%s", track.c_str());
            webstring=webstring+track+";";
            track = "";
            if(i>35){break;}
            i++;
        }
        if (ch == '\t'){    //now start looking for "\t:" don't add to track nbr
                ch = trcklst.read();    //this should be ":" after \t
                if (ch == ':'){ //yes, now we have the tracknbr + '\t:' don't add to tracknumber
                track="";
                }
        }
        else track += ch;
        if(track=="/audiofiles/"){track="";} //clear to have name (or path) after /audiofiles/
    }
    log_i("%s", webstring.c_str());
    webSrv.send((const char*)webstring.c_str());
    return true;
}

/***********************************************************************************************************************
*                                          M E N U E / B U T T O N S                                                   *
***********************************************************************************************************************/
void changeState(int state){
    log_i("changeState, state is: %i", _state);
    if(state == _state) return;  //nothing todo
    UDP.endPacket();
    vTaskDelay(50);
    UDP.beginPacket(Display, UDP_port); //open
    switch(state) {
        case RADIO:{
            clearAll();
            showHeadlineItem(RADIO);
            setStation(_cur_station);
            showHeadlineVolume(_cur_volume);
            UDP.printf("setClock.Show_time(1)\n");
            showLogoAndStationName();
            showVolumeBar();
            showStreamTitle();
            showFooter();
            UDP.endPacket();
            vTaskDelay(50);
            UDP.beginPacket(Display, UDP_port); //open
            switch(clocktype){
                case 1:{UDP.printf("setClock.Show_time(1)\n");break;}  //other clock
                case 2:{UDP.printf("setClock.Analog(0)\n");break;}  //Big sprite clock
                case 3:{UDP.printf("setClock.Digital(0)\n");break;}  //analog
            }
            break;
        }
        case RADIOico:{
            log_i("RADIOico");
            showHeadlineItem(RADIOico);
            _pressBtn[0] = "/Buttons/Button_Mute_Yellow.jpg";         _releaseBtn[0] = _f_mute? "/Buttons/Button_Mute_Red.jpg":"/Buttons/Button_Mute_Green.jpg";
            _pressBtn[1] = "/Buttons/MP3_Yellow.jpg";          _releaseBtn[1] = "/Buttons/MP3_Green.jpg";
            _pressBtn[2] = "/Buttons/BTinYellow.jpg";         _releaseBtn[2] = "/Buttons/BTinGreen.jpg";
            _pressBtn[3] = "/Buttons/Button_Previous_Yellow.jpg";     _releaseBtn[3] = "/Buttons/Button_Previous_Green.jpg";
            _pressBtn[4] = "/Buttons/Button_Next_Yellow.jpg";         _releaseBtn[4] = "/Buttons/Button_Next_Green.jpg";
            _pressBtn[5] = "/Buttons/Clock_Yellow.jpg";        _releaseBtn[5] = "/Buttons/Clock_Green.jpg";
            _pressBtn[6] = "/Buttons/Settings_Yellow.jpg";     _releaseBtn[6] = "/Buttons/Settings_Green.jpg";
            UDP.endPacket();
            vTaskDelay(50);
            UDP.beginPacket(Display, UDP_port); //open
            clearTitle();
            for(int i = 0; i < 7 ; i++)
            {
                UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n",i * _winButton.w, _winButton.y, _releaseBtn[i]);
                if(i==1 || i==4) 
                {
                    UDP.endPacket();
                    vTaskDelay(50);
                    UDP.beginPacket(Display, UDP_port); //open
                }
            }
            break;
        }
        /*case RADIOmenue:{
            log_i("In RADIOmenue");
            showHeadlineItem(RADIOmenue);
            _pressBtn[0] = "/Buttons/MP3_Yellow.jpg";            _releaseBtn[0] = "/Buttons/MP3_Green.jpg";
            _pressBtn[1] = "/Buttons/Clock_Yellow.jpg";          _releaseBtn[1] = "/Buttons/Clock_Green.jpg";
            _pressBtn[2] = "/Buttons/Radio_Yellow.jpg";          _releaseBtn[2] = "/Buttons/Radio_Greenw.jpg";
            _pressBtn[3] = "/Buttons/Button_Sleep_Yellow.jpg";   _releaseBtn[3] = "/Buttons/Button_Sleep_Green.jpg";
            if(TFT_CONTROLLER != 2){
                _pressBtn[4]="/Buttons/Bulb_Yellow.jpg";       _releaseBtn[4]="/Buttons/Bulb_Green.jpg";
            }
            else{
                _pressBtn[4]="/Buttons/Black.jpg";                 _releaseBtn[4]="/Buttons/Black.jpg";
            }
            _pressBtn[5]="/Buttons/Black.jpg";                 _releaseBtn[5]="/Buttons/Black.jpg";
            _pressBtn[6]="/Buttons/Black.jpg";                 _releaseBtn[6]="/Buttons/Black.jpg";
            for(int i = 0; i < 7 ; i++)
            {
                UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n",i * _winButton.w, _winButton.y, _releaseBtn[i]);
                if(i==1 || i==4) 
                {
                    UDP.endPacket();
                    vTaskDelay(50);
                    UDP.beginPacket(Display, UDP_port); //open
                }
            }
            break;
        }*/
        case CLOCK:{
            if(_state == ALARM){
                pref.putUInt("alarm_time", _alarmtime);
                pref.putUShort("alarm_weekday", _alarmdays);
                SerialPrintfln("Alarm set to %2d:%2d on %s\n", _alarmtime / 60, _alarmtime % 60, byte_to_binary(_alarmdays));
                clearHeader();
            }
            _state = CLOCK;
            clearAll();
            UDP.printf("tft.showTime(0)\n");
            UDP.endPacket();
            vTaskDelay(50);
            UDP.beginPacket(Display, UDP_port); //open
            switch(clocktype){
                case 1:{UDP.printf("setClock.Show_time(1)\n");break;}  //other clock
                case 2:{UDP.printf("setClock.Analog(0)\n");UDP.printf("tftfillScreen(0)\n");UDP.printf("setClock.Digital(1)\n");break;}  //Big sprite clock
                case 3:{UDP.printf("setClock.Digital(0)\n");UDP.printf("tftfillScreen(0)\n");UDP.printf("setClock.Analog(1)\n");break;}  //analog
            }
            break;
        }
        case CLOCKico:{
            _state = CLOCKico;
            showHeadlineItem(CLOCKico);
            showHeadlineVolume(_cur_volume);
            clearMid();
            UDP.endPacket();
            vTaskDelay(50);
            UDP.beginPacket(Display, UDP_port); //open
            switch(clocktype){
                case 1:{UDP.printf("setClock.Show_time(0)\n");break;}  //small in top right
                case 2:{UDP.printf("setClock.Digital(0)\n");break;}  //Big sprite clock
                case 3:{UDP.printf("setClock.Analog(0)\n");break;}  //analog
            }
            UDP.printf("setClock.Show_time(1)\n");
            //display_time(true);
            _pressBtn[0] = "/Buttons/Bell_Yellow.jpg";                  _releaseBtn[0] = "/Buttons/Bell_Green.jpg";;
            _pressBtn[1] = "/Buttons/Button_Sleep_Yellow.jpg";          _releaseBtn[1] = "/Buttons/Button_Sleep_Green.jpg";
            _pressBtn[2] = "/Buttons/Radio_Yellow.jpg";                 _releaseBtn[2] = "/Buttons/Radio_Green.jpg";
            _pressBtn[3] = "/Buttons/Button_Mute_Red.jpg";              _releaseBtn[3] = _f_mute? "/Buttons/Button_Mute_Red.jpg":"/Buttons/Button_Mute_Green.jpg";
            _pressBtn[4] = "/Buttons/Button_Volume_Down_Yellow.jpg";   _releaseBtn[4] = "/Buttons/Button_Volume_Down_Green.jpg";
            _pressBtn[5] = "/Buttons/Button_Volume_Up_Yellow.jpg";     _releaseBtn[5] = "/Buttons/Button_Volume_Up_Green.jpg";
            _pressBtn[6] = "/Buttons/Black.jpg";                       _releaseBtn[6] = "/Buttons/Black.jpg"; 
            //for(int i = 0; i < 6 ; i++) {drawImage(_releaseBtn[i], i * _winButton.w, _winButton.y);}
            int s=0;
            for(int i = 0; i < 7 ; i++) 
            {
                UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n",i * _winButton.w, _winButton.y, _releaseBtn[i]);
                if(i==1 || i==4) 
                {
                    UDP.endPacket();
                    vTaskDelay(50);
                    UDP.beginPacket(Display, UDP_port); //open
                }
                }
            break;
        }
        case BRIGHTNESS:{
            showHeadlineItem(BRIGHTNESS);
            _pressBtn[0] = "/Buttons/Button_Previous_Yellow.jpg"; _releaseBtn[0] = "/Buttons/Button_Previous_Green.jpg";
            _pressBtn[1] = "/Buttons/Button_Next_Yellow.jpg";    _releaseBtn[1] = "/Buttons/Button_Next_Green.jpg";
            _pressBtn[2] = "/Buttons/OK_Yellow.jpg";              _releaseBtn[2] = "/Buttons/OK_Green.jpg";
            _pressBtn[3] = "/Buttons/Black.jpg";                  _releaseBtn[3] = "/Buttons/Black.jpg";
            _pressBtn[4] = "/Buttons/Black.jpg";                  _releaseBtn[4] = "/Buttons/Black.jpg";
            _pressBtn[5] = "/Buttons/Black.jpg";                  _releaseBtn[5] = "/Buttons/Black.jpg";
            _pressBtn[6] = "/Buttons/Black.jpg";                  _releaseBtn[6] = "/Buttons/Black.jpg";
            clearMid();
            clearFooter();
            UDP.printf("TJpgDec.drawSdJpg(%d,%d,%s)\n", 0, _winName.y,"/common/Brightness.jpg");
            showBrightnessBar();
            UDP.endPacket();
            vTaskDelay(50);
            UDP.beginPacket(Display, UDP_port); //open
            for(int i = 0; i < 7 ; i++) 
            {
                UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n",i * _winButton.w, _winButton.y, _releaseBtn[i]);
                if(i==2 || i==5) 
                {
                    UDP.endPacket();
                    vTaskDelay(50);
                    UDP.beginPacket(Display, UDP_port); //open
                }
            }
            break;

        }
        case PLAYER:{
            if(_state == RADIO){
                clearFName();
                clearTitle();
            }
            showHeadlineItem(PLAYER);
            _pressBtn[0] = "/Buttons/Button_Mute_Yellow.jpg";       _releaseBtn[0] = _f_mute? "/Buttons/Button_Mute_Red.jpg":"/Buttons/Button_Mute_Green.jpg";
            _pressBtn[1] = "/Buttons/Radio_Yellow.jpg";             _releaseBtn[1] = "/Buttons/Radio_Green.jpg";
            _pressBtn[2] = "/Buttons/BTinYellow.jpg";               _releaseBtn[2] = "/Buttons/BTinGreen.jpg";
            _pressBtn[3] = "/Buttons/Button_Previous_Yellow.jpg";   _releaseBtn[3] = "/Buttons/Button_Previous_Green.jpg";
            _pressBtn[4] = "/Buttons/Button_Next_Yellow.jpg";       _releaseBtn[4] = "/Buttons/Button_Next_Green.jpg";
            _pressBtn[5] = shuffle_play?"/Buttons/Shuffle_Yellow.jpg":"/Buttons/Shuffle_Green.jpg";      _releaseBtn[5] = _pressBtn[5];
            _pressBtn[6] = "/Buttons/OK_Yellow.jpg";                _releaseBtn[6] = "/Buttons/OK_Green.jpg";
            //for(int i = 0; i < 5 ; i++) {drawImage(_releaseBtn[i], i * _winButton.w, _winButton.y);}
            for(int i = 0; i < 7 ; i++) 
            {
                UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n",i * _winButton.w, _winButton.y, _releaseBtn[i]);
                if(i==1 || i==4) 
                {
                    UDP.endPacket();
                    vTaskDelay(50);
                    UDP.beginPacket(Display, UDP_port); //open
                }
            }
            UDP.endPacket();
            vTaskDelay(50);
            UDP.beginPacket(Display, UDP_port); //open
            clearFName();
            showVolumeBar();
            showHeadlineVolume(_cur_volume);
            break;
        }
        case PLAYERico:{
            showHeadlineItem(PLAYERico);
            _pressBtn[0] = "/Buttons/Button_Mute_Red.jpg";          _releaseBtn[0] = _f_mute? "/Buttons/Button_Mute_Red.jpg":"/Buttons/Button_Mute_Green.jpg";
            _pressBtn[1] = "/Buttons/Button_Volume_Down_Yellow.jpg";_releaseBtn[1] = "/Buttons/Button_Volume_Down_Green.jpg";
            _pressBtn[2] = "/Buttons/Button_Volume_Up_Yellow.jpg";  _releaseBtn[2] = "/Buttons/Button_Volume_Up_Green.jpg";
            _pressBtn[3] = "/Buttons/MP3_Yellow.jpg";               _releaseBtn[3]="/Buttons/MP3_Green.jpg";
            _pressBtn[4] = "/Buttons/Radio_Yellow.jpg";             _releaseBtn[4] = "/Buttons/Radio_Green.jpg";
            //for(int i = 0; i < 5 ; i++) {drawImage(_releaseBtn[i], i * _winButton.w, _winButton.y);}
            for(int i = 0; i < 5 ; i++) 
            {
                UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n",i * _winButton.w, _winButton.y, _releaseBtn[i]);
                if(i==1 || i==4) 
                {
                    UDP.endPacket();
                    vTaskDelay(50);
                    UDP.beginPacket(Display, UDP_port); //open
                }
            }
            break;
        }
        case ALARM:{
            _pressBtn[0] = "/Buttons/Button_Previous_Yellow.jpg";   _releaseBtn[0] = "/Buttons/Button_Previous_Green.jpg";;
            _pressBtn[1] = "/Buttons/Button_Next_Yellow.jpg";       _releaseBtn[1] = "/Buttons/Button_Next_Green.jpg";;
            _pressBtn[2] = "/Buttons/Up_Green.jpg.jpg";             _releaseBtn[2] = "/Buttons/Up_Green.jpg";
            _pressBtn[3] = "/Buttons/Down_Green.jpg";               _releaseBtn[3] = "/Buttons/Down_Green.jpg";
            _pressBtn[4] = "/Buttons/Button_Ready_Yellow.jpg";      _releaseBtn[4] = "/Buttons/Button_Ready_Green.jpg";
            clearAll();
            display_alarmtime(0, 0, true);
            display_alarmDays(0, true);
            //for(int i = 0; i < 5 ; i++) {drawImage(_releaseBtn[i], i * _winButton.w,  _dispHeight - _winButton.h);}
            for(int i = 0; i < 5 ; i++) 
            {
                UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n",i * _winButton.w, _winButton.y, _releaseBtn[i]);
                if(i==1 || i==4) 
                {
                    UDP.endPacket();
                    vTaskDelay(50);
                    UDP.beginPacket(Display, UDP_port); //open
                }
            }
            break;
        }
        case SLEEP:{
            showHeadlineItem(SLEEP);
            _pressBtn[0] = "/Buttons/Up_Green.jpg";                  _releaseBtn[0] = "/Buttons/Up_Green.jpg";
            _pressBtn[1] = "/Buttons/Down_Green.jpg";                _releaseBtn[1] = "/Buttons/Down_Green.jpg";;
            _pressBtn[2] = "/Buttons/OK_Yellow.jpg";                 _releaseBtn[2] = "/Buttons/OK_Green.jpg";
            _pressBtn[3] = "/Buttons/Black.jpg";                     _releaseBtn[3] = "/Buttons/Black.jpg";
            _pressBtn[4] = "/Buttons/Cancel_Yellow.jpg";             _releaseBtn[4] = "/Buttons/Cancel_Green.jpg";
            clearMid();
            display_sleeptime();
            UDP.endPacket();
            vTaskDelay(50);
            UDP.beginPacket(Display, UDP_port); //open
            
            //for(int i = 0; i < 5 ; i++) {drawImage(_releaseBtn[i], i * _winButton.w, _winButton.y);}
            for(int i = 0; i < 5 ; i++) 
            {
                UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n",i * _winButton.w, _winButton.y, _releaseBtn[i]);
                if(i==3) 
                {
                    UDP.endPacket();
                    vTaskDelay(50);
                    UDP.beginPacket(Display, UDP_port); //open
                }
            }

            break;
        }
        case SETTINGS:{
            showHeadlineItem(SETTINGS);
            _state = SETTINGS;
            //input click is change
            _pressBtn[0] = "/Buttons/Radio_Yellow.jpg";             if(input==1) {_releaseBtn[0] = "/Buttons/Radio_Green.jpg";}
                                                                    else if(input==2) {_releaseBtn[0]="/Buttons/MP3_Green.jpg";}
                                                                    else if(input==3) {_releaseBtn[0]="/Buttons/BTinGreen1.jpg";} 
            _pressBtn[1] = "/Buttons/Bell_Yellow.jpg";              _releaseBtn[1]="/Buttons/Bell_Green.jpg";                       
            _pressBtn[2] = "/Buttons/Button_Sleep_Yellow.jpg";      _releaseBtn[2]= "/Buttons/Button_Sleep_Green.jpg";                
            //output, click is change
            _pressBtn[3] = "/Buttons/Speaker_Out_Blue.jpg";         if(output==1) {_releaseBtn[3] = "/Buttons/Speaker_Out_Blue.jpg";}
                                                                    else if(output==2) {_releaseBtn[3]="/Buttons/BT_Out_Blue.jpg";}           
            _pressBtn[4] = "/Buttons/Clock_Yellow.jpg";             _releaseBtn[4]="/Buttons/Clock_Green.jpg";               
            _pressBtn[5] = "/Buttons/Bulb_Yellow.jpg";              _releaseBtn[5]="/Buttons/Bulb_Green.jpg";                     
            _pressBtn[6] = "/Buttons/OK_Yellow.jpg";                _releaseBtn[6]="/Buttons/OK_Green.jpg";                

            clearMid();
            if(fontLoaded!=AA_FONT_SMALL)
            {   
                UDP.printf("tft.loadFont(%d)\n",AA_FONT_SMALL);
                fontLoaded=AA_FONT_SMALL;
            }
            UDP.printf("tft.setTextColor(%d,%d)\n",TFT_WHITE, TFT_BLACK);
            UDP.printf("tft.setCursor(%d,%d)\n", 0, 40);
            UDP.endPacket();
            vTaskDelay(50);
            UDP.beginPacket(Display, UDP_port); //open
            UDP.printf("tft.print(%s)\n", "Input: "); 
            switch(input)
            {
                case 1: {UDP.printf("tft.println(Internet Radio)\n"); break;}
                case 2: {UDP.printf("tft.println(SD-card files)\n"); break;}
                case 3: {UDP.printf("tft.println(BT in)\n"); break;}
                default: {break;}
            }
            UDP.endPacket();
            vTaskDelay(50);
            UDP.beginPacket(Display, UDP_port); //open
            UDP.printf("tft.print(Output: )\n");
            Serial.print("output= "); Serial.println(output);
            switch(output)
            {
                case 1: {UDP.printf("tft.println(Speaker)\n"); break;}
                case 2: {UDP.printf("tft.println(BT out)\n"); break;}
                default: {break;}
            }
            UDP.endPacket();
            vTaskDelay(50);
            UDP.beginPacket(Display, UDP_port); //open
            UDP.printf("tft.print(Clock type: )\n");
            switch(clocktype){
                case 1:{UDP.printf("tft.println(Old digi clock)\n"); break;}
                case 2:{UDP.printf("tft.println(Digital clock/date)\n"); break;}
                case 3:{UDP.printf("tft.println(Analog clock)\n"); break;}
                default: {break;}
            }
            UDP.endPacket();
            vTaskDelay(50);
            UDP.beginPacket(Display, UDP_port); //open
            for(int i = 0; i < 7 ; i++) 
            {
                UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n",i * _winButton.w, _winButton.y, _releaseBtn[i]);
                if(i==1 || i==4) 
                {
                    UDP.endPacket();
                    vTaskDelay(50);
                    UDP.beginPacket(Display, UDP_port); //open
                }
            }
            break;
        }
    }
    _state = state;
    UDP.endPacket();
}
/***********************************************************************************************************************
*                                                      L O O P                                                         *
***********************************************************************************************************************/
void DigiClock() //date and time, ip and strength
{
    //int xpos = tft.width() / 2; // Half the screen width
    int ypos = 40;
    //spr.setTextDatum(C_BASELINE);                                               //using spr (=sprite) there is no need for a black rectangle, to wipe out previous. 
    if (timeinfo.tm_mday > previous_day || previous_day==100){    //date change only once per day{                                        //Now there is no interuption in display
      //spr.setTextColor(TFT_YELLOW, TFT_BLACK);                                  //But fonts need to be in a .h file, SPIFFS will not do
      //spr.loadFont(AA_FONT_num50);                                              //So there are only 3 and only numbers to keep it small
      sprintf(timc, " %02d-%02d-%04d ", timeinfo.tm_mday,timeinfo.tm_mon+1,timeinfo.tm_year+1900);
      Serial.println(timc);
      //sprintf(buf,"%02d-%02d-%02d",  timeinfo.tm_mday, timeinfo.tm_mon, timeinfo.tm_year); 
      //tft.setCursor(xpos-((spr.textWidth(timc))/2), ypos);                      //cursor (reference) is set for tft, not spr
      //spr.printToSprite(timc);
      previous_day=timeinfo.tm_mday;
    }
  
    ypos += 100;  // move ypos down
    if (timeinfo.tm_sec == 0 || previous_sec==100){  //previous_sec=100 at start
      //sprintf(buf,"%02d:%02d",  timeinfo.tm_hour, timeinfo.tm_min);
      strftime(timc, 12, " %H:%M ", &timeinfo); 
      /*spr.setTextColor(TFT_WHITE, TFT_BLACK);
      spr.loadFont(AA_FONT_num100);
      tft.setCursor(xpos - 45 - ((spr.textWidth(timc))/2), ypos);
      spr.printToSprite(timc);    // Prints to tft cursor position, tft cursor NOT moved
      spr.loadFont(AA_FONT_SMALL);
      spr.setTextColor(TFT_DEEPSKYBLUE, TFT_BLACK);
      tft.setCursor(10, 300);
      spr.printToSprite(_myIP);//_myIP
      tft.setCursor(430, 300);
      itoa(WiFi.RSSI(), timc, 10);
      Serial.println(timc);
      spr.printToSprite(timc);*/
    }
    
    //spr.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    //spr.loadFont(AA_FONT_num50);
    //tft.setCursor(xpos+90, ypos+40);
    sprintf(timc, ":%02d ", timeinfo.tm_sec);
    previous_sec=timeinfo.tm_sec;
    //spr.printToSprite(timc);
    //spr.unloadFont();
  
}

void timer_stuff(void)
{
    if (interruptCounter > 0) //run every 0.1sec
    {
        portENTER_CRITICAL(&timerMux); //portenter and exit are needed as to block it for other processes
        interruptCounter--;
        portEXIT_CRITICAL(&timerMux);
        totalcounter++;
        mediumcounter++;
        shortcounter++;     
    }
    /*if(!but_done){  //500msec for a button OK?
        switch (shortcounter){
            case 1: {if(digitalRead(REC)==LOW){REC_but(); but_done=true;} if(digitalRead(MODE)==LOW){MODE_but(); but_done=true;}} 
            case 2: {if(touchRead(PLAY)<20){PLAY_but(); but_done=true;} if(touchRead(SET)<20){SET_but(); but_done=true;}volbut_done=false;}
            case 3: {if(touchRead(VOLUP)<20){VOLUP_but(); but_done=true;} if(touchRead(VOLDWN)<20){VOLDWN_but(); but_done=true;}} 
            case 5: {but_done=false;}
            case 6: {if(digitalRead(REC)==LOW){REC_but(); but_done=true;} if(digitalRead(MODE)==LOW){MODE_but(); but_done=true;}} 
            case 7: {if(touchRead(PLAY)<20){PLAY_but(); but_done=true;} if(touchRead(SET)<20){SET_but(); but_done=true;}volbut_done=false;}
            case 8: {if(touchRead(VOLUP)<20){VOLUP_but(); but_done=true;} if(touchRead(VOLDWN)<20){VOLDWN_but(); but_done=true;}} 

            default: {break;}
        }
    }*/



    if (shortcounter >=10) {    //1 sec
        shortcounter=0;
        but_done=false;
        time(&now);
	    localtime_r(&now, &timeinfo);
        static uint8_t sec=0;
        /*if(_touchCnt){
            _touchCnt--;
            if(!_touchCnt){
                if(_state == RADIOico)   changeState(RADIO);
                if(_state == RADIOmenue) changeState(RADIO);
                if(_state == CLOCKico)   changeState(CLOCK);
            }
        }*/
        if(_f_rtc==true){ // true -> rtc has the current time
            int8_t h=0;
            String time_s;
            xSemaphoreTake(mutex_rtc, portMAX_DELAY);
            time_s = gettime_s();
            xSemaphoreGive(mutex_rtc);
            //if(!BTTask_runs && !audioTask_runs){audioInit();}
            //if(_state != ALARM && !_f_sleeping) UDP.printf("tft.showTime(1)\n")
            if(_state == CLOCK || _state == CLOCKico) display_time();
            if(_f_eof && (_state == RADIO || _f_eof_alarm)){
                _f_eof = false;
                if(_f_eof_alarm){
                    _f_eof_alarm = false;
                    if(_f_mute){
                        mute(); // mute off
                    }
                }
                connecttohost(_lastconnectedhost);
            }
            if(_state == PLAYER){
                if(_f_eof && _state == PLAYER){
                    _f_eof = false;
                    next_track_needed(true);
                }
                webSrv.send("playing_now="+artsong);
            }
            if((_f_mute==false)&&(!_f_sleeping)){
                if(time_s.endsWith("59:53") && _state == RADIO) { // speech the time 7 sec before a new hour is arrived
                    String hour = time_s.substring(0,2); // extract the hour
                    h = hour.toInt();
                    h++;
                    if( h== 24) h=0;
                    sprintf (_chbuf, "/voice_time/%d_00.mp3", h);
                    connecttoFS(_chbuf);
                }
            }
            if(_alarmtime == getMinuteOfTheDay()){ //is alarmtime?
                //log_i("is alarmtime");
                if((_alarmdays>>getweekday())&1){ //is alarmday?
                    if(!_f_semaphore) {_f_alarm = true;  _f_semaphore = true;} //set alarmflag
                }
            }
            else _f_semaphore=false;
            if(_f_alarm){
                SerialPrintfln("Alarm");
                _f_alarm=false;
                connecttoFS("/ring/alarm_clock.mp3");
                audioSetVolume(21);
            }
        }
        if(_commercial_dur > 0){
            _commercial_dur--;
            //if((_commercial_dur == 2) && (_state == RADIO)) clearTitle();// end of commercial? clear streamtitle
        }
        if(_f_newIcyDescription){
            _f_newIcyDescription = false;
            webSrv.send("icy_description=" +_icydescription);
        }
        if(!WL_CONNECTED) {
            Serial.printf("WiFi not connected! trying to connect");
            wifi_conn();
        }
    }
    if (mediumcounter > 599) //run every minute
    {
        mediumcounter = 0;  //back to 0 first, it might go higher again during the program
        if(!_f_rtc) {   //check every minute when time is not sync
            xSemaphoreTake(mutex_rtc, portMAX_DELAY);
            get_time();
            xSemaphoreGive(mutex_rtc);
        }
        //if ((_state==CLOCK || _state==CLOCKico) && (clocktype==1)) {if(clocktype==1)display_time(true); else if(clocktype==2) DigiClock();else if(clocktype==3) DigiClock();}
        //updateSleepTime();
        if(LOG){
            File log;
            log = SD_MMC.open("/log.txt", FILE_APPEND);
            String str=gettime_s(); str.concat("\t"); str.concat(_state); str.concat("\t"); str.concat(_audiotrack); str.concat("\t"); str.concat(_stationURL); str.concat("\n"); 
            log.print(str);
        }
    }
    if (totalcounter > 35999) //run every hour
    {
        totalcounter = 0;
        xSemaphoreTake(mutex_rtc, portMAX_DELAY);
        get_time(); //sync time every hour
        xSemaphoreGive(mutex_rtc);

    }
}

void loop() {
    static uint8_t sec=0;
    if(webSrv.loop()) return; // if true: ignore all other for faster response to web
    //ir.loop();
    ftpSrv.handleFTP();
    timer_stuff();
    UDP_Check();
}
/***********************************************************************************************************************
*                                                    E V E N T S                                                       *
***********************************************************************************************************************/
//Events from vs1053_ext library
void vs1053_info(const char *info){
    SerialPrintfln("%s", info);
    if(endsWith(info, "Stream lost")) SerialPrintfln("%s", info);
}
void audio_info(const char *info){
    // SerialPrintfln("%s", info);
    if(startsWith(info, "FLAC")) SerialPrintfln("%s", info);
    if(endsWith(info, "Stream lost")) SerialPrintfln("%s", info);
}
//----------------------------------------------------------------------------------------
void vs1053_showstation(const char *info){
    _stationName_air = info;
    //if(!_cur_station) showLogoAndStationName();
}
void audio_showstation(const char *info){
    _stationName_air = info;
    //if(!_cur_station) showLogoAndStationName();
}
//----------------------------------------------------------------------------------------
void vs1053_showstreamtitle(const char *info){
    if(_f_irNumberSeen) return; // discard streamtitle
    _streamTitle = info;
    if(_state == RADIO) showStreamTitle();
    SerialPrintfln("StreamTitle: %s", info);
    webSrv.send("streamtitle="+_streamTitle);
}
void audio_showstreamtitle(const char *info){
    if(_f_irNumberSeen) return; // discard streamtitle
    _streamTitle = info;
    if(_state == RADIO) showStreamTitle();
    SerialPrintfln("StreamTitle: %s", info);
    webSrv.send("streamtitle="+_streamTitle);
}
//----------------------------------------------------------------------------------------
void vs1053_commercial(const char *info){
    _commercial_dur = atoi(info) / 1000;                // info is the duration of advertising in ms
    _streamTitle = "Advertising: " + (String) _commercial_dur + "s";
    showStreamTitle();
    SerialPrintfln("StreamTitle: %s", info);
    webSrv.send("streamtitle="+_streamTitle);
}
void audio_commercial(const char *info){
    _commercial_dur = atoi(info) / 1000;                // info is the duration of advertising in ms
    _streamTitle = "Advertising: " + (String) _commercial_dur + "s";
    showStreamTitle();
    SerialPrintfln("StreamTitle: %s", info);
    webSrv.send("streamtitle="+_streamTitle);
}
//----------------------------------------------------------------------------------------
void vs1053_eof_mp3(const char *info){                  // end of mp3 file (filename)
    _f_eof = true;
    if(startsWith(info, "alarm")) _f_eof_alarm = true;
    SerialPrintfln("end of file: %s", info);
}
void audio_eof_mp3(const char *info){                  // end of mp3 file (filename)
    _f_eof = true;
    if(startsWith(info, "alarm")) _f_eof_alarm = true;
    SerialPrintfln("end of file: %s", info);
}
//----------------------------------------------------------------------------------------
void vs1053_lasthost(const char *info){                 // really connected URL
    free(_lastconnectedhost);
    _lastconnectedhost = strdup(info);
    SerialPrintfln("lastURL: %s", _lastconnectedhost);
}
void audio_lasthost(const char *info){                 // really connected URL
    free(_lastconnectedhost);
    _lastconnectedhost = strdup(info);
    SerialPrintfln("lastURL: %s", _lastconnectedhost);
}
//----------------------------------------------------------------------------------------
void vs1053_icyurl(const char *info){                   // if the Radio has a homepage, this event is calling
    if(strlen(info) > 5){
        SerialPrintfln("icy-url: %s", info);
        _homepage = String(info);
        if(!_homepage.startsWith("http")) _homepage = "http://" + _homepage;
    }
}
void audio_icyurl(const char *info){                   // if the Radio has a homepage, this event is calling
    if(strlen(info) > 5){
        SerialPrintfln("icy-url: %s", info);
        _homepage = String(info);
        if(!_homepage.startsWith("http")) _homepage = "http://" + _homepage;
    }
}
//----------------------------------------------------------------------------------------
void vs1053_id3data(const char *info){
    SerialPrintfln("id3data: %s", info);
    String i3d=info;
    if (i3d.startsWith("Artist: ")) artsong=(i3d.substring(8).c_str());
    else if (i3d.startsWith("Title: ")) Title=(i3d.substring(7).c_str());
    else return;
    if (!artsong.isEmpty() && !Title.isEmpty()) {
        artsong.concat(" - "); artsong.concat(Title); 
        showArtistSongAudioFile();
        webSrv.send("playing_now="+artsong);
    }  
}
void audio_id3data(const char *info){
    SerialPrintfln("id3data: %s", info);
    String i3d=info;
    if (i3d.startsWith("Artist: "))  artsong=(i3d.substring(8).c_str());
    else if (i3d.startsWith("Title: ")) Title=(i3d.substring(7).c_str());
    else return;
    if (!artsong.isEmpty() && !Title.isEmpty()) {
        showArtistSongAudioFile();
        webSrv.send("playing_now="+artsong);
    }  
}
//----------------------------------------------------------------------------------------
void vs1053_icydescription(const char *info){
    _icydescription = String(info);
    if(_streamTitle.length()==0 && _state == RADIO){
        _streamTitle = String(info);
        showStreamTitle();
        webSrv.send("streamtitle="+_streamTitle);
    }
    if(strlen(info)){
        _f_newIcyDescription = true;
        SerialPrintfln("icy-descr: %s", info);
    }
}
void audio_icydescription(const char *info){
    _icydescription = String(info);
    if(_streamTitle.length()==0 && _state == RADIO){
        _streamTitle = String(info);
        showStreamTitle();
        webSrv.send("streamtitle="+_streamTitle);
    }
    if(strlen(info)){
        _f_newIcyDescription = true;
        SerialPrintfln("icy-descr: %s", info);
    }
}
//----------------------------------------------------------------------------------------
void ftp_debug(const char* info) {
    if(startsWith(info, "File Name")) return;
    SerialPrintfln("ftpsrv: %s", info);
}
//----------------------------------------------------------------------------------------
void RTIME_info(const char *info){
    Serial.printf("rtime_info : %s\n", info);
}

//Events from tft library
void tft_info(const char *info){
    Serial.printf("tft_info   : %s\n", info);
}
// Events from IR Library
void ir_res(uint32_t res){
    _f_irNumberSeen = false;
    if(_state != RADIO) return;
    if(res != 0){
       setStation(res);
    }
    else{
        setStation(_cur_station); // valid between 1 ... 999
    }
    return;
}
// Event from TouchPad
void changeBtn_pressed(uint8_t btnNr){
    if(_state == ALARM || _state == BRIGHTNESS) {UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n",btnNr * _winButton.w, _dispHeight - _winButton.h, _pressBtn[btnNr]);}
    else                {UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n",btnNr * _winButton.w, _winButton.y, _pressBtn[btnNr]);}
}
void changeBtn_released(uint8_t btnNr){
    if(_state == RADIOico || _state == PLAYER){
        if(_f_mute)  _releaseBtn[0] = "/Buttons/Button_Mute_Red.jpg";
        else         _releaseBtn[0] = "/Buttons/Button_Mute_Green.jpg";
        if(_BT_In)   _releaseBtn[2] = "/Buttons/BTInYellow.jpg";
        else         _releaseBtn[2] = "/Buttons/BTInBlue.jpg";

    }
    if(_state == PLAYER){
        if(shuffle_play)  _releaseBtn[5] = "/Buttons/Shuffle_Yellow.jpg";
        else         _releaseBtn[5] = "/Buttons/Shuffle_Green.jpg";
    }
    if(_state == CLOCKico){
        if(_f_mute)  _releaseBtn[3] = "/Buttons/Button_Mute_Red.jpg";
        else         _releaseBtn[3] = "/Buttons/Button_Mute_Green.jpg";
    }
    if(_state == ALARM) {UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n",btnNr * _winButton.w, _dispHeight - _winButton.h, _releaseBtn[btnNr]);}
    else                {UDP.printf("TJpgDec.drawFsJpg(%d,%d,%s)\n",btnNr * _winButton.w, _winButton.y, _releaseBtn[btnNr]);}
    
}
void tp_pressed(uint16_t x, uint16_t y){
    log_i("tp_pressed, state is: %i", _state);
    _touchCnt = TouchCnt;
    enum : int8_t{none = -1, RADIO_1, RADIOico_1, RADIOico_2, RADIOmenue_1, RADIOmenue_2,
                             PLAYER_1, PLAYERico_1, ALARM_1, BRIGHTNESS_1, BRIGHTNESS_2,
                             CLOCK_1, CLOCKico_1, ALARM_2, SLEEP_1, VOLUME_1, SETTINGS_1};
    int8_t yPos    = none;
    int8_t btnNr   = none; // buttonnumber

    if(_f_sleeping) return; // awake in tp_released()

    switch(_state){
        case RADIO:         if(                     y <= _winTitle.y-40)                 {yPos = RADIO_1;}
                            else if(                y <= _winButton.y)                   {yPos = VOLUME_1;}
                            break;
        case RADIOico:      if(                     y <= _winTitle.y-40)                 {yPos = RADIOico_1;}
                            else if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = RADIOico_2;   btnNr = x / _winButton.w;}
                            else if(                y <= _winButton.y)                   {yPos = VOLUME_1;}
                            break;
        /*case RADIOmenue:    if(                     y <= _winTitle.y-40)                 {yPos = RADIOmenue_1;}
                            else if(_winButton.y <= y && y <= _winButton.y + _winButton.h)  {yPos = RADIOmenue_1;    btnNr = x / _winButton.w;}
                            else if(                y <= _winButton.y)                   {yPos = VOLUME_1;}
                            break;*/
        case PLAYER:        if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = PLAYER_1;     btnNr = x / _winButton.w;}
                            else if(                y <= _winButton.y)                   {yPos = VOLUME_1;}
                            break;
        case PLAYERico:     if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = PLAYERico_1;  btnNr = x / _winButton.w;}
                            else if(                y <= _winButton.y)                   {yPos = VOLUME_1;}
                            break;
        case CLOCK:         if(                     y <= _winTitle.y)                 {yPos = CLOCK_1;}
                            break;
        case CLOCKico:      if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = CLOCKico_1; btnNr = x / _winButton.w;}
                            break;
        case ALARM:         if(                     y <= _alarmdays_h)                {yPos = ALARM_1; btnNr = (x - 2) / _alarmdays_w;} //weekdays
                            if(                     y >= _winButton.y + _winFooter.h) {yPos = ALARM_2; btnNr = x / _winButton.w;}
                            break;
        case SLEEP:         if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = SLEEP_1; btnNr = x / _winButton.w;}
                            break;
        case BRIGHTNESS:    if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = BRIGHTNESS_1; btnNr = x / _winButton.w;}
                            else if(                y <= _winButton.y)                {yPos = BRIGHTNESS_2;} 
                            break;
        case SETTINGS:      if(_winButton.y <= y && y <= _winButton.y + _winButton.h) {yPos = SETTINGS_1; btnNr = x / _winButton.w;}
                            break;
        default:            break;
    }
    if(yPos == none) {log_w("Touchpoint not valid x=%d, y=%d", x, y); return;}
    log_i("tp_set, yPos=  %i, btn = %d", yPos, btnNr);
    switch(yPos){
        case RADIO_1:       changeState(RADIOico); break;
        case RADIOico_1:    changeState(RADIO); break;
        //case RADIOmenue_1:    changeState(RADIO); break;
        case CLOCK_1:       changeState(CLOCKico);   break;
        case RADIOico_2:    if(btnNr == 0){_releaseNr =  0; mute();}
                            else if(btnNr == 1){_releaseNr =  1; } // Mute
                            else if(btnNr == 2){_releaseNr =  2; } // station--
                            else if(btnNr == 3){_releaseNr =  3; } // station++  MP3
                            else if(btnNr == 4){_releaseNr =  4; } // MP3  
                            else if(btnNr == 5){_releaseNr =  5; } // Clock
                            else if(btnNr == 6){_releaseNr =  6; } // Settings (RADIOmenue_1) => Sleep; Alarm; clocktype at start; brightness
                            changeBtn_pressed(btnNr); break;
        /*case RADIOmenue_2:  if(btnNr == 0){_releaseNr = 10; stopSong(); listAudioFile();} // AudioPlayer
                            if(btnNr == 1){_releaseNr = 11;} // Clock
                            if(btnNr == 2){_releaseNr = 12;} // Radio
                            if(btnNr == 3){_releaseNr = 13;} // Sleep
                            if(TFT_CONTROLLER != 2){
                            if(btnNr == 4){_releaseNr = 14;} // Brightness
                            }
                            changeBtn_pressed(btnNr); break;*/
        case CLOCKico_1:    if(btnNr == 0){_releaseNr = 20;} // Alarm
                            if(btnNr == 1){_releaseNr = 21;} // Sleep
                            if(btnNr == 2){_releaseNr = 22;} // Radio
                            if(btnNr == 3){_releaseNr = 23; mute();}
                            if(btnNr == 4){_releaseNr = 24; } // Vol-
                            if(btnNr == 5){_releaseNr = 25; } // Vol+
                            changeBtn_pressed(btnNr); break;
        case ALARM_2:       if(btnNr == 0){_releaseNr = 30;} // left
                            if(btnNr == 1){_releaseNr = 31;} // right
                            if(btnNr == 2){_releaseNr = 32;} // up
                            if(btnNr == 3){_releaseNr = 33;} // down
                            if(btnNr == 4){_releaseNr = 34;} // ready (return to CLOCK)
                            changeBtn_pressed(btnNr); break;
        case PLAYER_1:      if(btnNr == 0){_releaseNr = 40; mute();} // Mute
                            if(btnNr == 1){_releaseNr = 41;} // RADIO
                            if(btnNr == 2){_releaseNr = 42;} // BTIn
                            if(btnNr == 3){_releaseNr = 43;} // Previous
                            if(btnNr == 4){_releaseNr = 44;} // Next
                            if(btnNr == 5){_releaseNr = 45;} // Shuffle
                            if(btnNr == 6){_releaseNr = 46;} // OK/Play
                            changeBtn_pressed(btnNr); break;
        case PLAYERico_1:   if(btnNr == 0){_releaseNr = 50; mute();}
                            if(btnNr == 1){_releaseNr = 51; } // Vol-
                            if(btnNr == 2){_releaseNr = 52; } // Vol+
                            if(btnNr == 3){_releaseNr = 53;} // PLAYER
                            if(btnNr == 4){_releaseNr = 54;} // RADIO
                            changeBtn_pressed(btnNr); break;
        case ALARM_1:       if(btnNr == 0){_releaseNr = 60;} // mon
                            if(btnNr == 1){_releaseNr = 61;} // tue
                            if(btnNr == 2){_releaseNr = 62;} // wed
                            if(btnNr == 3){_releaseNr = 63;} // thu
                            if(btnNr == 4){_releaseNr = 64;} // fri
                            if(btnNr == 5){_releaseNr = 65;} // sat
                            if(btnNr == 6){_releaseNr = 66;} // sun
                            break;
        case SLEEP_1:       if(btnNr == 0){_releaseNr = 70;} // sleeptime up
                            if(btnNr == 1){_releaseNr = 71;} // sleeptime down
                            if(btnNr == 2){_releaseNr = 72;} // display_sleeptime(0, true);} // ready, return to RADIO
                            if(btnNr == 3){_releaseNr = 73;} // unused
                            if(btnNr == 4){_releaseNr = 74;} // return to RADIO without saving sleeptime
                            changeBtn_pressed(btnNr); break;
        case BRIGHTNESS_1:  if(btnNr == 0){_releaseNr = 80;} // darker
                            if(btnNr == 1){_releaseNr = 81;} // brighter
                            if(btnNr == 2){_releaseNr = 82;} // okay
                            changeBtn_pressed(btnNr);break;
        case BRIGHTNESS_2:  {uint8_t br=map(x, 0, 480, 5, 100); setBrightness(br); break;}               //set brightness on bar touch
        case SETTINGS_1:    if(btnNr == 0){_releaseNr = 90;} // Radio 21; Player_SD 10; Player network xx; BT_in xx
                            if(btnNr == 1){_releaseNr = 91;} // Output
                            if(btnNr == 2){_releaseNr = 92;} // Brightness
                            if(btnNr == 3){_releaseNr = 93;} // Alarm
                            if(btnNr == 4){_releaseNr = 94;} // Sleep
                            if(btnNr == 5){_releaseNr = 95;}         // spare
                            if(btnNr == 6){_releaseNr = 96;} // return to active input
                            changeBtn_pressed(btnNr); break;
        case VOLUME_1:      UDP.beginPacket(Display, UDP_port); //open
                            uint8_t vol=map(x, 0, 480, 0, 21);setVolume(vol); showVolumeBar(); showHeadlineVolume(_cur_volume); 
                            UDP.endPacket();
                            String str="displaysetvolume="; str.concat(_cur_volume); Serial.println(str); webSrv.send(str); break;
    }
    log_i("tp_pressed is done released= %d", _releaseNr);
    tp_released();
}
void tp_released(){
    log_i("tp_released, state is: %i", _state);
    const char* chptr = NULL;
    char path[256 + 12] = "/audiofiles/";
    uint16_t w = 64, h = 64;
    UDP.beginPacket(Display, UDP_port); //open
    if(_f_sleeping == true){ //awake
        _f_sleeping = false;
        SerialPrintfln("awake");
        UDP.printf("tft.brightness(%d)\n",pref.getUShort("brightness"));
        //setTFTbrightness(pref.getUShort("brightness"));
        changeState(RADIO);
        connecttohost(_lastconnectedhost);
        showLogoAndStationName();
        showFooter();
        showHeadlineItem(RADIO);
        showHeadlineVolume(_cur_volume);
        UDP.endPacket();
        vTaskDelay(50);
        UDP.beginPacket(Display, UDP_port); //open
        return;
    }

    switch(_releaseNr){
        // RADIOico ******************************
        case  0:    changeBtn_released(0); break;                                    // Mute
        case  1:    changeState(PLAYER);                                             // Player SD
                    if(setAudioFolder("/audiofiles")) chptr = listAudioFile();
                    if(chptr) strcpy(_afn, chptr);
                    showFileName(_afn); webSrv.send("StatePlayer"); break;
        //case  2:    if(_BT_In) {a2dp_sink.end(false); _BT_In=false; wifi_conn();} else {a2dp_sink.start("BTESP32MusicPlayer");_BT_In=true;} break;         //BT_in
        case  3:    prevStation(); showFooterStaNr(); changeBtn_released(1); break;  // previousstation
        case  4:    nextStation(); showFooterStaNr(); changeBtn_released(2); break;  // nextstation
        case  5:    changeState(CLOCK); break;  //  
        case  6:    changeState(SETTINGS); break;                                   //  Settings

        //  Will be removed ***  RADIOmenue ******************************
        /*case 10:    changeState(PLAYER); webSrv.send("StatePlayer");
                    if(setAudioFolder("/audiofiles")) chptr = listAudioFile();
                    if(chptr) strcpy(_afn, chptr);
                    showFileName(_afn); break;
        case 11:    changeState(CLOCK); break;
        case 12:    changeState(RADIO); webSrv.send("StateRadio");break;
        case 13:    changeState(SLEEP); break;
        case 14:    changeState(BRIGHTNESS); break;*/

        // CLOCKico ******************************
        case 20:    changeState(ALARM); break;
        case 21:    changeState(SLEEP); break;
        case 22:    changeState(RADIO); webSrv.send("StateRadio");break;
        case 23:    changeBtn_released(3); break; // Mute
        case 24:    changeBtn_released(4); downvolume(); break;
        case 25:    changeBtn_released(5); upvolume();  break;

        // ALARM ******************************
        case 30:    changeBtn_released(0); display_alarmtime(-1 ,  0); break;
        case 31:    changeBtn_released(1); display_alarmtime( 1 ,  0); break;
        case 32:    changeBtn_released(2); display_alarmtime( 0 ,  1); break;
        case 33:    changeBtn_released(3); display_alarmtime( 0 , -1); break;
        case 34:    changeState(CLOCK); break;

        // AUDIOPLAYER ******************************
        case 40:    changeBtn_released(0); break; //Mute
        case 41:    changeState(RADIO); webSrv.send("StateRadio");break;
        case 42:    changeState(CLOCK); break;      //BTin
        case 43:    changeBtn_released(3); // first audiofile
                    if(setAudioFolder("/audiofiles")) chptr = listAudioFile();
                    if(chptr) strcpy(_afn, chptr);
                    showFileName(_afn); break;
        case 44:    changeBtn_released(4); // next audiofile
                    chptr = listAudioFile();
                    if(chptr) strcpy(_afn ,chptr);
                    showFileName(_afn); break;
        case 45:    if(!shuffle_play){shuffle_play=true;webSrv.send("shuffle_play=1");}else{shuffle_play=false;webSrv.send("shuffle_play=0");} changeBtn_released(5);break; 
        case 46:    showVolumeBar(); // ready
                    strcat(path, _afn);
                    connecttoFS((const char*) path);
                    if(_f_isFSConnected){
                        free(_lastconnectedfile);
                        _lastconnectedfile = strdup(path);
                    } break;

        // AUDIOPLAYERico ******************************
        case 50:    changeBtn_released(0); break; // Mute
        case 51:    changeBtn_released(1); downvolume(); showVolumeBar(); showHeadlineVolume(_cur_volume); break; // Vol-
        case 52:    changeBtn_released(2); upvolume();   showVolumeBar(); showHeadlineVolume(_cur_volume); break; // Vol+
        case 53:    changeState(PLAYER); webSrv.send("StatePlayer");showFileName(_afn); break;
        case 54:    changeState(RADIO); webSrv.send("StateRadio"); break;

        // ALARM (weekdays) ******************************
        case 60:    display_alarmDays(0); break;
        case 61:    display_alarmDays(1); break;
        case 62:    display_alarmDays(2); break;
        case 63:    display_alarmDays(3); break;
        case 64:    display_alarmDays(4); break;
        case 65:    display_alarmDays(5); break;
        case 66:    display_alarmDays(6); break;

        // SLEEP ******************************************
        case 70:    display_sleeptime(1);  changeBtn_released(0); break;
        case 71:    display_sleeptime(-1); changeBtn_released(1); break;
        case 72:    updateSleepTime(true);
                    changeBtn_released(2);
                    changeState(RADIO); webSrv.send("StateRadio");break;
        case 73:    changeBtn_released(3); break; // unused
        case 74:    _sleeptime = 0;
                    changeBtn_released(4);
                    changeState(RADIO); webSrv.send("StateRadio");break;

        //BRIGHTNESS ************************************
        case 80:    downBrightness(); changeBtn_released(0); break;
        case 81:    upBrightness();   changeBtn_released(1); break;
        case 82:    changeState(RADIO); webSrv.send("StateRadio");break;

        // SETTINGS ************************************
        case 90:    if(input == 3){input=1;} else {input++;}  _state=BRIGHTNESS; changeState(SETTINGS);break;
        case 91:    changeState(ALARM); break;
        case 92:    changeState(SLEEP); break;
        case 93:    if(output == 2){output=1;}else {output++;} _state=BRIGHTNESS; changeState(SETTINGS);break;
        case 94:    if(clocktype == 3){clocktype=1;}else {clocktype++;}  _state=BRIGHTNESS; changeState(SETTINGS);break;
        case 95:    changeState(BRIGHTNESS); break;
        case 96:    switch(input){          //change buttons
                        case 1: {changeState(RADIO); break;}   //Radio
                        case 2: {changeState(PLAYER); break;}   //Player_SD
                        case 3: { break;}   //BT_In
                    } ; break;
    }
    _releaseNr = -1;
    UDP.endPacket();
}

//Events from websrv
void WEBSRV_onCommand(const String cmd, const String param, const String arg){                       // called from html
    //log_i("HTML_cmd=%s params=%s arg=%s", cmd.c_str(),param.c_str(), arg.c_str());
    String  str;
    if(cmd == "homepage"){          webSrv.send("homepage=" + _homepage); return;}
    if(cmd == "to_listen"){         StationsItems(); return;}// via websocket, return the name and number of the current station
    if(cmd == "gettone"){           if(DECODER) webSrv.reply(setI2STone().c_str()); else webSrv.reply(setTone().c_str()); return;}
    if(cmd == "getmute"){           webSrv.reply(String(int(_f_mute)).c_str()); return;}
    if(cmd == "getstreamtitle"){    webSrv.reply(_streamTitle.c_str());return;}
    if(cmd == "setmute"){           mute();if(_f_mute) webSrv.reply("Mute=1\n");else webSrv.reply("Mute=0\n");return;}
    if(cmd == "toneha"){            pref.putUShort("toneha",(param.toInt()));                             // vs1053 tone
                                    webSrv.reply("Treble Gain set");
                                    setTone(); return;}
    if(cmd == "tonehf"){            pref.putUShort("tonehf",(param.toInt()));                             // vs1053 tone
                                    webSrv.reply("Treble Freq set");
                                    setTone(); return;}
    if(cmd == "tonela"){            pref.putUShort("tonela",(param.toInt()));                             // vs1053 tone
                                    webSrv.reply("Bass Gain set");
                                    setTone(); return;}
    if(cmd == "tonelf"){            pref.putUShort("tonelf",(param.toInt()));                             // vs1053 tone
                                    webSrv.reply("Bass Freq set");
                                    setTone(); return;}
    if(cmd == "LowPass"){           pref.putShort("toneLP", (param.toInt()));                           // audioI2S tone
                                    char lp[25] = "Lowpass set to "; strcat(lp, param.c_str()); strcat(lp, "dB");
                                    webSrv.reply(lp); setI2STone(); return;}
    if(cmd == "BandPass"){          pref.putShort("toneBP", (param.toInt()));                           // audioI2S tone
                                    char bp[25] = "Bandpass set to "; strcat(bp, param.c_str()); strcat(bp, "dB");
                                    webSrv.reply(bp); setI2STone(); return;}
    if(cmd == "HighPass"){          pref.putShort("toneHP", (param.toInt()));                           // audioI2S tone
                                    char hp[25] = "Highpass set to "; strcat(hp, param.c_str()); strcat(hp, "dB");
                                    webSrv.reply(hp); setI2STone(); return;}
    if(cmd == "audiolist"){         send_tracks_to_web(); return;}    //sendAudioList2Web("/audiofiles") //via websocket
    if(cmd == "audiotrack"){        audiotrack(param.c_str()); webSrv.reply("OK\n"); return;}
    if(cmd == "audiotrackall")      {mp3playall=true; _f_eof=true; next_track_needed(true); webSrv.reply("OK\n"); return;}
    if(cmd == "next_track")         {mp3playall=true; _f_eof=true; next_track_needed(true); webSrv.reply("OK\n"); return;}
    if(cmd == "prev_track")         {mp3playall=true; _f_eof=true; next_track_needed(false); webSrv.reply("OK\n"); return;}
    if(cmd == "audiotracknew")      {webSrv.reply("generating new tracklist...\n"); nbroftracks=0;File root = SD_MMC.open("/audiofiles");tracklist(root, 0); return;}
    if(cmd == "shuffle_play")       {if(_state == PLAYER){_releaseNr=45; tp_released(); return;} else {if(shuffle_play) {shuffle_play=false; webSrv.reply("shuffle_play=0\n");}else{webSrv.reply("shuffle_play=1\n");} return;}}
    if(cmd == "getshuffle"){        webSrv.reply(String(int(shuffle_play)).c_str()); return;}
    if(cmd == "uploadfile"){        _filename = param;  return;}
    if(cmd == "upvolume"){          str = "Volume is now " + (String)upvolume(); webSrv.reply(str.c_str()); SerialPrintfln("%s", str.c_str()); return;}
    if(cmd == "downvolume"){        str = "Volume is now " + (String)downvolume(); webSrv.reply(str.c_str()); SerialPrintfln("%s", str.c_str()); return;}
    if(cmd == "SLVolume"){          if (millis() - previousMillis >= 300){int v =param.toInt();pref.putUInt("volume",v); setVolume(v);previousMillis = millis();} return;}  //200ms between values to reduce traffic
    if(cmd == "getvolume"){         str=String(_cur_volume); str.concat("\n");webSrv.reply(str); SerialPrintfln("%s", str.c_str()); return;}
    if(cmd == "prev_station"){      prevStation(); return;}                                             // via websocket
    if(cmd == "next_station"){      nextStation(); return;}                                             // via websocket
    if(cmd == "set_station"){       setStation(param.toInt()); StationsItems(); return;}                // via websocket
    if(cmd == "stationURL"){        connecttohost(param.c_str());webSrv.reply("OK\n"); return;}
    if(cmd == "getnetworks"){       webSrv.reply(WiFi.SSID().c_str()); return;}
    if(cmd == "ping"){              webSrv.send("pong"); return;}
    if(cmd == "index.html"){        webSrv.show(index_html); return;}
    if(cmd == "get_tftSize"){       webSrv.send(_tftSize? "tftSize=m": "tftSize=s"); return;}
    if(cmd == "get_decoder"){       webSrv.send( DECODER? "decoder=s": "decoder=h"); return;}
    if(cmd == "favicon.ico"){       webSrv.streamfile(SD_MMC, "/favicon.ico"); return;}
    if(cmd.startsWith("SD")){       str = cmd.substring(2); webSrv.streamfile(SD_MMC, scaleImage(str.c_str())); return;}
    if(cmd == "change_state"){      changeState(param.toInt()); return;}
    if(cmd == "stop"){              _resumeFilePos = audioStopSong(); webSrv.reply("OK\n"); return;}
    if(cmd == "resumefile"){        if(!_lastconnectedfile) webSrv.reply("nothing to resume\n");
                                    else {audiotrack(_lastconnectedfile, _resumeFilePos); webSrv.reply("OK\n");} return;}
    if(cmd == "artsong"){           webSrv.send("playing_now="+artsong); return;}
    /*if(cmd == "get_alarmdays"){     webSrv.send("alarmdays=" + String(_alarmdays, 10)); return;}

    if(cmd == "set_alarmdays"){     _alarmdays = param.toInt(); pref.putUShort("alarm_weekday", _alarmdays); return;}

    if(cmd == "get_alarmtime"){     webSrv.send("alarmtime=" + String(_alarmtime, 10)); return;}

    if(cmd == "set_alarmtime"){    _alarmtime = param.toInt(); pref.putUInt("alarm_time", _alarmtime); return;}

    if(cmd == "get_timeAnnouncement"){ if(_f_timeAnnouncement) webSrv.send("timeAnnouncement=1");
                                    if(  !_f_timeAnnouncement) webSrv.send("timeAnnouncement=0");
                                    return;}

    if(cmd == "set_timeAnnouncement"){ if(param == "true" ) _f_timeAnnouncement = true;
                                    if(   param == "false") _f_timeAnnouncement = false;
                                    pref.putBool("timeAnnouncing", _f_timeAnnouncement); return;}

    if(cmd == "DLNA_getServer")  {  DLNA_showServer(); return;}
    if(cmd == "DLNA_getContent0"){  _level = 0; DLNA_showContent(param, 0); return;}
    if(cmd == "DLNA_getContent1"){  _level = 1; DLNA_showContent(param, 1); return;} // search for level 1 content
    if(cmd == "DLNA_getContent2"){  _level = 2; DLNA_showContent(param, 2); return;} // search for level 2 content
    if(cmd == "DLNA_getContent3"){  _level = 3; DLNA_showContent(param, 3); return;} // search for level 3 content
    if(cmd == "DLNA_getContent4"){  _level = 4; DLNA_showContent(param, 4); return;} // search for level 4 content
    if(cmd == "DLNA_getContent5"){  _level = 5; DLNA_showContent(param, 5); return;} // search for level 5 content
*/
    if(cmd == "test"){              sprintf(_chbuf, "free heap: %u, Inbuff filled: %u, Inbuff free: %u\n", ESP.getFreeHeap(), audioInbuffFilled(), audioInbuffFree()); webSrv.reply(_chbuf); return;}

    log_e("unknown HTMLcommand %s", cmd.c_str());
}
void WEBSRV_onRequest(const String request, uint32_t contentLength){
    log_i("request %s contentLength %d", request.c_str(), contentLength);
    if(request.startsWith("------")) return;      // uninteresting WebKitFormBoundaryString
    if(request.indexOf("form-data") > 0) return;  // uninteresting Info
    if(request == "fileUpload"){savefile(_filename.c_str(), contentLength);  return;}
    log_e("unknown request: %s",request.c_str());
}
void WEBSRV_onInfo(const char* info){
    // if(startsWith(info, "WebSocket")) return;       // suppress WebSocket client available
    // if(!strcmp("ping", info)) return;               // suppress ping
    // if(!strcmp("to_listen", info)) return;          // suppress to_isten
    // if(startsWith(info, "Command client"))return;   // suppress Command client available
    SerialPrintfln("HTML_info  : %s", info);    // infos for debug
}

